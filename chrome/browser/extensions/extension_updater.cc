// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_updater.h"

#include <algorithm>
#include <set>

#include "base/logging.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/rand_util.h"
#include "base/scoped_vector.h"
#include "base/sha2.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/thread.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/utility_process_host.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_status.h"

using base::RandDouble;
using base::RandInt;
using base::Time;
using base::TimeDelta;
using prefs::kExtensionBlacklistUpdateVersion;
using prefs::kLastExtensionsUpdateCheck;
using prefs::kNextExtensionsUpdateCheck;

// NOTE: HTTPS is used here to ensure the response from omaha can be trusted.
// The response contains a url for fetching the blacklist and a hash value
// for validation.
const char* ExtensionUpdater::kBlacklistUpdateUrl =
    "https://clients2.google.com/service/update2/crx";

// Update AppID for extension blacklist.
const char* ExtensionUpdater::kBlacklistAppID = "com.google.crx.blacklist";

// Wait at least 5 minutes after browser startup before we do any checks. If you
// change this value, make sure to update comments where it is used.
const int kStartupWaitSeconds = 60 * 5;

// For sanity checking on update frequency - enforced in release mode only.
static const int kMinUpdateFrequencySeconds = 30;
static const int kMaxUpdateFrequencySeconds = 60 * 60 * 24 * 7;  // 7 days

// A utility class to do file handling on the file I/O thread.
class ExtensionUpdaterFileHandler
    : public base::RefCountedThreadSafe<ExtensionUpdaterFileHandler> {
 public:
  // Writes crx file data into a tempfile, and calls back the updater.
  void WriteTempFile(const std::string& extension_id, const std::string& data,
                     scoped_refptr<ExtensionUpdater> updater) {
    // Make sure we're running in the right thread.
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

    FilePath path;
    if (!file_util::CreateTemporaryFile(&path)) {
      LOG(WARNING) << "Failed to create temporary file path";
      return;
    }
    if (file_util::WriteFile(path, data.c_str(), data.length()) !=
        static_cast<int>(data.length())) {
      // TODO(asargent) - It would be nice to back off updating alltogether if
      // the disk is full. (http://crbug.com/12763).
      LOG(ERROR) << "Failed to write temporary file";
      file_util::Delete(path, false);
      return;
    }

    // The ExtensionUpdater is now responsible for cleaning up the temp file
    // from disk.
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(
            updater.get(), &ExtensionUpdater::OnCRXFileWritten, extension_id,
            path));
  }

  void DeleteFile(const FilePath& path) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
    if (!file_util::Delete(path, false)) {
      LOG(WARNING) << "Failed to delete temp file " << path.value();
    }
  }

 private:
  friend class base::RefCountedThreadSafe<ExtensionUpdaterFileHandler>;

  ~ExtensionUpdaterFileHandler() {}
};


ExtensionUpdater::ExtensionUpdater(ExtensionUpdateService* service,
                                   PrefService* prefs,
                                   int frequency_seconds)
    : service_(service), frequency_seconds_(frequency_seconds),
      prefs_(prefs), file_handler_(new ExtensionUpdaterFileHandler()),
      blacklist_checks_enabled_(true) {
  Init();
}

void ExtensionUpdater::Init() {
  DCHECK_GE(frequency_seconds_, 5);
  DCHECK(frequency_seconds_ <= kMaxUpdateFrequencySeconds);
#ifdef NDEBUG
  // In Release mode we enforce that update checks don't happen too often.
  frequency_seconds_ = std::max(frequency_seconds_, kMinUpdateFrequencySeconds);
#endif
  frequency_seconds_ = std::min(frequency_seconds_, kMaxUpdateFrequencySeconds);
}

ExtensionUpdater::~ExtensionUpdater() {}

static void EnsureInt64PrefRegistered(PrefService* prefs,
                                      const wchar_t name[]) {
  if (!prefs->IsPrefRegistered(name))
    prefs->RegisterInt64Pref(name, 0);
}

static void EnsureBlacklistVersionPrefRegistered(PrefService* prefs) {
  if (!prefs->IsPrefRegistered(kExtensionBlacklistUpdateVersion))
    prefs->RegisterStringPref(kExtensionBlacklistUpdateVersion, L"0");
}

// The overall goal here is to balance keeping clients up to date while
// avoiding a thundering herd against update servers.
TimeDelta ExtensionUpdater::DetermineFirstCheckDelay() {
  // If someone's testing with a quick frequency, just allow it.
  if (frequency_seconds_ < kStartupWaitSeconds)
    return TimeDelta::FromSeconds(frequency_seconds_);

  // If we've never scheduled a check before, start at frequency_seconds_.
  if (!prefs_->HasPrefPath(kNextExtensionsUpdateCheck))
    return TimeDelta::FromSeconds(frequency_seconds_);

  // If it's been a long time since our last actual check, we want to do one
  // relatively soon.
  Time now = Time::Now();
  Time last = Time::FromInternalValue(prefs_->GetInt64(
      kLastExtensionsUpdateCheck));
  int days = (now - last).InDays();
  if (days >= 30) {
    // Wait 5-10 minutes.
    return TimeDelta::FromSeconds(RandInt(kStartupWaitSeconds,
                                          kStartupWaitSeconds * 2));
  } else if (days >= 14) {
    // Wait 10-20 minutes.
    return TimeDelta::FromSeconds(RandInt(kStartupWaitSeconds * 2,
                                          kStartupWaitSeconds * 4));
  } else if (days >= 3) {
    // Wait 20-40 minutes.
    return TimeDelta::FromSeconds(RandInt(kStartupWaitSeconds * 4,
                                          kStartupWaitSeconds * 8));
  }

  // Read the persisted next check time, and use that if it isn't too soon.
  // Otherwise pick something random.
  Time saved_next = Time::FromInternalValue(prefs_->GetInt64(
      kNextExtensionsUpdateCheck));
  Time earliest = now + TimeDelta::FromSeconds(kStartupWaitSeconds);
  if (saved_next >= earliest) {
    return saved_next - now;
  } else {
    return TimeDelta::FromSeconds(RandInt(kStartupWaitSeconds,
                                          frequency_seconds_));
  }
}

void ExtensionUpdater::Start() {
  // Make sure our prefs are registered, then schedule the first check.
  EnsureInt64PrefRegistered(prefs_, kLastExtensionsUpdateCheck);
  EnsureInt64PrefRegistered(prefs_, kNextExtensionsUpdateCheck);
  EnsureBlacklistVersionPrefRegistered(prefs_);
  ScheduleNextCheck(DetermineFirstCheckDelay());
}

void ExtensionUpdater::Stop() {
  timer_.Stop();
  manifest_fetcher_.reset();
  extension_fetcher_.reset();
  manifests_pending_.clear();
  extensions_pending_.clear();
}

void ExtensionUpdater::OnURLFetchComplete(
    const URLFetcher* source, const GURL& url, const URLRequestStatus& status,
    int response_code, const ResponseCookies& cookies,
    const std::string& data) {
  if (source == manifest_fetcher_.get()) {
    OnManifestFetchComplete(url, status, response_code, data);
  } else if (source == extension_fetcher_.get()) {
    OnCRXFetchComplete(url, status, response_code, data);
  } else {
    NOTREACHED();
  }
}

// Utility class to handle doing xml parsing in a sandboxed utility process.
class SafeManifestParser : public UtilityProcessHost::Client {
 public:
  SafeManifestParser(const std::string& xml, ExtensionUpdater* updater)
      : xml_(xml), updater_(updater) {
  }

  // Posts a task over to the IO loop to start the parsing of xml_ in a
  // utility process.
  void Start() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(
            this, &SafeManifestParser::ParseInSandbox,
            g_browser_process->resource_dispatcher_host()));
  }

  // Creates the sandboxed utility process and tells it to start parsing.
  void ParseInSandbox(ResourceDispatcherHost* rdh) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

    // TODO(asargent) we shouldn't need to do this branch here - instead
    // UtilityProcessHost should handle it for us. (http://crbug.com/19192)
    bool use_utility_process = rdh &&
        !CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess);

#if defined(OS_POSIX)
    // TODO(port): Don't use a utility process on linux (crbug.com/22703) or
    // MacOS (crbug.com/8102) until problems related to autoupdate are fixed.
    use_utility_process = false;
#endif

    if (use_utility_process) {
      UtilityProcessHost* host = new UtilityProcessHost(
          rdh, this, ChromeThread::UI);
      host->StartUpdateManifestParse(xml_);
    } else {
      UpdateManifest manifest;
      if (manifest.Parse(xml_)) {
        ChromeThread::PostTask(
            ChromeThread::UI, FROM_HERE,
            NewRunnableMethod(
                this, &SafeManifestParser::OnParseUpdateManifestSucceeded,
                manifest.results()));
      } else {
        ChromeThread::PostTask(
            ChromeThread::UI, FROM_HERE,
            NewRunnableMethod(
                this, &SafeManifestParser::OnParseUpdateManifestFailed,
                manifest.errors()));
      }
    }
  }

  // Callback from the utility process when parsing succeeded.
  virtual void OnParseUpdateManifestSucceeded(
      const UpdateManifest::ResultList& list) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    updater_->HandleManifestResults(list);
  }

  // Callback from the utility process when parsing failed.
  virtual void OnParseUpdateManifestFailed(const std::string& error_message) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    LOG(WARNING) << "Error parsing update manifest:\n" << error_message;
  }

 private:
  ~SafeManifestParser() {}

  const std::string& xml_;

  scoped_refptr<ExtensionUpdater> updater_;
};


void ExtensionUpdater::OnManifestFetchComplete(const GURL& url,
                                               const URLRequestStatus& status,
                                               int response_code,
                                               const std::string& data) {
  // We want to try parsing the manifest, and if it indicates updates are
  // available, we want to fire off requests to fetch those updates.
  if (status.status() == URLRequestStatus::SUCCESS && response_code == 200) {
    scoped_refptr<SafeManifestParser> safe_parser =
        new SafeManifestParser(data, this);
    safe_parser->Start();
  } else {
    // TODO(asargent) Do exponential backoff here. (http://crbug.com/12546).
    LOG(INFO) << "Failed to fetch manifst '" << url.possibly_invalid_spec() <<
        "' response code:" << response_code;
  }
  manifest_fetcher_.reset();

  // If we have any pending manifest requests, fire off the next one.
  if (!manifests_pending_.empty()) {
    GURL url = manifests_pending_.front();
    manifests_pending_.pop_front();
    StartUpdateCheck(url);
  }
}

void ExtensionUpdater::HandleManifestResults(
    const UpdateManifest::ResultList& results) {
  std::vector<int> updates = DetermineUpdates(results);
  for (size_t i = 0; i < updates.size(); i++) {
    const UpdateManifest::Result* update = &(results.at(updates[i]));
    FetchUpdatedExtension(update->extension_id, update->crx_url,
        update->package_hash, update->version);
  }
}

void ExtensionUpdater::ProcessBlacklist(const std::string& data) {
  // Verify sha256 hash value.
  char sha256_hash_value[base::SHA256_LENGTH];
  base::SHA256HashString(data, sha256_hash_value, base::SHA256_LENGTH);
  std::string hash_in_hex = HexEncode(sha256_hash_value, base::SHA256_LENGTH);

  if (current_extension_fetch_.package_hash != hash_in_hex) {
    NOTREACHED() << "Fetched blacklist checksum is not as expected. "
      << "Expected: " << current_extension_fetch_.package_hash
      << " Actual: " << hash_in_hex;
    return;
  }
  std::vector<std::string> blacklist;
  SplitString(data, '\n', &blacklist);

  // Tell ExtensionService to update prefs.
  service_->UpdateExtensionBlacklist(blacklist);

  // Update the pref value for blacklist version
  prefs_->SetString(kExtensionBlacklistUpdateVersion,
    ASCIIToWide(current_extension_fetch_.version));
  prefs_->ScheduleSavePersistentPrefs();
}

void ExtensionUpdater::OnCRXFetchComplete(const GURL& url,
                                          const URLRequestStatus& status,
                                          int response_code,
                                          const std::string& data) {
  if (url != current_extension_fetch_.url) {
    LOG(ERROR) << "Called with unexpected url:'" << url.spec()
               << "' expected:'" << current_extension_fetch_.url.spec() << "'";
    NOTREACHED();
  } else if (status.status() == URLRequestStatus::SUCCESS &&
             response_code == 200) {
    if (current_extension_fetch_.id == kBlacklistAppID) {
      ProcessBlacklist(data);
    } else {
      // Successfully fetched - now write crx to a file so we can have the
      // ExtensionsService install it.
      ChromeThread::PostTask(
          ChromeThread::FILE, FROM_HERE,
          NewRunnableMethod(
              file_handler_.get(), &ExtensionUpdaterFileHandler::WriteTempFile,
              current_extension_fetch_.id, data, this));
    }
  } else {
    // TODO(asargent) do things like exponential backoff, handling
    // 503 Service Unavailable / Retry-After headers, etc. here.
    // (http://crbug.com/12546).
    LOG(INFO) << "Failed to fetch extension '" <<
        url.possibly_invalid_spec() << "' response code:" << response_code;
  }
  extension_fetcher_.reset();
  current_extension_fetch_ = ExtensionFetch();

  // If there are any pending downloads left, start one.
  if (extensions_pending_.size() > 0) {
    ExtensionFetch next = extensions_pending_.front();
    extensions_pending_.pop_front();
    FetchUpdatedExtension(next.id, next.url, next.package_hash, next.version);
  }
}

void ExtensionUpdater::OnCRXFileWritten(const std::string& id,
                                        const FilePath& path) {
  service_->UpdateExtension(id, path);
}

void ExtensionUpdater::OnExtensionInstallFinished(const FilePath& path,
                                                  Extension* extension) {
  // Have the file_handler_ delete the temp file on the file I/O thread.
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          file_handler_.get(), &ExtensionUpdaterFileHandler::DeleteFile, path));
}


// Helper function for building up request parameters in update check urls. It
// appends information about one extension to a request parameter string. The
// format for request parameters in update checks is:
//
//   ?x=EXT1_INFO&x=EXT2_INFO
//
// where EXT1_INFO and EXT2_INFO are url-encoded strings of the form:
//
//   id=EXTENSION_ID&v=VERSION&uc
//
// So for two extensions like:
//   Extension 1- id:aaaa version:1.1
//   Extension 2- id:bbbb version:2.0
//
// the full update url would be:
//   http://somehost/path?x=id%3Daaaa%26v%3D1.1%26uc&x=id%3Dbbbb%26v%3D2.0%26uc
//
// (Note that '=' is %3D and '&' is %26 when urlencoded.)
//
// Again, this function would just append one extension's worth of data, e.g.
// "x=id%3Daaaa%26v%3D1.1%26uc"
void AppendExtensionInfo(std::string* str, const Extension& extension) {
    const Version* version = extension.version();
    DCHECK(version);
    std::vector<std::string> parts;

    // Push extension id, version, and uc (indicates an update check to Omaha).
    parts.push_back("id=" + extension.id());
    parts.push_back("v=" + version->GetString());
    parts.push_back("uc");

    str->append("x=" + EscapeQueryParamValue(JoinString(parts, '&')));
}

// Creates a blacklist update url.
GURL ExtensionUpdater::GetBlacklistUpdateUrl(const std::wstring& version) {
  std::string blklist_info = StringPrintf("id=%s&v=%s&uc", kBlacklistAppID,
      WideToASCII(version).c_str());
  return GURL(StringPrintf("%s?x=%s", kBlacklistUpdateUrl,
                           EscapeQueryParamValue(blklist_info).c_str()));
}

void ExtensionUpdater::ScheduleNextCheck(const TimeDelta& target_delay) {
  DCHECK(!timer_.IsRunning());
  DCHECK(target_delay >= TimeDelta::FromSeconds(1));

  // Add +/- 10% random jitter.
  double delay_ms = target_delay.InMillisecondsF();
  double jitter_factor = (RandDouble() * .2) - 0.1;
  delay_ms += delay_ms * jitter_factor;
  TimeDelta actual_delay = TimeDelta::FromMilliseconds(
      static_cast<int64>(delay_ms));

  // Save the time of next check.
  Time next = Time::Now() + actual_delay;
  prefs_->SetInt64(kNextExtensionsUpdateCheck, next.ToInternalValue());
  prefs_->ScheduleSavePersistentPrefs();

  timer_.Start(actual_delay, this, &ExtensionUpdater::TimerFired);
}

void ExtensionUpdater::TimerFired() {
  CheckNow();

  // If the user has overridden the update frequency, don't bother reporting
  // this.
  if (frequency_seconds_ == ExtensionsService::kDefaultUpdateFrequencySeconds) {
    Time last = Time::FromInternalValue(prefs_->GetInt64(
        kLastExtensionsUpdateCheck));
    if (last.ToInternalValue() != 0) {
      // Use counts rather than time so we can use minutes rather than millis.
      UMA_HISTOGRAM_CUSTOM_COUNTS("Extensions.UpdateCheckGap",
          (Time::Now() - last).InMinutes(),
          base::TimeDelta::FromSeconds(kStartupWaitSeconds).InMinutes(),
          base::TimeDelta::FromDays(40).InMinutes(),
          50);  // 50 buckets seems to be the default.
    }
  }

  // Save the last check time, and schedule the next check.
  int64 now = Time::Now().ToInternalValue();
  prefs_->SetInt64(kLastExtensionsUpdateCheck, now);
  ScheduleNextCheck(TimeDelta::FromSeconds(frequency_seconds_));
}

void ExtensionUpdater::CheckNow() {
  // Generate a set of update urls for loaded extensions.
  std::set<GURL> urls;

  if (blacklist_checks_enabled_) {
    urls.insert(GetBlacklistUpdateUrl(
        prefs_->GetString(kExtensionBlacklistUpdateVersion)));
  }

  int no_url_count = 0;
  int google_url_count = 0;
  int other_url_count = 0;
  int theme_count = 0;

  const ExtensionList* extensions = service_->extensions();
  for (ExtensionList::const_iterator iter = extensions->begin();
       iter != extensions->end(); ++iter) {
    Extension* extension = (*iter);
    const GURL& update_url = extension->update_url();
    const std::string google_suffix = ".google.com";
    size_t suffix_index = update_url.host().length() - google_suffix.length();
    if (update_url.host().find(google_suffix) == suffix_index) {
      google_url_count++;
    } else if (update_url.is_empty() || extension->id().empty()) {
      // TODO(asargent) when a default URL is added, make sure to update
      // the total histogram below.  Also, make sure to skip extensions that
      // are "converted_from_user_script".
      if (!extension->converted_from_user_script())
        no_url_count++;
      continue;
    } else {
      other_url_count++;
    }
    if (extension->IsTheme())
      theme_count++;

    DCHECK(update_url.is_valid());
    DCHECK(!update_url.has_ref());

    // Append extension information to the url.
    std::string full_url_string = update_url.spec();
    full_url_string.append(update_url.has_query() ? "&" : "?");
    AppendExtensionInfo(&full_url_string, *extension);

    GURL full_url(full_url_string);
    if (!full_url.is_valid()) {
      LOG(ERROR) << "invalid url: " << full_url.possibly_invalid_spec();
      NOTREACHED();
    } else {
      urls.insert(full_url);
    }
  }
  // Now do an update check for each url we found.
  for (std::set<GURL>::iterator iter = urls.begin(); iter != urls.end();
       ++iter) {
    // StartUpdateCheck makes sure the url isn't already downloading or
    // scheduled, so we don't need to check before calling it.
    StartUpdateCheck(*iter);
  }

  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckExtensions",
                           google_url_count + other_url_count - theme_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckTheme",
                           theme_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckGoogleUrl",
                           google_url_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckOtherUrl",
                           other_url_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateCheckNoUrl",
                           no_url_count);
}


bool ExtensionUpdater::GetExistingVersion(const std::string& id,
                                          std::string* version) {
  if (id == kBlacklistAppID) {
    *version =
      WideToASCII(prefs_->GetString(kExtensionBlacklistUpdateVersion));
    return true;
  }
  Extension* extension = service_->GetExtensionById(id);
  if (!extension) {
    return false;
  }
  *version = extension->version()->GetString();
  return true;
}

std::vector<int> ExtensionUpdater::DetermineUpdates(
    const std::vector<UpdateManifest::Result>& possible_updates) {

  std::vector<int> result;

  // This will only get set if one of possible_updates specifies
  // browser_min_version.
  scoped_ptr<Version> browser_version;

  for (size_t i = 0; i < possible_updates.size(); i++) {
    const UpdateManifest::Result* update = &possible_updates[i];

    std::string version;
    if (!GetExistingVersion(update->extension_id, &version)) {
      continue;
    }

    // If the update version is the same or older than what's already installed,
    // we don't want it.
    scoped_ptr<Version> existing_version(
        Version::GetVersionFromString(version));
    scoped_ptr<Version> update_version(
        Version::GetVersionFromString(update->version));

    if (!update_version.get() ||
        update_version->CompareTo(*(existing_version.get())) <= 0) {
      continue;
    }

    // If the update specifies a browser minimum version, do we qualify?
    if (update->browser_min_version.length() > 0) {
      // First determine the browser version if we haven't already.
      if (!browser_version.get()) {
        scoped_ptr<FileVersionInfo> version_info(
          FileVersionInfo::CreateFileVersionInfoForCurrentModule());
        if (version_info.get()) {
          browser_version.reset(Version::GetVersionFromString(
            version_info->product_version()));
        }
      }
      scoped_ptr<Version> browser_min_version(
          Version::GetVersionFromString(update->browser_min_version));
      if (browser_version.get() && browser_min_version.get() &&
          browser_min_version->CompareTo(*browser_version.get()) > 0) {
        // TODO(asargent) - We may want this to show up in the extensions UI
        // eventually. (http://crbug.com/12547).
        LOG(WARNING) << "Updated version of extension " << update->extension_id
          << " available, but requires chrome version "
          << update->browser_min_version;

        continue;
      }
    }
    result.push_back(i);
  }
  return result;
}

void ExtensionUpdater::StartUpdateCheck(const GURL& url) {
  if (std::find(manifests_pending_.begin(), manifests_pending_.end(), url) !=
      manifests_pending_.end()) {
    return;  // already scheduled
  }

  if (manifest_fetcher_.get() != NULL) {
    if (manifest_fetcher_->url() != url) {
      manifests_pending_.push_back(url);
    }
  } else {
    manifest_fetcher_.reset(
        URLFetcher::Create(kManifestFetcherId, url, URLFetcher::GET, this));
    manifest_fetcher_->set_request_context(Profile::GetDefaultRequestContext());
    manifest_fetcher_->set_load_flags(net::LOAD_DO_NOT_SEND_COOKIES |
                                      net::LOAD_DO_NOT_SAVE_COOKIES);
    manifest_fetcher_->Start();
  }
}

void ExtensionUpdater::FetchUpdatedExtension(const std::string& id,
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
    if (extension_fetcher_->url() != url) {
      extensions_pending_.push_back(ExtensionFetch(id, url, hash, version));
    }
  } else {
    extension_fetcher_.reset(
        URLFetcher::Create(kExtensionFetcherId, url, URLFetcher::GET, this));
    extension_fetcher_->set_request_context(
        Profile::GetDefaultRequestContext());
    extension_fetcher_->set_load_flags(net::LOAD_DO_NOT_SEND_COOKIES |
                                       net::LOAD_DO_NOT_SAVE_COOKIES);
    extension_fetcher_->Start();
    current_extension_fetch_ = ExtensionFetch(id, url, hash, version);
  }
}
