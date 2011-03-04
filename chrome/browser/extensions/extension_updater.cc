// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_updater.h"

#include <algorithm>
#include <set>

#include "base/logging.h"
#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/sha2.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/threading/thread.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/utility_process_host.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_status.h"

#if defined(OS_MACOSX)
#include "base/sys_string_conversions.h"
#endif

using base::RandDouble;
using base::RandInt;
using base::Time;
using base::TimeDelta;
using prefs::kExtensionBlacklistUpdateVersion;
using prefs::kLastExtensionsUpdateCheck;
using prefs::kNextExtensionsUpdateCheck;

// Update AppID for extension blacklist.
const char* ExtensionUpdater::kBlacklistAppID = "com.google.crx.blacklist";

// Wait at least 5 minutes after browser startup before we do any checks. If you
// change this value, make sure to update comments where it is used.
const int kStartupWaitSeconds = 60 * 5;

// For sanity checking on update frequency - enforced in release mode only.
static const int kMinUpdateFrequencySeconds = 30;
static const int kMaxUpdateFrequencySeconds = 60 * 60 * 24 * 7;  // 7 days

// Maximum length of an extension manifest update check url, since it is a GET
// request. We want to stay under 2K because of proxies, etc.
static const int kExtensionsManifestMaxURLSize = 2000;

ManifestFetchData::ManifestFetchData(const GURL& update_url)
    : base_url_(update_url),
      full_url_(update_url) {
}

ManifestFetchData::~ManifestFetchData() {}

// The format for request parameters in update checks is:
//
//   ?x=EXT1_INFO&x=EXT2_INFO
//
// where EXT1_INFO and EXT2_INFO are url-encoded strings of the form:
//
//   id=EXTENSION_ID&v=VERSION&uc
//
// Additionally, we may include the parameter ping=PING_DATA where PING_DATA
// looks like r=DAYS for extensions in the Chrome extensions gallery. This value
// will be present at most once every 24 hours, and indicate the number of days
// since the last time it was present in an update check.
//
// So for two extensions like:
//   Extension 1- id:aaaa version:1.1
//   Extension 2- id:bbbb version:2.0
//
// the full update url would be:
//   http://somehost/path?x=id%3Daaaa%26v%3D1.1%26uc&x=id%3Dbbbb%26v%3D2.0%26uc
//
// (Note that '=' is %3D and '&' is %26 when urlencoded.)
bool ManifestFetchData::AddExtension(std::string id, std::string version,
                                     int days,
                                     const std::string& update_url_data) {
  if (extension_ids_.find(id) != extension_ids_.end()) {
    NOTREACHED() << "Duplicate extension id " << id;
    return false;
  }

  // Compute the string we'd append onto the full_url_, and see if it fits.
  std::vector<std::string> parts;
  parts.push_back("id=" + id);
  parts.push_back("v=" + version);
  parts.push_back("uc");

  if (!update_url_data.empty()) {
    // Make sure the update_url_data string is escaped before using it so that
    // there is no chance of overriding the id or v other parameter value
    // we place into the x= value.
    parts.push_back("ap=" + EscapeQueryParamValue(update_url_data, true));
  }

  if (ShouldPing(days)) {
    parts.push_back("ping=" +
        EscapeQueryParamValue("r=" + base::IntToString(days), true));
  }

  std::string extra = full_url_.has_query() ? "&" : "?";
  extra += "x=" + EscapeQueryParamValue(JoinString(parts, '&'), true);

  // Check against our max url size, exempting the first extension added.
  int new_size = full_url_.possibly_invalid_spec().size() + extra.size();
  if (!extension_ids_.empty() && new_size > kExtensionsManifestMaxURLSize) {
    UMA_HISTOGRAM_PERCENTAGE("Extensions.UpdateCheckHitUrlSizeLimit", 1);
    return false;
  }
  UMA_HISTOGRAM_PERCENTAGE("Extensions.UpdateCheckHitUrlSizeLimit", 0);

  // We have room so go ahead and add the extension.
  extension_ids_.insert(id);
  ping_days_[id] = days;
  full_url_ = GURL(full_url_.possibly_invalid_spec() + extra);
  return true;
}

bool ManifestFetchData::Includes(std::string extension_id) const {
  return extension_ids_.find(extension_id) != extension_ids_.end();
}

bool ManifestFetchData::DidPing(std::string extension_id) const {
  std::map<std::string, int>::const_iterator i = ping_days_.find(extension_id);
  if (i != ping_days_.end()) {
    return ShouldPing(i->second);
  }
  return false;
}

bool ManifestFetchData::ShouldPing(int days) const {
  return base_url_.DomainIs("google.com") &&
         (days == kNeverPinged || days > 0);
}

namespace {

// Calculates the value to use for the ping days parameter.
static int CalculatePingDays(const Time& last_ping_day) {
  int days = ManifestFetchData::kNeverPinged;
  if (!last_ping_day.is_null()) {
    days = (Time::Now() - last_ping_day).InDays();
  }
  return days;
}

}  // namespace

ManifestFetchesBuilder::ManifestFetchesBuilder(
    ExtensionUpdateService* service) : service_(service) {
  DCHECK(service_);
}

ManifestFetchesBuilder::~ManifestFetchesBuilder() {}

void ManifestFetchesBuilder::AddExtension(const Extension& extension) {
  // Skip extensions with empty update URLs converted from user
  // scripts.
  if (extension.converted_from_user_script() &&
      extension.update_url().is_empty()) {
    return;
  }

  // If the extension updates itself from the gallery, ignore any update URL
  // data.  At the moment there is no extra data that an extension can
  // communicate to the the gallery update servers.
  std::string update_url_data;
  if (!extension.UpdatesFromGallery())
    update_url_data = service_->extension_prefs()->
        GetUpdateUrlData(extension.id());

  AddExtensionData(extension.location(),
                   extension.id(),
                   *extension.version(),
                   extension.GetType(),
                   extension.update_url(), update_url_data);
}

void ManifestFetchesBuilder::AddPendingExtension(
    const std::string& id,
    const PendingExtensionInfo& info) {
  // Use a zero version to ensure that a pending extension will always
  // be updated, and thus installed (assuming all extensions have
  // non-zero versions).
  scoped_ptr<Version> version(
      Version::GetVersionFromString("0.0.0.0"));

  AddExtensionData(
      info.install_source(), id, *version,
      Extension::TYPE_UNKNOWN, info.update_url(), "");
}

void ManifestFetchesBuilder::ReportStats() const {
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

std::vector<ManifestFetchData*> ManifestFetchesBuilder::GetFetches() {
  std::vector<ManifestFetchData*> fetches;
  fetches.reserve(fetches_.size());
  for (std::multimap<GURL, ManifestFetchData*>::iterator it =
           fetches_.begin(); it != fetches_.end(); ++it) {
    fetches.push_back(it->second);
  }
  fetches_.clear();
  url_stats_ = URLStats();
  return fetches;
}

void ManifestFetchesBuilder::AddExtensionData(
    Extension::Location location,
    const std::string& id,
    const Version& version,
    Extension::Type extension_type,
    GURL update_url,
    const std::string& update_url_data) {

  if (!Extension::IsAutoUpdateableLocation(location)) {
    return;
  }

  // Skip extensions with non-empty invalid update URLs.
  if (!update_url.is_empty() && !update_url.is_valid()) {
    LOG(WARNING) << "Extension " << id << " has invalid update url "
                 << update_url;
    return;
  }

  // Skip extensions with empty IDs.
  if (id.empty()) {
    LOG(WARNING) << "Found extension with empty ID";
    return;
  }

  if (update_url.DomainIs("google.com")) {
    url_stats_.google_url_count++;
  } else if (update_url.is_empty()) {
    url_stats_.no_url_count++;
    // Fill in default update URL.
    //
    // TODO(akalin): Figure out if we should use the HTTPS version.
    update_url = Extension::GalleryUpdateUrl(false);
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
    case Extension::TYPE_UNKNOWN:
    default:
      ++url_stats_.pending_count;
      break;
  }

  DCHECK(!update_url.is_empty());
  DCHECK(update_url.is_valid());

  ManifestFetchData* fetch = NULL;
  std::multimap<GURL, ManifestFetchData*>::iterator existing_iter =
      fetches_.find(update_url);

  // Find or create a ManifestFetchData to add this extension to.
  int ping_days =
      CalculatePingDays(service_->extension_prefs()->LastPingDay(id));
  while (existing_iter != fetches_.end()) {
    if (existing_iter->second->AddExtension(id, version.GetString(),
                                            ping_days, update_url_data)) {
      fetch = existing_iter->second;
      break;
    }
    existing_iter++;
  }
  if (!fetch) {
    fetch = new ManifestFetchData(update_url);
    fetches_.insert(std::pair<GURL, ManifestFetchData*>(update_url, fetch));
    bool added = fetch->AddExtension(id, version.GetString(), ping_days,
                                     update_url_data);
    DCHECK(added);
  }
}

// A utility class to do file handling on the file I/O thread.
class ExtensionUpdaterFileHandler
    : public base::RefCountedThreadSafe<ExtensionUpdaterFileHandler> {
 public:
  // Writes crx file data into a tempfile, and calls back the updater.
  void WriteTempFile(const std::string& extension_id, const std::string& data,
                     const GURL& download_url,
                     scoped_refptr<ExtensionUpdater> updater) {
    // Make sure we're running in the right thread.
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

    FilePath path;
    if (!file_util::CreateTemporaryFile(&path)) {
      LOG(WARNING) << "Failed to create temporary file path";
      return;
    }
    if (file_util::WriteFile(path, data.c_str(), data.length()) !=
        static_cast<int>(data.length())) {
      // TODO(asargent) - It would be nice to back off updating altogether if
      // the disk is full. (http://crbug.com/12763).
      LOG(ERROR) << "Failed to write temporary file";
      file_util::Delete(path, false);
      return;
    }

    // The ExtensionUpdater now owns the temp file.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(
            updater.get(), &ExtensionUpdater::OnCRXFileWritten, extension_id,
            path, download_url));
  }

 private:
  friend class base::RefCountedThreadSafe<ExtensionUpdaterFileHandler>;

  ~ExtensionUpdaterFileHandler() {}
};

ExtensionUpdater::ExtensionFetch::ExtensionFetch()
    : id(""),
      url(),
      package_hash(""),
      version("") {}

ExtensionUpdater::ExtensionFetch::ExtensionFetch(const std::string& i,
                                                 const GURL& u,
                                                 const std::string& h,
                                                 const std::string& v)
    : id(i), url(u), package_hash(h), version(v) {}

ExtensionUpdater::ExtensionFetch::~ExtensionFetch() {}

ExtensionUpdater::ExtensionUpdater(ExtensionUpdateService* service,
                                   PrefService* prefs,
                                   int frequency_seconds)
    : alive_(false), service_(service), frequency_seconds_(frequency_seconds),
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

ExtensionUpdater::~ExtensionUpdater() {
  Stop();
}

static void EnsureInt64PrefRegistered(PrefService* prefs,
                                      const char name[]) {
  if (!prefs->FindPreference(name))
    prefs->RegisterInt64Pref(name, 0);
}

static void EnsureBlacklistVersionPrefRegistered(PrefService* prefs) {
  if (!prefs->FindPreference(kExtensionBlacklistUpdateVersion))
    prefs->RegisterStringPref(kExtensionBlacklistUpdateVersion, "0");
}

// The overall goal here is to balance keeping clients up to date while
// avoiding a thundering herd against update servers.
TimeDelta ExtensionUpdater::DetermineFirstCheckDelay() {
  DCHECK(alive_);
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
  DCHECK(!alive_);
  // If these are NULL, then that means we've been called after Stop()
  // has been called.
  DCHECK(service_);
  DCHECK(prefs_);
  alive_ = true;
  // Make sure our prefs are registered, then schedule the first check.
  EnsureInt64PrefRegistered(prefs_, kLastExtensionsUpdateCheck);
  EnsureInt64PrefRegistered(prefs_, kNextExtensionsUpdateCheck);
  EnsureBlacklistVersionPrefRegistered(prefs_);
  ScheduleNextCheck(DetermineFirstCheckDelay());
}

void ExtensionUpdater::Stop() {
  alive_ = false;
  service_ = NULL;
  prefs_ = NULL;
  timer_.Stop();
  manifest_fetcher_.reset();
  extension_fetcher_.reset();
  STLDeleteElements(&manifests_pending_);
  manifests_pending_.clear();
  extensions_pending_.clear();
}

void ExtensionUpdater::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  // Stop() destroys all our URLFetchers, which means we shouldn't be
  // called after Stop() is called.
  DCHECK(alive_);

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
  // Takes ownership of |fetch_data|.
  SafeManifestParser(const std::string& xml, ManifestFetchData* fetch_data,
                     ExtensionUpdater* updater)
      : xml_(xml), updater_(updater) {
    fetch_data_.reset(fetch_data);
  }

  // Posts a task over to the IO loop to start the parsing of xml_ in a
  // utility process.
  void Start() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(
            this, &SafeManifestParser::ParseInSandbox,
            g_browser_process->resource_dispatcher_host()));
  }

  // Creates the sandboxed utility process and tells it to start parsing.
  void ParseInSandbox(ResourceDispatcherHost* rdh) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

    // TODO(asargent) we shouldn't need to do this branch here - instead
    // UtilityProcessHost should handle it for us. (http://crbug.com/19192)
    bool use_utility_process = rdh &&
        !CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess);
    if (use_utility_process) {
      UtilityProcessHost* host = new UtilityProcessHost(
          rdh, this, BrowserThread::UI);
      host->StartUpdateManifestParse(xml_);
    } else {
      UpdateManifest manifest;
      if (manifest.Parse(xml_)) {
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            NewRunnableMethod(
                this, &SafeManifestParser::OnParseUpdateManifestSucceeded,
                manifest.results()));
      } else {
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            NewRunnableMethod(
                this, &SafeManifestParser::OnParseUpdateManifestFailed,
                manifest.errors()));
      }
    }
  }

  // Callback from the utility process when parsing succeeded.
  virtual void OnParseUpdateManifestSucceeded(
      const UpdateManifest::Results& results) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    updater_->HandleManifestResults(*fetch_data_, results);
  }

  // Callback from the utility process when parsing failed.
  virtual void OnParseUpdateManifestFailed(const std::string& error_message) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    LOG(WARNING) << "Error parsing update manifest:\n" << error_message;
  }

 private:
  ~SafeManifestParser() {}

  const std::string& xml_;
  scoped_ptr<ManifestFetchData> fetch_data_;

  scoped_refptr<ExtensionUpdater> updater_;
};


void ExtensionUpdater::OnManifestFetchComplete(
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const std::string& data) {
  // We want to try parsing the manifest, and if it indicates updates are
  // available, we want to fire off requests to fetch those updates.
  if (status.status() == net::URLRequestStatus::SUCCESS &&
      (response_code == 200 || (url.SchemeIsFile() && data.length() > 0))) {
    scoped_refptr<SafeManifestParser> safe_parser(
        new SafeManifestParser(data, current_manifest_fetch_.release(), this));
    safe_parser->Start();
  } else {
    // TODO(asargent) Do exponential backoff here. (http://crbug.com/12546).
    VLOG(1) << "Failed to fetch manifest '" << url.possibly_invalid_spec()
            << "' response code:" << response_code;
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

void ExtensionUpdater::HandleManifestResults(
    const ManifestFetchData& fetch_data,
    const UpdateManifest::Results& results) {
  // This can be called after we've been stopped.
  if (!alive_)
    return;

  // Examine the parsed manifest and kick off fetches of any new crx files.
  std::vector<int> updates = DetermineUpdates(fetch_data, results);
  for (size_t i = 0; i < updates.size(); i++) {
    const UpdateManifest::Result* update = &(results.list.at(updates[i]));
    FetchUpdatedExtension(update->extension_id, update->crx_url,
        update->package_hash, update->version);
  }

  // If the manifest response included a <daystart> element, we want to save
  // that value for any extensions which had sent ping_days in the request.
  if (fetch_data.base_url().DomainIs("google.com") &&
      results.daystart_elapsed_seconds >= 0) {
    Time daystart =
      Time::Now() - TimeDelta::FromSeconds(results.daystart_elapsed_seconds);

    const std::set<std::string>& extension_ids = fetch_data.extension_ids();
    std::set<std::string>::const_iterator i;
    for (i = extension_ids.begin(); i != extension_ids.end(); i++) {
      bool did_ping = fetch_data.DidPing(*i);
      if (did_ping) {
        if (*i == kBlacklistAppID) {
          service_->extension_prefs()->SetBlacklistLastPingDay(daystart);
        } else if (service_->GetExtensionById(*i, true) != NULL) {
          service_->extension_prefs()->SetLastPingDay(*i, daystart);
        }
      }
    }
  }
}

void ExtensionUpdater::ProcessBlacklist(const std::string& data) {
  DCHECK(alive_);
  // Verify sha256 hash value.
  char sha256_hash_value[base::SHA256_LENGTH];
  base::SHA256HashString(data, sha256_hash_value, base::SHA256_LENGTH);
  std::string hash_in_hex = base::HexEncode(sha256_hash_value,
                                            base::SHA256_LENGTH);

  if (current_extension_fetch_.package_hash != hash_in_hex) {
    NOTREACHED() << "Fetched blacklist checksum is not as expected. "
      << "Expected: " << current_extension_fetch_.package_hash
      << " Actual: " << hash_in_hex;
    return;
  }
  std::vector<std::string> blacklist;
  base::SplitString(data, '\n', &blacklist);

  // Tell ExtensionService to update prefs.
  service_->UpdateExtensionBlacklist(blacklist);

  // Update the pref value for blacklist version
  prefs_->SetString(kExtensionBlacklistUpdateVersion,
                    current_extension_fetch_.version);
  prefs_->ScheduleSavePersistentPrefs();
}

void ExtensionUpdater::OnCRXFetchComplete(const GURL& url,
                                          const net::URLRequestStatus& status,
                                          int response_code,
                                          const std::string& data) {
  if (status.status() == net::URLRequestStatus::SUCCESS &&
      (response_code == 200 || (url.SchemeIsFile() && data.length() > 0))) {
    if (current_extension_fetch_.id == kBlacklistAppID) {
      ProcessBlacklist(data);
    } else {
      // Successfully fetched - now write crx to a file so we can have the
      // ExtensionService install it.
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          NewRunnableMethod(
              file_handler_.get(), &ExtensionUpdaterFileHandler::WriteTempFile,
              current_extension_fetch_.id, data, url,
              make_scoped_refptr(this)));
    }
  } else {
    // TODO(asargent) do things like exponential backoff, handling
    // 503 Service Unavailable / Retry-After headers, etc. here.
    // (http://crbug.com/12546).
    VLOG(1) << "Failed to fetch extension '" << url.possibly_invalid_spec()
            << "' response code:" << response_code;
  }
  extension_fetcher_.reset();
  current_extension_fetch_ = ExtensionFetch();

  // If there are any pending downloads left, start one.
  if (!extensions_pending_.empty()) {
    ExtensionFetch next = extensions_pending_.front();
    extensions_pending_.pop_front();
    FetchUpdatedExtension(next.id, next.url, next.package_hash, next.version);
  }
}

void ExtensionUpdater::OnCRXFileWritten(const std::string& id,
                                        const FilePath& path,
                                        const GURL& download_url) {
  // This can be called after we've been stopped.
  if (!alive_)
    return;
  // The ExtensionService is now responsible for cleaning up the temp file
  // at |path|.
  service_->UpdateExtension(id, path, download_url);
}


void ExtensionUpdater::ScheduleNextCheck(const TimeDelta& target_delay) {
  DCHECK(alive_);
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
  DCHECK(alive_);
  CheckNow();

  // If the user has overridden the update frequency, don't bother reporting
  // this.
  if (frequency_seconds_ == ExtensionService::kDefaultUpdateFrequencySeconds) {
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
  DCHECK(alive_);
  ManifestFetchesBuilder fetches_builder(service_);

  const ExtensionList* extensions = service_->extensions();
  for (ExtensionList::const_iterator iter = extensions->begin();
       iter != extensions->end(); ++iter) {
    fetches_builder.AddExtension(**iter);
  }

  const PendingExtensionMap& pending_extensions =
      service_->pending_extensions();
  for (PendingExtensionMap::const_iterator iter = pending_extensions.begin();
       iter != pending_extensions.end(); ++iter) {
    Extension::Location location = iter->second.install_source();
    if (location != Extension::EXTERNAL_PREF &&
        location != Extension::EXTERNAL_REGISTRY)
      fetches_builder.AddPendingExtension(iter->first, iter->second);
  }

  fetches_builder.ReportStats();

  std::vector<ManifestFetchData*> fetches(fetches_builder.GetFetches());

  // Start a fetch of the blacklist if needed.
  if (blacklist_checks_enabled_ && service_->HasInstalledExtensions()) {
    // Note: it is very important that we use  the https version of the update
    // url here to avoid DNS hijacking of the blacklist, which is not validated
    // by a public key signature like .crx files are.
    ManifestFetchData* blacklist_fetch =
        new ManifestFetchData(Extension::GalleryUpdateUrl(true));
    std::string version = prefs_->GetString(kExtensionBlacklistUpdateVersion);
    int ping_days =
        CalculatePingDays(service_->extension_prefs()->BlacklistLastPingDay());
    blacklist_fetch->AddExtension(kBlacklistAppID, version, ping_days, "");
    StartUpdateCheck(blacklist_fetch);
  }

  // Now start fetching regular extension updates
  for (std::vector<ManifestFetchData*>::const_iterator it = fetches.begin();
       it != fetches.end(); ++it) {
    // StartUpdateCheck makes sure the url isn't already downloading or
    // scheduled, so we don't need to check before calling it. Ownership of
    // fetch is transferred here.
    StartUpdateCheck(*it);
  }
  // We don't want to use fetches after this since StartUpdateCheck()
  // takes ownership of its argument.
  fetches.clear();
}

bool ExtensionUpdater::GetExistingVersion(const std::string& id,
                                          std::string* version) {
  DCHECK(alive_);
  if (id == kBlacklistAppID) {
    *version = prefs_->GetString(kExtensionBlacklistUpdateVersion);
    return true;
  }
  const Extension* extension = service_->GetExtensionById(id, false);
  if (!extension) {
    return false;
  }
  *version = extension->version()->GetString();
  return true;
}

std::vector<int> ExtensionUpdater::DetermineUpdates(
    const ManifestFetchData& fetch_data,
    const UpdateManifest::Results& possible_updates) {
  DCHECK(alive_);
  std::vector<int> result;

  // This will only get set if one of possible_updates specifies
  // browser_min_version.
  scoped_ptr<Version> browser_version;

  for (size_t i = 0; i < possible_updates.list.size(); i++) {
    const UpdateManifest::Result* update = &possible_updates.list[i];

    if (!fetch_data.Includes(update->extension_id))
      continue;

    if (service_->pending_extensions().find(update->extension_id) ==
        service_->pending_extensions().end()) {
      // If we're not installing pending extension, and the update
      // version is the same or older than what's already installed,
      // we don't want it.
      std::string version;
      if (!GetExistingVersion(update->extension_id, &version))
        continue;

      scoped_ptr<Version> existing_version(
          Version::GetVersionFromString(version));
      scoped_ptr<Version> update_version(
          Version::GetVersionFromString(update->version));

      if (!update_version.get() ||
          update_version->CompareTo(*(existing_version.get())) <= 0) {
        continue;
      }
    }

    // If the update specifies a browser minimum version, do we qualify?
    if (update->browser_min_version.length() > 0) {
      // First determine the browser version if we haven't already.
      if (!browser_version.get()) {
        chrome::VersionInfo version_info;
        if (version_info.is_valid()) {
          browser_version.reset(Version::GetVersionFromString(
                                    version_info.Version()));
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

void ExtensionUpdater::StartUpdateCheck(ManifestFetchData* fetch_data) {
  scoped_ptr<ManifestFetchData> scoped_fetch_data(fetch_data);
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableBackgroundNetworking))
    return;

  std::deque<ManifestFetchData*>::const_iterator i;
  for (i = manifests_pending_.begin(); i != manifests_pending_.end(); i++) {
    if (fetch_data->full_url() == (*i)->full_url()) {
      // This url is already scheduled to be fetched.
      return;
    }
  }

  if (manifest_fetcher_.get() != NULL) {
    if (manifest_fetcher_->url() != fetch_data->full_url()) {
      manifests_pending_.push_back(scoped_fetch_data.release());
    }
  } else {
    UMA_HISTOGRAM_COUNTS("Extensions.UpdateCheckUrlLength",
        fetch_data->full_url().possibly_invalid_spec().length());

    current_manifest_fetch_.swap(scoped_fetch_data);
    manifest_fetcher_.reset(
        URLFetcher::Create(kManifestFetcherId, fetch_data->full_url(),
                           URLFetcher::GET, this));
    manifest_fetcher_->set_request_context(
        service_->profile()->GetRequestContext());
    manifest_fetcher_->set_load_flags(net::LOAD_DO_NOT_SEND_COOKIES |
                                      net::LOAD_DO_NOT_SAVE_COOKIES |
                                      net::LOAD_DISABLE_CACHE);
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
        service_->profile()->GetRequestContext());
    extension_fetcher_->set_load_flags(net::LOAD_DO_NOT_SEND_COOKIES |
                                       net::LOAD_DO_NOT_SAVE_COOKIES |
                                       net::LOAD_DISABLE_CACHE);
    extension_fetcher_->Start();
    current_extension_fetch_ = ExtensionFetch(id, url, hash, version);
  }
}
