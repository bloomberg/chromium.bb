// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/extension_updater.h"

#include <algorithm>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/extensions/updater/extension_downloader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "crypto/sha2.h"

using base::RandDouble;
using base::RandInt;
using base::Time;
using base::TimeDelta;
using content::BrowserThread;
using prefs::kExtensionBlacklistUpdateVersion;
using prefs::kLastExtensionsUpdateCheck;
using prefs::kNextExtensionsUpdateCheck;

typedef extensions::ExtensionDownloaderDelegate::Error Error;
typedef extensions::ExtensionDownloaderDelegate::PingResult PingResult;

namespace {

// Wait at least 5 minutes after browser startup before we do any checks. If you
// change this value, make sure to update comments where it is used.
const int kStartupWaitSeconds = 60 * 5;

// For sanity checking on update frequency - enforced in release mode only.
const int kMinUpdateFrequencySeconds = 30;
const int kMaxUpdateFrequencySeconds = 60 * 60 * 24 * 7;  // 7 days

// Require at least 5 seconds between consecutive non-succesful extension update
// checks.
const int kMinUpdateThrottleTime = 5;

// When we've computed a days value, we want to make sure we don't send a
// negative value (due to the system clock being set backwards, etc.), since -1
// is a special sentinel value that means "never pinged", and other negative
// values don't make sense.
int SanitizeDays(int days) {
  if (days < 0)
    return 0;
  return days;
}

// Calculates the value to use for the ping days parameter.
int CalculatePingDays(const Time& last_ping_day) {
  int days = extensions::ManifestFetchData::kNeverPinged;
  if (!last_ping_day.is_null()) {
    days = SanitizeDays((Time::Now() - last_ping_day).InDays());
  }
  return days;
}

int CalculateActivePingDays(const Time& last_active_ping_day,
                            bool hasActiveBit) {
  if (!hasActiveBit)
    return 0;
  if (last_active_ping_day.is_null())
    return extensions::ManifestFetchData::kNeverPinged;
  return SanitizeDays((Time::Now() - last_active_ping_day).InDays());
}

}  // namespace

namespace extensions {

ExtensionUpdater::CheckParams::CheckParams()
    : check_blacklist(true), install_immediately(false) {}

ExtensionUpdater::CheckParams::~CheckParams() {}

ExtensionUpdater::FetchedCRXFile::FetchedCRXFile(
    const std::string& i,
    const base::FilePath& p,
    const GURL& u,
    const std::set<int>& request_ids)
    : extension_id(i),
      path(p),
      download_url(u),
      request_ids(request_ids) {}

ExtensionUpdater::FetchedCRXFile::FetchedCRXFile()
    : extension_id(""),
      path(),
      download_url() {}

ExtensionUpdater::FetchedCRXFile::~FetchedCRXFile() {}

ExtensionUpdater::InProgressCheck::InProgressCheck()
    : install_immediately(false) {}

ExtensionUpdater::InProgressCheck::~InProgressCheck() {}

struct ExtensionUpdater::ThrottleInfo {
  ThrottleInfo()
      : in_progress(true),
        throttle_delay(kMinUpdateThrottleTime),
        check_start(Time::Now()) {}

  bool in_progress;
  int throttle_delay;
  Time check_start;
};

ExtensionUpdater::ExtensionUpdater(ExtensionServiceInterface* service,
                                   ExtensionPrefs* extension_prefs,
                                   PrefService* prefs,
                                   Profile* profile,
                                   Blacklist* blacklist,
                                   int frequency_seconds)
    : alive_(false),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      service_(service), frequency_seconds_(frequency_seconds),
      will_check_soon_(false), extension_prefs_(extension_prefs),
      prefs_(prefs), profile_(profile), blacklist_(blacklist),
      next_request_id_(0),
      crx_install_is_running_(false) {
  DCHECK_GE(frequency_seconds_, 5);
  DCHECK_LE(frequency_seconds_, kMaxUpdateFrequencySeconds);
#ifdef NDEBUG
  // In Release mode we enforce that update checks don't happen too often.
  frequency_seconds_ = std::max(frequency_seconds_, kMinUpdateFrequencySeconds);
#endif
  frequency_seconds_ = std::min(frequency_seconds_, kMaxUpdateFrequencySeconds);

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

ExtensionUpdater::~ExtensionUpdater() {
  Stop();
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
  downloader_.reset();
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

  timer_.Start(FROM_HERE, actual_delay, this, &ExtensionUpdater::TimerFired);
}

void ExtensionUpdater::TimerFired() {
  DCHECK(alive_);
  CheckNow(default_params_);

  // If the user has overridden the update frequency, don't bother reporting
  // this.
  if (frequency_seconds_ == ExtensionService::kDefaultUpdateFrequencySeconds) {
    Time last = Time::FromInternalValue(prefs_->GetInt64(
        kLastExtensionsUpdateCheck));
    if (last.ToInternalValue() != 0) {
      // Use counts rather than time so we can use minutes rather than millis.
      UMA_HISTOGRAM_CUSTOM_COUNTS("Extensions.UpdateCheckGap",
          (Time::Now() - last).InMinutes(),
          TimeDelta::FromSeconds(kStartupWaitSeconds).InMinutes(),
          TimeDelta::FromDays(40).InMinutes(),
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
  if (will_check_soon_)
    return;
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
  CheckNow(default_params_);
  will_check_soon_ = false;
}

void ExtensionUpdater::AddToDownloader(
    const ExtensionSet* extensions,
    const std::list<std::string>& pending_ids,
    int request_id) {
  InProgressCheck& request = requests_in_progress_[request_id];
  for (ExtensionSet::const_iterator extension_iter = extensions->begin();
       extension_iter != extensions->end(); ++extension_iter) {
    const Extension& extension = **extension_iter;
    if (!Manifest::IsAutoUpdateableLocation(extension.location())) {
      VLOG(2) << "Extension " << extension.id() << " is not auto updateable";
      continue;
    }
    // An extension might be overwritten by policy, and have its update url
    // changed. Make sure existing extensions aren't fetched again, if a
    // pending fetch for an extension with the same id already exists.
    std::list<std::string>::const_iterator pending_id_iter = std::find(
        pending_ids.begin(), pending_ids.end(), extension.id());
    if (pending_id_iter == pending_ids.end()) {
      if (downloader_->AddExtension(extension, request_id))
        request.in_progress_ids_.push_back(extension.id());
    }
  }
}

void ExtensionUpdater::CheckNow(const CheckParams& params) {
  int request_id = next_request_id_++;

  VLOG(2) << "Starting update check " << request_id;
  if (params.ids.empty())
    NotifyStarted();

  DCHECK(alive_);

  InProgressCheck& request = requests_in_progress_[request_id];
  request.callback = params.callback;
  request.install_immediately = params.install_immediately;

  if (!downloader_.get()) {
    downloader_.reset(
        new ExtensionDownloader(this, profile_->GetRequestContext()));
  }

  // Add fetch records for extensions that should be fetched by an update URL.
  // These extensions are not yet installed. They come from group policy
  // and external install sources.
  const PendingExtensionManager* pending_extension_manager =
      service_->pending_extension_manager();

  std::list<std::string> pending_ids;

  if (params.ids.empty()) {
    // If no extension ids are specified, check for updates for all extensions.
    pending_extension_manager->GetPendingIdsForUpdateCheck(&pending_ids);

    std::list<std::string>::const_iterator iter;
    for (iter = pending_ids.begin(); iter != pending_ids.end(); ++iter) {
      const PendingExtensionInfo* info = pending_extension_manager->GetById(
          *iter);
      if (!Manifest::IsAutoUpdateableLocation(info->install_source())) {
        VLOG(2) << "Extension " << *iter << " is not auto updateable";
        continue;
      }
      if (downloader_->AddPendingExtension(*iter, info->update_url(),
                                           request_id))
        request.in_progress_ids_.push_back(*iter);
    }

    AddToDownloader(service_->extensions(), pending_ids, request_id);
    AddToDownloader(service_->disabled_extensions(), pending_ids, request_id);
  } else {
    for (std::list<std::string>::const_iterator it = params.ids.begin();
         it != params.ids.end(); ++it) {
      const Extension* extension = service_->GetExtensionById(*it, true);
      DCHECK(extension);
      if (downloader_->AddExtension(*extension, request_id))
        request.in_progress_ids_.push_back(extension->id());
    }
  }

  // Start a fetch of the blacklist if needed.
  if (params.check_blacklist) {
    ManifestFetchData::PingData ping_data;
    ping_data.rollcall_days =
        CalculatePingDays(extension_prefs_->BlacklistLastPingDay());
    request.in_progress_ids_.push_back(ExtensionDownloader::kBlacklistAppID);
    downloader_->StartBlacklistUpdate(
        prefs_->GetString(kExtensionBlacklistUpdateVersion), ping_data,
        request_id);
  }

  // StartAllPending() might call OnExtensionDownloadFailed/Finished before
  // it returns, which would cause NotifyIfFinished to incorrectly try to
  // send out a notification. So check before we call StartAllPending if any
  // extensions are going to be updated, and use that to figure out if
  // NotifyIfFinished should be called.
  bool noChecks = request.in_progress_ids_.empty();

  // StartAllPending() will call OnExtensionDownloadFailed or
  // OnExtensionDownloadFinished for each extension that was checked.
  downloader_->StartAllPending();


  if (noChecks)
    NotifyIfFinished(request_id);
}

bool ExtensionUpdater::CheckExtensionSoon(const std::string& extension_id,
                                          const FinishedCallback& callback) {
  bool have_throttle_info = ContainsKey(throttle_info_, extension_id);
  ThrottleInfo& info = throttle_info_[extension_id];
  if (have_throttle_info) {
    // We already had a ThrottleInfo object for this extension, check if the
    // update check request should be allowed.

    // If another check is in progress, don't start a new check.
    if (info.in_progress)
      return false;

    Time now = Time::Now();
    Time last = info.check_start;
    // If somehow time moved back, we don't want to infinitely keep throttling.
    if (now < last) {
      last = now;
      info.check_start = now;
    }
    Time earliest = last + TimeDelta::FromSeconds(info.throttle_delay);
    // If check is too soon, throttle.
    if (now < earliest)
      return false;

    // TODO(mek): Somehow increase time between allowing checks when checks
    // are repeatedly throttled and don't result in updates being installed.

    // It's okay to start a check, update values.
    info.check_start = now;
    info.in_progress = true;
  }

  CheckParams params;
  params.ids.push_back(extension_id);
  params.check_blacklist = false;
  params.callback = base::Bind(&ExtensionUpdater::ExtensionCheckFinished,
                               weak_ptr_factory_.GetWeakPtr(),
                               extension_id, callback);
  CheckNow(params);
  return true;
}

void ExtensionUpdater::ExtensionCheckFinished(
    const std::string& extension_id,
    const FinishedCallback& callback) {
  std::map<std::string, ThrottleInfo>::iterator it =
      throttle_info_.find(extension_id);
  if (it != throttle_info_.end()) {
    it->second.in_progress = false;
  }
  callback.Run();
}

void ExtensionUpdater::OnExtensionDownloadFailed(
    const std::string& id,
    Error error,
    const PingResult& ping,
    const std::set<int>& request_ids) {
  DCHECK(alive_);
  UpdatePingData(id, ping);
  bool install_immediately = false;
  for (std::set<int>::const_iterator it = request_ids.begin();
       it != request_ids.end(); ++it) {
    InProgressCheck& request = requests_in_progress_[*it];
    install_immediately |= request.install_immediately;
    request.in_progress_ids_.remove(id);
    NotifyIfFinished(*it);
  }

  // This method is called if no updates were found. However a previous update
  // check might have queued an update for this extension already. If a
  // current update check has |install_immediately| set the previously
  // queued update should be installed now.
  if (install_immediately && service_->GetPendingExtensionUpdate(id))
    service_->FinishDelayedInstallation(id);
}

void ExtensionUpdater::OnExtensionDownloadFinished(
    const std::string& id,
    const base::FilePath& path,
    const GURL& download_url,
    const std::string& version,
    const PingResult& ping,
    const std::set<int>& request_ids) {
  DCHECK(alive_);
  UpdatePingData(id, ping);

  VLOG(2) << download_url << " written to " << path.value();

  FetchedCRXFile fetched(id, path, download_url, request_ids);
  fetched_crx_files_.push(fetched);

  // MaybeInstallCRXFile() removes extensions from |in_progress_ids_| after
  // starting the crx installer.
  MaybeInstallCRXFile();
}

void ExtensionUpdater::OnBlacklistDownloadFinished(
    const std::string& data,
    const std::string& package_hash,
    const std::string& version,
    const PingResult& ping,
    const std::set<int>& request_ids) {
  DCHECK(alive_);
  UpdatePingData(ExtensionDownloader::kBlacklistAppID, ping);
  for (std::set<int>::const_iterator it = request_ids.begin();
       it != request_ids.end(); ++it) {
    InProgressCheck& request = requests_in_progress_[*it];
    request.in_progress_ids_.remove(ExtensionDownloader::kBlacklistAppID);
    NotifyIfFinished(*it);
  }

  // Verify sha256 hash value.
  char sha256_hash_value[crypto::kSHA256Length];
  crypto::SHA256HashString(data, sha256_hash_value, crypto::kSHA256Length);
  std::string hash_in_hex = base::HexEncode(sha256_hash_value,
                                            crypto::kSHA256Length);

  if (package_hash != hash_in_hex) {
    NOTREACHED() << "Fetched blacklist checksum is not as expected. "
        << "Expected: " << package_hash << " Actual: " << hash_in_hex;
    return;
  }
  std::vector<std::string> blacklist;
  base::SplitString(data, '\n', &blacklist);

  blacklist_->SetFromUpdater(blacklist, version);
}

bool ExtensionUpdater::GetPingDataForExtension(
    const std::string& id,
    ManifestFetchData::PingData* ping_data) {
  DCHECK(alive_);
  ping_data->rollcall_days = CalculatePingDays(
      extension_prefs_->LastPingDay(id));
  ping_data->is_enabled = service_->IsExtensionEnabled(id);
  ping_data->active_days =
      CalculateActivePingDays(extension_prefs_->LastActivePingDay(id),
                              extension_prefs_->GetActiveBit(id));
  return true;
}

std::string ExtensionUpdater::GetUpdateUrlData(const std::string& id) {
  DCHECK(alive_);
  return extension_prefs_->GetUpdateUrlData(id);
}

bool ExtensionUpdater::IsExtensionPending(const std::string& id) {
  DCHECK(alive_);
  return service_->pending_extension_manager()->IsIdPending(id);
}

bool ExtensionUpdater::GetExtensionExistingVersion(const std::string& id,
                                                   std::string* version) {
  DCHECK(alive_);
  if (id == ExtensionDownloader::kBlacklistAppID) {
    *version = prefs_->GetString(kExtensionBlacklistUpdateVersion);
    return true;
  }
  const Extension* extension = service_->GetExtensionById(id, true);
  if (!extension)
    return false;
  const Extension* update = service_->GetPendingExtensionUpdate(id);
  if (update)
    *version = update->VersionString();
  else
    *version = extension->VersionString();
  return true;
}

void ExtensionUpdater::UpdatePingData(const std::string& id,
                                      const PingResult& ping_result) {
  DCHECK(alive_);
  if (ping_result.did_ping) {
    if (id == ExtensionDownloader::kBlacklistAppID) {
      extension_prefs_->SetBlacklistLastPingDay(ping_result.day_start);
    } else if (service_->GetExtensionById(id, true) != NULL) {
      extension_prefs_->SetLastPingDay(id, ping_result.day_start);
    }
  }
  if (extension_prefs_->GetActiveBit(id)) {
    extension_prefs_->SetActiveBit(id, false);
    extension_prefs_->SetLastActivePingDay(id, ping_result.day_start);
  }
}

void ExtensionUpdater::MaybeInstallCRXFile() {
  if (crx_install_is_running_ || fetched_crx_files_.empty())
    return;

  std::set<int> request_ids;

  while (!fetched_crx_files_.empty() && !crx_install_is_running_) {
    const FetchedCRXFile& crx_file = fetched_crx_files_.top();

    VLOG(2) << "updating " << crx_file.extension_id
            << " with " << crx_file.path.value();

    // The ExtensionService is now responsible for cleaning up the temp file
    // at |crx_file.path|.
    CrxInstaller* installer = NULL;
    if (service_->UpdateExtension(crx_file.extension_id,
                                  crx_file.path,
                                  crx_file.download_url,
                                  &installer)) {
      crx_install_is_running_ = true;
      current_crx_file_ = crx_file;

      for (std::set<int>::const_iterator it = crx_file.request_ids.begin();
          it != crx_file.request_ids.end(); ++it) {
        InProgressCheck& request = requests_in_progress_[*it];
        if (request.install_immediately) {
          installer->set_install_wait_for_idle(false);
          break;
        }
      }

      // Source parameter ensures that we only see the completion event for the
      // the installer we started.
      registrar_.Add(this,
                     chrome::NOTIFICATION_CRX_INSTALLER_DONE,
                     content::Source<CrxInstaller>(installer));
    } else {
      for (std::set<int>::const_iterator it = crx_file.request_ids.begin();
           it != crx_file.request_ids.end(); ++it) {
        InProgressCheck& request = requests_in_progress_[*it];
        request.in_progress_ids_.remove(crx_file.extension_id);
      }
      request_ids.insert(crx_file.request_ids.begin(),
                         crx_file.request_ids.end());
    }
    fetched_crx_files_.pop();
  }

  for (std::set<int>::const_iterator it = request_ids.begin();
       it != request_ids.end(); ++it) {
    NotifyIfFinished(*it);
  }
}

void ExtensionUpdater::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_CRX_INSTALLER_DONE: {
      // No need to listen for CRX_INSTALLER_DONE anymore.
      registrar_.Remove(this,
                        chrome::NOTIFICATION_CRX_INSTALLER_DONE,
                        source);
      crx_install_is_running_ = false;

      const FetchedCRXFile& crx_file = current_crx_file_;
      for (std::set<int>::const_iterator it = crx_file.request_ids.begin();
          it != crx_file.request_ids.end(); ++it) {
        InProgressCheck& request = requests_in_progress_[*it];
        request.in_progress_ids_.remove(crx_file.extension_id);
        NotifyIfFinished(*it);
      }

      // If any files are available to update, start one.
      MaybeInstallCRXFile();
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_INSTALLED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      if (extension)
        throttle_info_.erase(extension->id());
      break;
    }
    default:
      NOTREACHED();
  }
}

void ExtensionUpdater::NotifyStarted() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_UPDATING_STARTED,
      content::Source<Profile>(profile_),
      content::NotificationService::NoDetails());
}

void ExtensionUpdater::NotifyIfFinished(int request_id) {
  DCHECK(ContainsKey(requests_in_progress_, request_id));
  const InProgressCheck& request = requests_in_progress_[request_id];
  if (request.in_progress_ids_.empty()) {
    VLOG(2) << "Finished update check " << request_id;
    if (!request.callback.is_null())
      request.callback.Run();
    requests_in_progress_.erase(request_id);
  }
}

}  // namespace extensions
