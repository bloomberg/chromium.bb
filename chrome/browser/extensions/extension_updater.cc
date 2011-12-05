// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_updater.h"

#include <algorithm>
#include <set>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_handle.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/pref_names.h"
#include "content/browser/utility_process_host.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/common/url_fetcher.h"
#include "crypto/sha2.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_status.h"

#if defined(OS_MACOSX)
#include "base/sys_string_conversions.h"
#endif

#define SEND_ACTIVE_PINGS 1

using base::RandDouble;
using base::RandInt;
using base::Time;
using base::TimeDelta;
using content::BrowserThread;
using prefs::kExtensionBlacklistUpdateVersion;
using prefs::kLastExtensionsUpdateCheck;
using prefs::kNextExtensionsUpdateCheck;

// Update AppID for extension blacklist.
const char* ExtensionUpdater::kBlacklistAppID = "com.google.crx.blacklist";

namespace {

// Wait at least 5 minutes after browser startup before we do any checks. If you
// change this value, make sure to update comments where it is used.
const int kStartupWaitSeconds = 60 * 5;

// For sanity checking on update frequency - enforced in release mode only.
static const int kMinUpdateFrequencySeconds = 30;
static const int kMaxUpdateFrequencySeconds = 60 * 60 * 24 * 7;  // 7 days

// Maximum length of an extension manifest update check url, since it is a GET
// request. We want to stay under 2K because of proxies, etc.
static const int kExtensionsManifestMaxURLSize = 2000;

// TODO(skerner): It would be nice to know if the file system failure
// happens when creating a temp file or when writing to it. Knowing this
// will require changes to URLFetcher.
enum FileWriteResult {
  SUCCESS = 0,
  CANT_CREATE_OR_WRITE_TEMP_CRX,
  CANT_READ_CRX_FILE,
  NUM_FILE_WRITE_RESULTS
};

// Prototypes allow the functions to be defined in the order they run.
void CheckThatCRXIsReadable(const FilePath& crx_path);
void RecordFileUpdateHistogram(FileWriteResult file_write_result);

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

void RecordFileUpdateHistogram(FileWriteResult file_write_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UMA_HISTOGRAM_ENUMERATION("Extensions.UpdaterWriteCrxAsFile",
                            file_write_result,
                            NUM_FILE_WRITE_RESULTS);
}

}  // namespace

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
// looks like r=DAYS or a=DAYS for extensions in the Chrome extensions gallery.
// ('r' refers to 'roll call' ie installation, and 'a' refers to 'active').
// These values will each be present at most once every 24 hours, and indicate
// the number of days since the last time it was present in an update check.
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
                                     const PingData& ping_data,
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
    parts.push_back("ap=" + net::EscapeQueryParamValue(update_url_data, true));
  }

  // Append brand code, rollcall and active ping parameters.
  if (base_url_.DomainIs("google.com")) {
#if defined(GOOGLE_CHROME_BUILD)
    std::string brand;
    google_util::GetBrand(&brand);
    if (!brand.empty() && !google_util::IsOrganic(brand))
      parts.push_back("brand=" + brand);
#endif

    std::string ping_value;
    pings_[id] = PingData(0, 0);

    if (ping_data.rollcall_days == kNeverPinged ||
        ping_data.rollcall_days > 0) {
      ping_value += "r=" + base::IntToString(ping_data.rollcall_days);
      pings_[id].rollcall_days = ping_data.rollcall_days;
    }
#if SEND_ACTIVE_PINGS
    if (ping_data.active_days == kNeverPinged || ping_data.active_days > 0) {
      if (!ping_value.empty())
        ping_value += "&";
      ping_value += "a=" + base::IntToString(ping_data.active_days);
      pings_[id].active_days = ping_data.active_days;
    }
#endif  // SEND_ACTIVE_PINGS
    if (!ping_value.empty())
      parts.push_back("ping=" + net::EscapeQueryParamValue(ping_value, true));
  }

  std::string extra = full_url_.has_query() ? "&" : "?";
  extra += "x=" + net::EscapeQueryParamValue(JoinString(parts, '&'), true);

  // Check against our max url size, exempting the first extension added.
  int new_size = full_url_.possibly_invalid_spec().size() + extra.size();
  if (!extension_ids_.empty() && new_size > kExtensionsManifestMaxURLSize) {
    UMA_HISTOGRAM_PERCENTAGE("Extensions.UpdateCheckHitUrlSizeLimit", 1);
    return false;
  }
  UMA_HISTOGRAM_PERCENTAGE("Extensions.UpdateCheckHitUrlSizeLimit", 0);

  // We have room so go ahead and add the extension.
  extension_ids_.insert(id);
  full_url_ = GURL(full_url_.possibly_invalid_spec() + extra);
  return true;
}

bool ManifestFetchData::Includes(const std::string& extension_id) const {
  return extension_ids_.find(extension_id) != extension_ids_.end();
}

bool ManifestFetchData::DidPing(std::string extension_id, PingType type) const {
  std::map<std::string, PingData>::const_iterator i = pings_.find(extension_id);
  if (i == pings_.end())
    return false;
  int value = 0;
  if (type == ROLLCALL)
    value = i->second.rollcall_days;
  else if (type == ACTIVE)
    value = i->second.active_days;
  else
    NOTREACHED();
  return value == kNeverPinged || value > 0;
}

namespace {

// When we've computed a days value, we want to make sure we don't send a
// negative value (due to the system clock being set backwards, etc.), since -1
// is a special sentinel value that means "never pinged", and other negative
// values don't make sense.
static int SanitizeDays(int days) {
  if (days < 0)
    return 0;
  return days;
}

// Calculates the value to use for the ping days parameter.
static int CalculatePingDays(const Time& last_ping_day) {
  int days = ManifestFetchData::kNeverPinged;
  if (!last_ping_day.is_null()) {
    days = SanitizeDays((Time::Now() - last_ping_day).InDays());
  }
  return days;
}

static int CalculateActivePingDays(const Time& last_active_ping_day,
                                   bool hasActiveBit) {
  if (!hasActiveBit)
    return 0;
  if (last_active_ping_day.is_null())
    return ManifestFetchData::kNeverPinged;
  return SanitizeDays((Time::Now() - last_active_ping_day).InDays());
}

}  // namespace

ManifestFetchesBuilder::ManifestFetchesBuilder(
    ExtensionServiceInterface* service,
    ExtensionPrefs* prefs)
    : service_(service), prefs_(prefs) {
  DCHECK(service_);
  DCHECK(prefs_);
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
    update_url_data = prefs_->GetUpdateUrlData(extension.id());

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
  Version version("0.0.0.0");
  DCHECK(version.IsValid());

  AddExtensionData(info.install_source(), id, version,
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

  DCHECK(!update_url.is_empty());
  DCHECK(update_url.is_valid());

  ManifestFetchData* fetch = NULL;
  std::multimap<GURL, ManifestFetchData*>::iterator existing_iter =
      fetches_.find(update_url);

  // Find or create a ManifestFetchData to add this extension to.
  ManifestFetchData::PingData ping_data;
  ping_data.rollcall_days = CalculatePingDays(prefs_->LastPingDay(id));
  ping_data.active_days =
      CalculateActivePingDays(prefs_->LastActivePingDay(id),
                              prefs_->GetActiveBit(id));
  while (existing_iter != fetches_.end()) {
    if (existing_iter->second->AddExtension(id, version.GetString(),
                                            ping_data, update_url_data)) {
      fetch = existing_iter->second;
      break;
    }
    existing_iter++;
  }
  if (!fetch) {
    fetch = new ManifestFetchData(update_url);
    fetches_.insert(std::pair<GURL, ManifestFetchData*>(update_url, fetch));
    bool added = fetch->AddExtension(id, version.GetString(), ping_data,
                                     update_url_data);
    DCHECK(added);
  }
}

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

ExtensionUpdater::FetchedCRXFile::FetchedCRXFile(const std::string& i,
                                                 const FilePath& p,
                                                 const GURL& u)
    : id(i),
      path(p),
      download_url(u) {}

ExtensionUpdater::FetchedCRXFile::FetchedCRXFile()
    : id(""),
      path(),
      download_url() {}

ExtensionUpdater::FetchedCRXFile::~FetchedCRXFile() {}

ExtensionUpdater::ExtensionUpdater(ExtensionServiceInterface* service,
                                   ExtensionPrefs* extension_prefs,
                                   PrefService* prefs,
                                   Profile* profile,
                                   int frequency_seconds)
    : alive_(false),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      service_(service), frequency_seconds_(frequency_seconds),
      will_check_soon_(false), extension_prefs_(extension_prefs),
      prefs_(prefs), profile_(profile), blacklist_checks_enabled_(true),
      crx_install_is_running_(false) {
  Init();
}

void ExtensionUpdater::Init() {
  DCHECK_GE(frequency_seconds_, 5);
  DCHECK_LE(frequency_seconds_, kMaxUpdateFrequencySeconds);
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
    prefs->RegisterInt64Pref(name, 0, PrefService::UNSYNCABLE_PREF);
}

static void EnsureBlacklistVersionPrefRegistered(PrefService* prefs) {
  if (!prefs->FindPreference(kExtensionBlacklistUpdateVersion)) {
    prefs->RegisterStringPref(kExtensionBlacklistUpdateVersion,
                              "0",
                              PrefService::UNSYNCABLE_PREF);
  }
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
  DCHECK(extension_prefs_);
  DCHECK(prefs_);
  DCHECK(profile_);
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());
  alive_ = true;
  // Make sure our prefs are registered, then schedule the first check.
  EnsureInt64PrefRegistered(prefs_, kLastExtensionsUpdateCheck);
  EnsureInt64PrefRegistered(prefs_, kNextExtensionsUpdateCheck);
  EnsureBlacklistVersionPrefRegistered(prefs_);
  ScheduleNextCheck(DetermineFirstCheckDelay());
}

void ExtensionUpdater::Stop() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  alive_ = false;
  service_ = NULL;
  extension_prefs_ = NULL;
  prefs_ = NULL;
  profile_ = NULL;
  timer_.Stop();
  will_check_soon_ = false;
  manifest_fetcher_.reset();
  extension_fetcher_.reset();
  STLDeleteElements(&manifests_pending_);
  manifests_pending_.clear();
  extensions_pending_.clear();
}

void ExtensionUpdater::OnURLFetchComplete(const content::URLFetcher* source) {
  // Stop() destroys all our URLFetchers, which means we shouldn't be
  // called after Stop() is called.
  DCHECK(alive_);

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
  NotifyIfFinished();
}

// Utility class to handle doing xml parsing in a sandboxed utility process.
class SafeManifestParser : public UtilityProcessHost::Client {
 public:
  // Takes ownership of |fetch_data|.
  SafeManifestParser(const std::string& xml, ManifestFetchData* fetch_data,
                     base::WeakPtr<ExtensionUpdater> updater)
      : xml_(xml), updater_(updater) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    fetch_data_.reset(fetch_data);
  }

  // Posts a task over to the IO loop to start the parsing of xml_ in a
  // utility process.
  void Start() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (!BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE,
            base::Bind(
                &SafeManifestParser::ParseInSandbox, this,
                g_browser_process->resource_dispatcher_host()))) {
      NOTREACHED();
    }
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
          this, BrowserThread::UI);
      host->Send(new ChromeUtilityMsg_ParseUpdateManifest(xml_));
    } else {
      UpdateManifest manifest;
      if (manifest.Parse(xml_)) {
        if (!BrowserThread::PostTask(
                BrowserThread::UI, FROM_HERE,
                base::Bind(
                    &SafeManifestParser::OnParseUpdateManifestSucceeded, this,
                    manifest.results()))) {
          NOTREACHED();
        }
      } else {
        if (!BrowserThread::PostTask(
                BrowserThread::UI, FROM_HERE,
                base::Bind(
                    &SafeManifestParser::OnParseUpdateManifestFailed, this,
                    manifest.errors()))) {
          NOTREACHED();
        }
      }
    }
  }

  // UtilityProcessHost::Client
  virtual bool OnMessageReceived(const IPC::Message& message) {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(SafeManifestParser, message)
      IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ParseUpdateManifest_Succeeded,
                          OnParseUpdateManifestSucceeded)
      IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ParseUpdateManifest_Failed,
                          OnParseUpdateManifestFailed)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void OnParseUpdateManifestSucceeded(
      const UpdateManifest::Results& results) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (!updater_) {
      return;
    }
    updater_->HandleManifestResults(*fetch_data_, &results);
  }

  void OnParseUpdateManifestFailed(const std::string& error_message) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (!updater_) {
      return;
    }
    LOG(WARNING) << "Error parsing update manifest:\n" << error_message;
    updater_->HandleManifestResults(*fetch_data_, NULL);
  }

 private:
  ~SafeManifestParser() {
    // If we're using UtilityProcessHost, we may not be destroyed on
    // the UI or IO thread.
  }

  const std::string xml_;

  // Should be accessed only on UI thread.
  scoped_ptr<ManifestFetchData> fetch_data_;
  base::WeakPtr<ExtensionUpdater> updater_;
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
        new SafeManifestParser(data, current_manifest_fetch_.release(),
                               weak_ptr_factory_.GetWeakPtr()));
    safe_parser->Start();
  } else {
    // TODO(asargent) Do exponential backoff here. (http://crbug.com/12546).
    VLOG(1) << "Failed to fetch manifest '" << url.possibly_invalid_spec()
            << "' response code:" << response_code;
    RemoveFromInProgress(current_manifest_fetch_->extension_ids());
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
    const UpdateManifest::Results* results) {
  DCHECK(alive_);

  // Remove all the ids's from in_progress_ids_ (we will add them back in
  // below if they actually have updates we need to fetch and install).
  RemoveFromInProgress(fetch_data.extension_ids());

  if (!results) {
    NotifyIfFinished();
    return;
  }

  // Examine the parsed manifest and kick off fetches of any new crx files.
  std::vector<int> updates = DetermineUpdates(fetch_data, *results);
  for (size_t i = 0; i < updates.size(); i++) {
    const UpdateManifest::Result* update = &(results->list.at(updates[i]));
    const std::string& id = update->extension_id;
    in_progress_ids_.insert(id);
    if (id != std::string(kBlacklistAppID))
      NotifyUpdateFound(update->extension_id);
    FetchUpdatedExtension(update->extension_id, update->crx_url,
        update->package_hash, update->version);
  }

  // If the manifest response included a <daystart> element, we want to save
  // that value for any extensions which had sent a ping in the request.
  if (fetch_data.base_url().DomainIs("google.com") &&
      results->daystart_elapsed_seconds >= 0) {
    Time daystart =
      Time::Now() - TimeDelta::FromSeconds(results->daystart_elapsed_seconds);

    const std::set<std::string>& extension_ids = fetch_data.extension_ids();
    std::set<std::string>::const_iterator i;
    for (i = extension_ids.begin(); i != extension_ids.end(); i++) {
      if (fetch_data.DidPing(*i, ManifestFetchData::ROLLCALL)) {
        if (*i == kBlacklistAppID) {
          extension_prefs_->SetBlacklistLastPingDay(daystart);
        } else if (service_->GetExtensionById(*i, true) != NULL) {
          extension_prefs_->SetLastPingDay(*i, daystart);
        }
      }
      if (extension_prefs_->GetActiveBit(*i)) {
        extension_prefs_->SetActiveBit(*i, false);
        extension_prefs_->SetLastActivePingDay(*i, daystart);
      }
    }
  }
  NotifyIfFinished();
}

void ExtensionUpdater::ProcessBlacklist(const std::string& data) {
  DCHECK(alive_);
  // Verify sha256 hash value.
  char sha256_hash_value[crypto::kSHA256Length];
  crypto::SHA256HashString(data, sha256_hash_value, crypto::kSHA256Length);
  std::string hash_in_hex = base::HexEncode(sha256_hash_value,
                                            crypto::kSHA256Length);

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

void ExtensionUpdater::OnCRXFetchComplete(
    const content::URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code) {

  base::PlatformFileError error_code = base::PLATFORM_FILE_OK;
  if (source->FileErrorOccurred(&error_code)) {
    LOG(ERROR) << "Failed to write update CRX with id "
               << current_extension_fetch_.id << ". "
               << "Error code is "<< error_code;

    RecordCRXWriteHistogram(false, FilePath());
    OnCRXFileWriteError(current_extension_fetch_.id);

  } else if (status.status() == net::URLRequestStatus::SUCCESS &&
      (response_code == 200 || url.SchemeIsFile())) {
    if (current_extension_fetch_.id == kBlacklistAppID) {
      std::string data;
      source->GetResponseAsString(&data);
      ProcessBlacklist(data);
      in_progress_ids_.erase(current_extension_fetch_.id);
    } else {
      FilePath crx_path;
      // Take ownership of the file at |crx_path|.
      CHECK(source->GetResponseAsFilePath(true, &crx_path));
      RecordCRXWriteHistogram(true, crx_path);
      OnCRXFileWritten(current_extension_fetch_.id, crx_path, url);
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

  // If there are any pending downloads left, start the next one.
  if (!extensions_pending_.empty()) {
    ExtensionFetch next = extensions_pending_.front();
    extensions_pending_.pop_front();
    FetchUpdatedExtension(next.id, next.url, next.package_hash, next.version);
  }
}

void ExtensionUpdater::OnCRXFileWritten(const std::string& id,
                                        const FilePath& path,
                                        const GURL& download_url) {
  DCHECK(alive_);

  FetchedCRXFile fetched(id, path, download_url);
  fetched_crx_files_.push(fetched);

  MaybeInstallCRXFile();
}

bool ExtensionUpdater::MaybeInstallCRXFile() {
  if (crx_install_is_running_)
    return false;

  while (!fetched_crx_files_.empty() && !crx_install_is_running_) {
    const FetchedCRXFile& crx_file = fetched_crx_files_.top();

    // The ExtensionService is now responsible for cleaning up the temp file
    // at |extension_file.path|.
    CrxInstaller* installer = NULL;
    if (service_->UpdateExtension(crx_file.id,
                                  crx_file.path,
                                  crx_file.download_url,
                                  &installer)) {
      crx_install_is_running_ = true;

      // Source parameter ensures that we only see the completion event for the
      // the installer we started.
      registrar_.Add(this,
                     chrome::NOTIFICATION_CRX_INSTALLER_DONE,
                     content::Source<CrxInstaller>(installer));
    }
    in_progress_ids_.erase(crx_file.id);
    fetched_crx_files_.pop();
  }

  // If an updater is running, it was started above.
  return crx_install_is_running_;
}

void ExtensionUpdater::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_CRX_INSTALLER_DONE);

  // No need to listen for CRX_INSTALLER_DONE anymore.
  registrar_.Remove(this,
                    chrome::NOTIFICATION_CRX_INSTALLER_DONE,
                    source);
  crx_install_is_running_ = false;
  // If any files are available to update, start one.
  MaybeInstallCRXFile();
}

void ExtensionUpdater::OnCRXFileWriteError(const std::string& id) {
  DCHECK(alive_);
  in_progress_ids_.erase(id);
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

  timer_.Start(FROM_HERE, actual_delay, this, &ExtensionUpdater::TimerFired);
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

void ExtensionUpdater::CheckSoon() {
  DCHECK(alive_);
  if (will_check_soon_) {
    return;
  }
  if (BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&ExtensionUpdater::DoCheckSoon,
                     weak_ptr_factory_.GetWeakPtr()))) {
    will_check_soon_ = true;
  } else {
    NOTREACHED();
  }
}

bool ExtensionUpdater::WillCheckSoon() const {
  return will_check_soon_;
}

void ExtensionUpdater::DoCheckSoon() {
  DCHECK(will_check_soon_);
  CheckNow();
  will_check_soon_ = false;
}

void ExtensionUpdater::CheckNow() {
  DCHECK(alive_);
  NotifyStarted();
  ManifestFetchesBuilder fetches_builder(service_, extension_prefs_);

  const PendingExtensionManager* pending_extension_manager =
      service_->pending_extension_manager();
  std::set<std::string> pending_ids;

  PendingExtensionManager::const_iterator iter;
  for (iter = pending_extension_manager->begin();
       iter != pending_extension_manager->end(); iter++) {
    // TODO(skerner): Move the determination of what gets fetched into
    // class PendingExtensionManager.
    Extension::Location location = iter->second.install_source();
    if (location != Extension::EXTERNAL_PREF &&
        location != Extension::EXTERNAL_REGISTRY) {
      fetches_builder.AddPendingExtension(iter->first, iter->second);
      pending_ids.insert(iter->first);
    }
  }

  const ExtensionSet* extensions = service_->extensions();
  for (ExtensionSet::const_iterator iter = extensions->begin();
       iter != extensions->end(); ++iter) {
    // An extension might be overwritten by policy, and have its update url
    // changed. Make sure existing extensions aren't fetched again, if a
    // pending fetch for an extension with the same id already exists.
    if (!ContainsKey(pending_ids, (*iter)->id())) {
      fetches_builder.AddExtension(**iter);
    }
  }

  fetches_builder.ReportStats();

  std::vector<ManifestFetchData*> fetches(fetches_builder.GetFetches());

  // Start a fetch of the blacklist if needed.
  if (blacklist_checks_enabled_) {
    // Note: it is very important that we use  the https version of the update
    // url here to avoid DNS hijacking of the blacklist, which is not validated
    // by a public key signature like .crx files are.
    ManifestFetchData* blacklist_fetch =
        new ManifestFetchData(extension_urls::GetWebstoreUpdateUrl(true));
    std::string version = prefs_->GetString(kExtensionBlacklistUpdateVersion);
    ManifestFetchData::PingData ping_data;
    ping_data.rollcall_days =
        CalculatePingDays(extension_prefs_->BlacklistLastPingDay());
    blacklist_fetch->AddExtension(kBlacklistAppID, version, ping_data, "");
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

  NotifyIfFinished();
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

  // This will only be valid if one of possible_updates specifies
  // browser_min_version.
  Version browser_version;
  PendingExtensionManager* pending_extension_manager =
      service_->pending_extension_manager();

  for (size_t i = 0; i < possible_updates.list.size(); i++) {
    const UpdateManifest::Result* update = &possible_updates.list[i];

    if (!fetch_data.Includes(update->extension_id))
      continue;

    if (!pending_extension_manager->IsIdPending(update->extension_id)) {
      // If we're not installing pending extension, and the update
      // version is the same or older than what's already installed,
      // we don't want it.
      std::string version;
      if (!GetExistingVersion(update->extension_id, &version))
        continue;

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
  AddToInProgress(fetch_data->extension_ids());

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
    if (manifest_fetcher_->GetURL() != fetch_data->full_url()) {
      manifests_pending_.push_back(scoped_fetch_data.release());
    }
  } else {
    UMA_HISTOGRAM_COUNTS("Extensions.UpdateCheckUrlLength",
        fetch_data->full_url().possibly_invalid_spec().length());

    current_manifest_fetch_.swap(scoped_fetch_data);
    manifest_fetcher_.reset(content::URLFetcher::Create(
        kManifestFetcherId, fetch_data->full_url(), content::URLFetcher::GET,
        this));
    manifest_fetcher_->SetRequestContext(profile_->GetRequestContext());
    manifest_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
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
    if (extension_fetcher_->GetURL() != url) {
      extensions_pending_.push_back(ExtensionFetch(id, url, hash, version));
    }
  } else {
    extension_fetcher_.reset(content::URLFetcher::Create(
        kExtensionFetcherId, url, content::URLFetcher::GET, this));
    extension_fetcher_->SetRequestContext(
        profile_->GetRequestContext());
    extension_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                                     net::LOAD_DO_NOT_SAVE_COOKIES |
                                     net::LOAD_DISABLE_CACHE);
    // Download CRX files to a temp file. The blacklist is small and will be
    // processed in memory, so it is fetched into a string.
    if (id != ExtensionUpdater::kBlacklistAppID) {
      extension_fetcher_->SaveResponseToTemporaryFile(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
    }

    extension_fetcher_->Start();
    current_extension_fetch_ = ExtensionFetch(id, url, hash, version);
  }
}

void ExtensionUpdater::NotifyStarted() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_UPDATING_STARTED,
      content::Source<Profile>(profile_),
      content::NotificationService::NoDetails());
}

void ExtensionUpdater::NotifyUpdateFound(const std::string& extension_id) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_UPDATE_FOUND,
      content::Source<Profile>(profile_),
      content::Details<const std::string>(&extension_id));
}

void ExtensionUpdater::NotifyIfFinished() {
  if (in_progress_ids_.empty()) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_UPDATING_FINISHED,
        content::Source<Profile>(profile_),
        content::NotificationService::NoDetails());
    VLOG(1) << "Sending EXTENSION_UPDATING_FINISHED";
  }
}

void ExtensionUpdater::AddToInProgress(const std::set<std::string>& ids) {
  std::set<std::string>::const_iterator i;
  for (i = ids.begin(); i != ids.end(); ++i)
    in_progress_ids_.insert(*i);
}

void ExtensionUpdater::RemoveFromInProgress(const std::set<std::string>& ids) {
  std::set<std::string>::const_iterator i;
  for (i = ids.begin(); i != ids.end(); ++i)
    in_progress_ids_.erase(*i);
}
