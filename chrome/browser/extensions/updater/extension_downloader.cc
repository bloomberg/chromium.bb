// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/extension_downloader.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_handle.h"
#include "base/metrics/histogram.h"
#include "base/platform_file.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/version.h"
#include "chrome/browser/extensions/updater/safe_manifest_parser.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/url_fetcher.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_status.h"

using base::Time;
using base::TimeDelta;
using content::BrowserThread;

namespace extensions {

const char ExtensionDownloader::kBlacklistAppID[] = "com.google.crx.blacklist";

namespace {

const char kNotFromWebstoreInstallSource[] = "notfromwebstore";
const char kDefaultInstallSource[] = "";

// TODO(skerner): It would be nice to know if the file system failure
// happens when creating a temp file or when writing to it. Knowing this
// will require changes to URLFetcher.
enum FileWriteResult {
  SUCCESS = 0,
  CANT_CREATE_OR_WRITE_TEMP_CRX,
  CANT_READ_CRX_FILE,
  NUM_FILE_WRITE_RESULTS,
};

void RecordFileUpdateHistogram(FileWriteResult file_write_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UMA_HISTOGRAM_ENUMERATION("Extensions.UpdaterWriteCrxAsFile",
                            file_write_result,
                            NUM_FILE_WRITE_RESULTS);
}

void CheckThatCRXIsReadable(const FilePath& crx_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  FileWriteResult file_write_result = SUCCESS;

  // Open the file in the same way
  // SandboxExtensionUnpacker::ValidateSigniture() will.
  ScopedStdioHandle file(file_util::OpenFile(crx_path, "rb"));
  if (!file.get()) {
    LOG(ERROR) << "Can't read CRX file written for update at path "
               << crx_path.value().c_str();
    file_write_result = CANT_READ_CRX_FILE;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RecordFileUpdateHistogram, file_write_result));
}

// Record the result of writing a CRX file. Will be used to understand
// high failure rates of CRX installs in the field.  If |success| is
// true, |crx_path| should be set to the path to the CRX file.
void RecordCRXWriteHistogram(bool success, const FilePath& crx_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!success) {
    // We know there was an error writing the file.
    RecordFileUpdateHistogram(CANT_CREATE_OR_WRITE_TEMP_CRX);
  } else {
    // Test that the file can be read. Based on histograms in
    // SandboxExtensionUnpacker, we know that many CRX files
    // can not be read. Try reading.
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&CheckThatCRXIsReadable, crx_path));
  }
}

}  // namespace

ExtensionDownloader::ExtensionFetch::ExtensionFetch()
    : id(""),
      url(),
      package_hash(""),
      version("") {}

ExtensionDownloader::ExtensionFetch::ExtensionFetch(
    const std::string& id,
    const GURL& url,
    const std::string& package_hash,
    const std::string& version)
    : id(id), url(url), package_hash(package_hash), version(version) {}

ExtensionDownloader::ExtensionFetch::~ExtensionFetch() {}

ExtensionDownloader::ExtensionDownloader(
    ExtensionDownloaderDelegate* delegate,
    net::URLRequestContextGetter* request_context)
    : delegate_(delegate),
      request_context_(request_context),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(delegate_);
  DCHECK(request_context_);
}

ExtensionDownloader::~ExtensionDownloader() {
  for (FetchMap::iterator it = fetches_preparing_.begin();
       it != fetches_preparing_.end(); ++it) {
    STLDeleteElements(&it->second);
  }
  STLDeleteElements(&manifests_pending_);
}

bool ExtensionDownloader::AddExtension(const Extension& extension) {
  // Skip extensions with empty update URLs converted from user
  // scripts.
  if (extension.converted_from_user_script() &&
      extension.update_url().is_empty()) {
    return false;
  }

  // If the extension updates itself from the gallery, ignore any update URL
  // data.  At the moment there is no extra data that an extension can
  // communicate to the the gallery update servers.
  std::string update_url_data;
  if (!extension.UpdatesFromGallery())
    update_url_data = delegate_->GetUpdateUrlData(extension.id());

  return AddExtensionData(extension.id(), *extension.version(),
                          extension.GetType(), extension.update_url(),
                          update_url_data);
}

bool ExtensionDownloader::AddPendingExtension(const std::string& id,
                                              const GURL& update_url) {
  // Use a zero version to ensure that a pending extension will always
  // be updated, and thus installed (assuming all extensions have
  // non-zero versions).
  Version version("0.0.0.0");
  DCHECK(version.IsValid());

  return AddExtensionData(id, version, Extension::TYPE_UNKNOWN, update_url, "");
}

void ExtensionDownloader::StartAllPending() {
  ReportStats();
  url_stats_ = URLStats();

  for (FetchMap::iterator it = fetches_preparing_.begin();
       it != fetches_preparing_.end(); ++it) {
    const std::vector<ManifestFetchData*>& list = it->second;
    for (size_t i = 0; i < list.size(); ++i) {
      StartUpdateCheck(list[i]);
    }
  }
  fetches_preparing_.clear();
}

void ExtensionDownloader::StartBlacklistUpdate(
    const std::string& version,
   const ManifestFetchData::PingData& ping_data) {
  // Note: it is very important that we use the https version of the update
  // url here to avoid DNS hijacking of the blacklist, which is not validated
  // by a public key signature like .crx files are.
  ManifestFetchData* blacklist_fetch =
      new ManifestFetchData(extension_urls::GetWebstoreUpdateUrl(true));
  blacklist_fetch->AddExtension(kBlacklistAppID, version, &ping_data, "",
                                kDefaultInstallSource);
  StartUpdateCheck(blacklist_fetch);
}

bool ExtensionDownloader::AddExtensionData(const std::string& id,
                                           const Version& version,
                                           Extension::Type extension_type,
                                           GURL update_url,
                                           const std::string& update_url_data) {
  // Skip extensions with non-empty invalid update URLs.
  if (!update_url.is_empty() && !update_url.is_valid()) {
    LOG(WARNING) << "Extension " << id << " has invalid update url "
                 << update_url;
    return false;
  }

  // Skip extensions with empty IDs.
  if (id.empty()) {
    LOG(WARNING) << "Found extension with empty ID";
    return false;
  }

  if (update_url.DomainIs("google.com")) {
    url_stats_.google_url_count++;
  } else if (update_url.is_empty()) {
    url_stats_.no_url_count++;
    // Fill in default update URL.
    //
    // TODO(akalin): Figure out if we should use the HTTPS version.
    update_url = extension_urls::GetWebstoreUpdateUrl(false);
  } else {
    url_stats_.other_url_count++;
  }

  switch (extension_type) {
    case Extension::TYPE_THEME:
      ++url_stats_.theme_count;
      break;
    case Extension::TYPE_EXTENSION:
    case Extension::TYPE_USER_SCRIPT:
      ++url_stats_.extension_count;
      break;
    case Extension::TYPE_HOSTED_APP:
    case Extension::TYPE_PACKAGED_APP:
      ++url_stats_.app_count;
      break;
    case Extension::TYPE_UNKNOWN:
    default:
      ++url_stats_.pending_count;
      break;
  }

  std::vector<GURL> update_urls;
  update_urls.push_back(update_url);
  // If UMA is enabled, also add to ManifestFetchData for the
  // webstore update URL.
  if (!extension_urls::IsWebstoreUpdateUrl(update_url) &&
      MetricsServiceHelper::IsMetricsReportingEnabled()) {
    update_urls.push_back(extension_urls::GetWebstoreUpdateUrl(false));
  }

  for (size_t i = 0; i < update_urls.size(); ++i) {
    DCHECK(!update_urls[i].is_empty());
    DCHECK(update_urls[i].is_valid());

    std::string install_source = i == 0 ?
        kDefaultInstallSource : kNotFromWebstoreInstallSource;

    ManifestFetchData::PingData ping_data;
    ManifestFetchData::PingData* optional_ping_data = NULL;
    if (delegate_->GetPingDataForExtension(id, &ping_data))
      optional_ping_data = &ping_data;

    // Find or create a ManifestFetchData to add this extension to.
    ManifestFetchData* fetch = NULL;
    FetchMap::iterator existing_iter = fetches_preparing_.find(update_urls[i]);
    if (existing_iter != fetches_preparing_.end() &&
        !existing_iter->second.empty()) {
      // Try to add to the ManifestFetchData at the end of the list.
      ManifestFetchData* existing_fetch = existing_iter->second.back();
      if (existing_fetch->AddExtension(id, version.GetString(),
                                       optional_ping_data, update_url_data,
                                       install_source)) {
        fetch = existing_fetch;
      }
    }
    if (!fetch) {
      // Otherwise add a new element to the list, if the list doesn't exist or
      // if its last element is already full.
      fetch = new ManifestFetchData(update_urls[i]);
      fetches_preparing_[update_urls[i]].push_back(fetch);
      bool added = fetch->AddExtension(id, version.GetString(),
                                       optional_ping_data,
                                       update_url_data,
                                       install_source);
      DCHECK(added);
    }
  }

  return true;
}

void ExtensionDownloader::ReportStats() const {
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckExtension",
                           url_stats_.extension_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckTheme",
                           url_stats_.theme_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckApp",
                           url_stats_.app_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckPending",
                           url_stats_.pending_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckGoogleUrl",
                           url_stats_.google_url_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckOtherUrl",
                           url_stats_.other_url_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckNoUrl",
                           url_stats_.no_url_count);
}

void ExtensionDownloader::StartUpdateCheck(ManifestFetchData* fetch_data) {
  scoped_ptr<ManifestFetchData> scoped_fetch_data(fetch_data);
  const std::set<std::string>& id_set(fetch_data->extension_ids());

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableBackgroundNetworking)) {
    NotifyExtensionsDownloadFailed(id_set,
                                   ExtensionDownloaderDelegate::DISABLED);
    return;
  }

  std::deque<ManifestFetchData*>::const_iterator i;
  for (i = manifests_pending_.begin(); i != manifests_pending_.end(); i++) {
    if (fetch_data->full_url() == (*i)->full_url()) {
      // This url is already scheduled to be fetched.
      return;
    }
  }

  if (manifest_fetcher_.get() != NULL) {
    if (manifest_fetcher_->GetURL() != fetch_data->full_url()) {
      manifests_pending_.push_back(scoped_fetch_data.release());
    }
  } else {
    UMA_HISTOGRAM_COUNTS("Extensions.UpdateCheckUrlLength",
        fetch_data->full_url().possibly_invalid_spec().length());

    if (VLOG_IS_ON(2)) {
      std::vector<std::string> id_vector(id_set.begin(), id_set.end());
      std::string id_list = JoinString(id_vector, ',');
      VLOG(2) << "Fetching " << fetch_data->full_url() << " for "
              << id_list;
    }

    current_manifest_fetch_.swap(scoped_fetch_data);
    manifest_fetcher_.reset(content::URLFetcher::Create(
        kManifestFetcherId, fetch_data->full_url(), content::URLFetcher::GET,
        this));
    manifest_fetcher_->SetRequestContext(request_context_);
    manifest_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                                    net::LOAD_DO_NOT_SAVE_COOKIES |
                                    net::LOAD_DISABLE_CACHE);
    manifest_fetcher_->Start();
  }
}

void ExtensionDownloader::OnURLFetchComplete(
    const net::URLFetcher* source) {
  VLOG(2) << source->GetResponseCode() << " " << source->GetURL();

  if (source == manifest_fetcher_.get()) {
    std::string data;
    source->GetResponseAsString(&data);
    OnManifestFetchComplete(source->GetURL(),
                            source->GetStatus(),
                            source->GetResponseCode(),
                            data);
  } else if (source == extension_fetcher_.get()) {
    OnCRXFetchComplete(source,
                       source->GetURL(),
                       source->GetStatus(),
                       source->GetResponseCode());
  } else {
    NOTREACHED();
  }
}

void ExtensionDownloader::OnManifestFetchComplete(
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const std::string& data) {
  // We want to try parsing the manifest, and if it indicates updates are
  // available, we want to fire off requests to fetch those updates.
  if (status.status() == net::URLRequestStatus::SUCCESS &&
      (response_code == 200 || (url.SchemeIsFile() && data.length() > 0))) {
    VLOG(2) << "beginning manifest parse for " << url;
    scoped_refptr<SafeManifestParser> safe_parser(
        new SafeManifestParser(
            data,
            current_manifest_fetch_.release(),
            base::Bind(&ExtensionDownloader::HandleManifestResults,
                       weak_ptr_factory_.GetWeakPtr())));
    safe_parser->Start();
  } else {
    // TODO(asargent) Do exponential backoff here. (http://crbug.com/12546).
    VLOG(1) << "Failed to fetch manifest '" << url.possibly_invalid_spec()
            << "' response code:" << response_code;
    NotifyExtensionsDownloadFailed(
        current_manifest_fetch_->extension_ids(),
        ExtensionDownloaderDelegate::MANIFEST_FETCH_FAILED);
  }
  manifest_fetcher_.reset();
  current_manifest_fetch_.reset();

  // If we have any pending manifest requests, fire off the next one.
  if (!manifests_pending_.empty()) {
    ManifestFetchData* manifest_fetch = manifests_pending_.front();
    manifests_pending_.pop_front();
    StartUpdateCheck(manifest_fetch);
  }
}

void ExtensionDownloader::HandleManifestResults(
    const ManifestFetchData& fetch_data,
    const UpdateManifest::Results* results) {
  // Keep a list of extensions that will not be updated, so that the |delegate_|
  // can be notified once we're done here.
  std::set<std::string> not_updated(fetch_data.extension_ids());

  if (!results) {
    NotifyExtensionsDownloadFailed(
        not_updated,
        ExtensionDownloaderDelegate::MANIFEST_INVALID);
    return;
  }

  // Examine the parsed manifest and kick off fetches of any new crx files.
  std::vector<int> updates;
  DetermineUpdates(fetch_data, *results, &updates);
  for (size_t i = 0; i < updates.size(); i++) {
    const UpdateManifest::Result* update = &(results->list.at(updates[i]));
    const std::string& id = update->extension_id;
    not_updated.erase(id);
    if (id != kBlacklistAppID) {
      NotifyUpdateFound(update->extension_id);
    } else {
      // The URL of the blacklist file is returned by the server and we need to
      // be sure that we continue to be able to reliably detect whether a URL
      // references a blacklist file.
      DCHECK(extension_urls::IsBlacklistUpdateUrl(update->crx_url))
          << update->crx_url;
    }
    FetchUpdatedExtension(update->extension_id, update->crx_url,
                          update->package_hash, update->version);
  }

  // If the manifest response included a <daystart> element, we want to save
  // that value for any extensions which had sent a ping in the request.
  if (fetch_data.base_url().DomainIs("google.com") &&
      results->daystart_elapsed_seconds >= 0) {
    Time day_start =
        Time::Now() - TimeDelta::FromSeconds(results->daystart_elapsed_seconds);

    const std::set<std::string>& extension_ids = fetch_data.extension_ids();
    std::set<std::string>::const_iterator i;
    for (i = extension_ids.begin(); i != extension_ids.end(); i++) {
      const std::string& id = *i;
      ExtensionDownloaderDelegate::PingResult& result = ping_results_[id];
      result.did_ping = fetch_data.DidPing(id, ManifestFetchData::ROLLCALL);
      result.day_start = day_start;
    }
  }

  NotifyExtensionsDownloadFailed(
      not_updated,
      ExtensionDownloaderDelegate::NO_UPDATE_AVAILABLE);
}

void ExtensionDownloader::DetermineUpdates(
    const ManifestFetchData& fetch_data,
    const UpdateManifest::Results& possible_updates,
    std::vector<int>* result) {
  // This will only be valid if one of possible_updates specifies
  // browser_min_version.
  Version browser_version;

  for (size_t i = 0; i < possible_updates.list.size(); i++) {
    const UpdateManifest::Result* update = &possible_updates.list[i];
    const std::string& id = update->extension_id;

    if (!fetch_data.Includes(id)) {
      VLOG(2) << "Ignoring " << id << " from this manifest";
      continue;
    }

    if (VLOG_IS_ON(2)) {
      if (update->version.empty())
        VLOG(2) << "manifest indicates " << id << " has no update";
      else
        VLOG(2) << "manifest indicates " << id
                << " latest version is '" << update->version << "'";
    }

    if (!delegate_->IsExtensionPending(id)) {
      // If we're not installing pending extension, and the update
      // version is the same or older than what's already installed,
      // we don't want it.
      std::string version;
      if (!delegate_->GetExtensionExistingVersion(id, &version)) {
        VLOG(2) << id << " is not installed";
        continue;
      }

      VLOG(2) << id << " is at '" << version << "'";

      Version existing_version(version);
      Version update_version(update->version);

      if (!update_version.IsValid() ||
          update_version.CompareTo(existing_version) <= 0) {
        continue;
      }
    }

    // If the update specifies a browser minimum version, do we qualify?
    if (update->browser_min_version.length() > 0) {
      // First determine the browser version if we haven't already.
      if (!browser_version.IsValid()) {
        chrome::VersionInfo version_info;
        if (version_info.is_valid())
          browser_version = Version(version_info.Version());
      }
      Version browser_min_version(update->browser_min_version);
      if (browser_version.IsValid() && browser_min_version.IsValid() &&
          browser_min_version.CompareTo(browser_version) > 0) {
        // TODO(asargent) - We may want this to show up in the extensions UI
        // eventually. (http://crbug.com/12547).
        LOG(WARNING) << "Updated version of extension " << id
                     << " available, but requires chrome version "
                     << update->browser_min_version;
        continue;
      }
    }
    VLOG(2) << "will try to update " << id;
    result->push_back(i);
  }
}

  // Begins (or queues up) download of an updated extension.
void ExtensionDownloader::FetchUpdatedExtension(const std::string& id,
                                                const GURL& url,
                                                const std::string& hash,
                                                const std::string& version) {
  for (std::deque<ExtensionFetch>::const_iterator iter =
           extensions_pending_.begin();
       iter != extensions_pending_.end(); ++iter) {
    if (iter->id == id || iter->url == url) {
      return;  // already scheduled
    }
  }

  if (extension_fetcher_.get() != NULL) {
    if (extension_fetcher_->GetURL() != url) {
      extensions_pending_.push_back(ExtensionFetch(id, url, hash, version));
    }
  } else {
    extension_fetcher_.reset(content::URLFetcher::Create(
        kExtensionFetcherId, url, content::URLFetcher::GET, this));
    extension_fetcher_->SetRequestContext(request_context_);
    extension_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                                     net::LOAD_DO_NOT_SAVE_COOKIES |
                                     net::LOAD_DISABLE_CACHE);
    // Download CRX files to a temp file. The blacklist is small and will be
    // processed in memory, so it is fetched into a string.
    if (id != kBlacklistAppID) {
      extension_fetcher_->SaveResponseToTemporaryFile(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
    }

    VLOG(2) << "Starting fetch of " << url << " for " << id;

    extension_fetcher_->Start();
    current_extension_fetch_ = ExtensionFetch(id, url, hash, version);
  }
}

void ExtensionDownloader::OnCRXFetchComplete(
    const net::URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code) {
  const std::string& id = current_extension_fetch_.id;
  const ExtensionDownloaderDelegate::PingResult& ping = ping_results_[id];

  base::PlatformFileError error_code = base::PLATFORM_FILE_OK;
  if (source->FileErrorOccurred(&error_code)) {
    LOG(ERROR) << "Failed to write update CRX with id " << id << ". "
               << "Error code is "<< error_code;
    RecordCRXWriteHistogram(false, FilePath());
    delegate_->OnExtensionDownloadFailed(
        id, ExtensionDownloaderDelegate::CRX_FETCH_FAILED, ping);
  } else if (status.status() == net::URLRequestStatus::SUCCESS &&
      (response_code == 200 || url.SchemeIsFile())) {
    if (id == kBlacklistAppID) {
      std::string data;
      source->GetResponseAsString(&data);
      // TODO(asargent): try to get rid of this special case for the blacklist
      // to simplify the delegate's interface.
      delegate_->OnBlacklistDownloadFinished(
          data, current_extension_fetch_.package_hash,
          current_extension_fetch_.version, ping);
    } else {
      FilePath crx_path;
      // Take ownership of the file at |crx_path|.
      CHECK(source->GetResponseAsFilePath(true, &crx_path));
      RecordCRXWriteHistogram(true, crx_path);
      delegate_->OnExtensionDownloadFinished(id, crx_path, url,
                                             current_extension_fetch_.version,
                                             ping);
    }
  } else {
    // TODO(asargent) do things like exponential backoff, handling
    // 503 Service Unavailable / Retry-After headers, etc. here.
    // (http://crbug.com/12546).
    VLOG(1) << "Failed to fetch extension '" << url.possibly_invalid_spec()
            << "' response code:" << response_code;
    delegate_->OnExtensionDownloadFailed(
        id, ExtensionDownloaderDelegate::CRX_FETCH_FAILED, ping);
  }

  extension_fetcher_.reset();
  current_extension_fetch_ = ExtensionFetch();
  ping_results_.erase(id);

  // If there are any pending downloads left, start the next one.
  if (!extensions_pending_.empty()) {
    ExtensionFetch next = extensions_pending_.front();
    extensions_pending_.pop_front();
    FetchUpdatedExtension(next.id, next.url, next.package_hash, next.version);
  }
}

void ExtensionDownloader::NotifyExtensionsDownloadFailed(
    const std::set<std::string>& extension_ids,
    ExtensionDownloaderDelegate::Error error) {
  for (std::set<std::string>::const_iterator it = extension_ids.begin();
       it != extension_ids.end(); ++it) {
    delegate_->OnExtensionDownloadFailed(*it, error, ping_results_[*it]);
    ping_results_.erase(*it);
  }
}

void ExtensionDownloader::NotifyUpdateFound(const std::string& id) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_UPDATE_FOUND,
      content::NotificationService::AllBrowserContextsAndSources(),
      content::Details<const std::string>(&id));
}

}  // namespace extensions
