// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/extension_updater.h"

#include <stdint.h>

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/module/module.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/update_query_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "crypto/sha2.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/extension_updater_uma.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

using base::RandDouble;
using base::RandInt;
using base::Time;
using base::TimeDelta;
using content::BrowserThread;

typedef extensions::ExtensionDownloaderDelegate::Error Error;
typedef extensions::ExtensionDownloaderDelegate::PingResult PingResult;

namespace {

// Wait at least 60 seconds after browser startup before we do any checks. If
// you change this value, make sure to update comments where it is used.
const int kStartupWaitSeconds = 60;

// The minimum number of seconds there should be for the delay passed to
// ScheduleNextCheck.
const int kScheduleNextCheckMinGapSecs = 1;

// For sanity checking on update frequency - enforced in release mode only.
#if defined(NDEBUG)
const int kMinUpdateFrequencySeconds = 30;
#endif
const int kMaxUpdateFrequencySeconds = 60 * 60 * 24 * 7;  // 7 days

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
    : install_immediately(false),
      fetch_priority(ManifestFetchData::FetchPriority::BACKGROUND) {}

ExtensionUpdater::CheckParams::~CheckParams() {}

ExtensionUpdater::FetchedCRXFile::FetchedCRXFile(
    const CRXFileInfo& file,
    bool file_ownership_passed,
    const std::set<int>& request_ids,
    const InstallCallback& callback)
    : info(file),
      file_ownership_passed(file_ownership_passed),
      request_ids(request_ids),
      callback(callback) {
}

ExtensionUpdater::FetchedCRXFile::FetchedCRXFile()
    : file_ownership_passed(true) {
}

ExtensionUpdater::FetchedCRXFile::FetchedCRXFile(const FetchedCRXFile& other) =
    default;

ExtensionUpdater::FetchedCRXFile::~FetchedCRXFile() {}

ExtensionUpdater::InProgressCheck::InProgressCheck()
    : install_immediately(false) {}

ExtensionUpdater::InProgressCheck::InProgressCheck(
    const InProgressCheck& other) = default;

ExtensionUpdater::InProgressCheck::~InProgressCheck() {}

ExtensionUpdater::ExtensionUpdater(
    ExtensionServiceInterface* service,
    ExtensionPrefs* extension_prefs,
    PrefService* prefs,
    Profile* profile,
    int frequency_seconds,
    ExtensionCache* cache,
    const ExtensionDownloader::Factory& downloader_factory)
    : alive_(false),
      service_(service),
      downloader_factory_(downloader_factory),
      frequency_seconds_(frequency_seconds),
      will_check_soon_(false),
      extension_prefs_(extension_prefs),
      prefs_(prefs),
      profile_(profile),
      next_request_id_(0),
      crx_install_is_running_(false),
      extension_cache_(cache),
      weak_ptr_factory_(this) {
  DCHECK_GE(frequency_seconds_, 5);
  DCHECK_LE(frequency_seconds_, kMaxUpdateFrequencySeconds);
#if defined(NDEBUG)
  // In Release mode we enforce that update checks don't happen too often.
  frequency_seconds_ = std::max(frequency_seconds_, kMinUpdateFrequencySeconds);
#endif
  frequency_seconds_ = std::min(frequency_seconds_, kMaxUpdateFrequencySeconds);
}

ExtensionUpdater::~ExtensionUpdater() {
  Stop();
}

void ExtensionUpdater::EnsureDownloaderCreated() {
  if (!downloader_.get()) {
    downloader_ = downloader_factory_.Run(this);
  }
}

// The overall goal here is to balance keeping clients up to date while
// avoiding a thundering herd against update servers.
TimeDelta ExtensionUpdater::DetermineFirstCheckDelay() {
  DCHECK(alive_);
  // If someone's testing with a quick frequency, just allow it.
  if (frequency_seconds_ < kStartupWaitSeconds)
    return TimeDelta::FromSeconds(frequency_seconds_);

  // If we've never scheduled a check before, start at a random time up to
  // frequency_seconds_ away.
  if (!prefs_->HasPrefPath(pref_names::kNextUpdateCheck))
    return TimeDelta::FromSeconds(
        RandInt(kStartupWaitSeconds, frequency_seconds_));

  // Read the persisted next check time, and use that if it isn't in the past
  // or too far in the future (this can happen with system clock changes).
  Time saved_next = Time::FromInternalValue(prefs_->GetInt64(
      pref_names::kNextUpdateCheck));
  Time now = Time::Now();
  base::Time earliest =
      now + TimeDelta::FromSeconds(kScheduleNextCheckMinGapSecs);
  base::Time latest = now + TimeDelta::FromSeconds(frequency_seconds_);
  if (saved_next > earliest && saved_next < latest) {
    return saved_next - now;
  }

  // In most cases we'll get here because the persisted next check time passed
  // while we weren't running, so pick something soon.
  return TimeDelta::FromSeconds(
      RandInt(kStartupWaitSeconds, kStartupWaitSeconds * 5));
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
  DCHECK(target_delay >= TimeDelta::FromSeconds(kScheduleNextCheckMinGapSecs));

  // Add +/- 10% random jitter.
  double delay_ms = target_delay.InMillisecondsF();
  double jitter_factor = (RandDouble() * .2) - 0.1;
  delay_ms += delay_ms * jitter_factor;
  TimeDelta actual_delay =
      TimeDelta::FromMilliseconds(static_cast<int64_t>(delay_ms));

  // Save the time of next check.
  Time next = Time::Now() + actual_delay;
  prefs_->SetInt64(pref_names::kNextUpdateCheck, next.ToInternalValue());

  timer_.Start(FROM_HERE, actual_delay, this, &ExtensionUpdater::TimerFired);
}

void ExtensionUpdater::TimerFired() {
  DCHECK(alive_);
  CheckNow(CheckParams());

  // If the user has overridden the update frequency, don't bother reporting
  // this.
  if (frequency_seconds_ == extensions::kDefaultUpdateFrequencySeconds) {
    Time last = Time::FromInternalValue(prefs_->GetInt64(
        pref_names::kLastUpdateCheck));
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
  int64_t now = Time::Now().ToInternalValue();
  prefs_->SetInt64(pref_names::kLastUpdateCheck, now);
  ScheduleNextCheck(TimeDelta::FromSeconds(frequency_seconds_));
}

void ExtensionUpdater::CheckSoon() {
  DCHECK(alive_);
  if (will_check_soon_)
    return;
  if (BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              base::BindOnce(&ExtensionUpdater::DoCheckSoon,
                                             weak_ptr_factory_.GetWeakPtr()))) {
    will_check_soon_ = true;
  } else {
    NOTREACHED();
  }
}

bool ExtensionUpdater::WillCheckSoon() const {
  return will_check_soon_;
}

void ExtensionUpdater::SetExtensionCacheForTesting(
    ExtensionCache* extension_cache) {
  extension_cache_ = extension_cache;
}

void ExtensionUpdater::StopTimerForTesting() {
  timer_.Stop();
}

void ExtensionUpdater::DoCheckSoon() {
  DCHECK(will_check_soon_);
  CheckNow(CheckParams());
  will_check_soon_ = false;
}

void ExtensionUpdater::AddToDownloader(
    const ExtensionSet* extensions,
    const std::list<std::string>& pending_ids,
    int request_id,
    ManifestFetchData::FetchPriority fetch_priority) {
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
    if (!base::ContainsValue(pending_ids, extension.id()) &&
        downloader_->AddExtension(extension, request_id, fetch_priority)) {
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

  EnsureDownloaderCreated();

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
      if (downloader_->AddPendingExtension(
              *iter, info->update_url(), info->install_source(),
              pending_extension_manager->IsPolicyReinstallForCorruptionExpected(
                  *iter),
              request_id, params.fetch_priority))
        request.in_progress_ids_.push_back(*iter);
    }

    ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
    AddToDownloader(&registry->enabled_extensions(), pending_ids, request_id,
                    params.fetch_priority);
    AddToDownloader(&registry->disabled_extensions(), pending_ids, request_id,
                    params.fetch_priority);
  } else {
    for (std::list<std::string>::const_iterator it = params.ids.begin();
         it != params.ids.end(); ++it) {
      const Extension* extension = service_->GetExtensionById(*it, true);
      if (extension && downloader_->AddExtension(*extension, request_id,
                                                 params.fetch_priority))
        request.in_progress_ids_.push_back(extension->id());
    }
  }

  // StartAllPending() might call OnExtensionDownloadFailed/Finished before
  // it returns, which would cause NotifyIfFinished to incorrectly try to
  // send out a notification. So check before we call StartAllPending if any
  // extensions are going to be updated, and use that to figure out if
  // NotifyIfFinished should be called.
  bool noChecks = request.in_progress_ids_.empty();

  // StartAllPending() will call OnExtensionDownloadFailed or
  // OnExtensionDownloadFinished for each extension that was checked.
  downloader_->StartAllPending(extension_cache_);

  if (noChecks)
    NotifyIfFinished(request_id);
}

void ExtensionUpdater::CheckExtensionSoon(const std::string& extension_id,
                                          const FinishedCallback& callback) {
  CheckParams params;
  params.ids.push_back(extension_id);
  params.callback = callback;
  CheckNow(params);
}

void ExtensionUpdater::OnExtensionDownloadFailed(
    const std::string& id,
    Error error,
    const PingResult& ping,
    const std::set<int>& request_ids) {
  DCHECK(alive_);

  switch (error) {
    case Error::CRX_FETCH_FAILED:
    case Error::MANIFEST_FETCH_FAILED:
    case Error::MANIFEST_INVALID:
      UMA_HISTOGRAM_ENUMERATION(
          "Extensions.ExtensionUpdaterUpdateResults",
          ExtensionUpdaterUpdateResult::UPDATE_ERROR,
          ExtensionUpdaterUpdateResult::UPDATE_RESULT_COUNT);
      break;
    case Error::NO_UPDATE_AVAILABLE:
      UMA_HISTOGRAM_ENUMERATION(
          "Extensions.ExtensionUpdaterUpdateResults",
          ExtensionUpdaterUpdateResult::NO_UPDATE,
          ExtensionUpdaterUpdateResult::UPDATE_RESULT_COUNT);
      break;
    case Error::DISABLED:
      // Error::DISABLED corresponds to the browser having disabled extension
      // updates, the extension updater does not actually run when this error
      // code is emitted. For this reason, Error::DISABLED is not included in
      // Extensions.ExtensionUpdaterUpdateResults UMA; we are only interested
      // in the update results when the extension updater runs.
      break;
  }

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
    service_->FinishDelayedInstallationIfReady(id, install_immediately);
}

void ExtensionUpdater::OnExtensionDownloadFinished(
    const CRXFileInfo& file,
    bool file_ownership_passed,
    const GURL& download_url,
    const std::string& version,
    const PingResult& ping,
    const std::set<int>& request_ids,
    const InstallCallback& callback) {
  DCHECK(alive_);
  UpdatePingData(file.extension_id, ping);

  VLOG(2) << download_url << " written to " << file.path.value();

  FetchedCRXFile fetched(file, file_ownership_passed, request_ids, callback);
  fetched_crx_files_.push(fetched);

  // MaybeInstallCRXFile() removes extensions from |in_progress_ids_| after
  // starting the crx installer.
  MaybeInstallCRXFile();
}

bool ExtensionUpdater::GetPingDataForExtension(
    const std::string& id,
    ManifestFetchData::PingData* ping_data) {
  DCHECK(alive_);
  ping_data->rollcall_days = CalculatePingDays(
      extension_prefs_->LastPingDay(id));
  ping_data->is_enabled = service_->IsExtensionEnabled(id);
  if (!ping_data->is_enabled)
    ping_data->disable_reasons = extension_prefs_->GetDisableReasons(id);
  ping_data->active_days =
      CalculateActivePingDays(extension_prefs_->LastActivePingDay(id),
                              extension_prefs_->GetActiveBit(id));
  return true;
}

std::string ExtensionUpdater::GetUpdateUrlData(const std::string& id) {
  DCHECK(alive_);
  return extension::GetUpdateURLData(extension_prefs_, id);
}

bool ExtensionUpdater::IsExtensionPending(const std::string& id) {
  DCHECK(alive_);
  return service_->pending_extension_manager()->IsIdPending(id);
}

bool ExtensionUpdater::GetExtensionExistingVersion(const std::string& id,
                                                   std::string* version) {
  DCHECK(alive_);
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
  if (ping_result.did_ping)
    extension_prefs_->SetLastPingDay(id, ping_result.day_start);
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

    VLOG(2) << "updating " << crx_file.info.extension_id
            << " with " << crx_file.info.path.value();

    // The ExtensionService is now responsible for cleaning up the temp file
    // at |crx_file.info.path|.
    CrxInstaller* installer = NULL;
    if (service_->UpdateExtension(crx_file.info,
                                  crx_file.file_ownership_passed,
                                  &installer)) {
      crx_install_is_running_ = true;
      current_crx_file_ = crx_file;

      for (std::set<int>::const_iterator it = crx_file.request_ids.begin();
          it != crx_file.request_ids.end(); ++it) {
        InProgressCheck& request = requests_in_progress_[*it];
        if (request.install_immediately) {
          installer->set_install_immediately(true);
          break;
        }
      }

      // Source parameter ensures that we only see the completion event for the
      // the installer we started.
      registrar_.Add(this,
                     extensions::NOTIFICATION_CRX_INSTALLER_DONE,
                     content::Source<CrxInstaller>(installer));
    } else {
      for (std::set<int>::const_iterator it = crx_file.request_ids.begin();
           it != crx_file.request_ids.end(); ++it) {
        InProgressCheck& request = requests_in_progress_[*it];
        request.in_progress_ids_.remove(crx_file.info.extension_id);
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
  DCHECK_EQ(extensions::NOTIFICATION_CRX_INSTALLER_DONE, type);

  registrar_.Remove(this, extensions::NOTIFICATION_CRX_INSTALLER_DONE, source);
  crx_install_is_running_ = false;

  // If installing this file didn't succeed, we may need to re-download it.
  const Extension* extension = content::Details<const Extension>(details).ptr();

  UMA_HISTOGRAM_ENUMERATION("Extensions.ExtensionUpdaterUpdateResults",
                            extension
                                ? ExtensionUpdaterUpdateResult::UPDATE_SUCCESS
                                : ExtensionUpdaterUpdateResult::UPDATE_ERROR,
                            ExtensionUpdaterUpdateResult::UPDATE_RESULT_COUNT);

  extensions::CrxInstaller* installer =
      content::Source<extensions::CrxInstaller>(source).ptr();
  const FetchedCRXFile& crx_file = current_crx_file_;
  if (!extension && installer->hash_check_failed() &&
      !crx_file.callback.is_null()) {
    // If extension downloader asked us to notify it about failed installations,
    // it will resume a pending download from the manifest data it has already
    // fetched and call us again with the same request_id's (with either
    // OnExtensionDownloadFailed or OnExtensionDownloadFinished). For that
    // reason we don't notify finished requests yet.
    crx_file.callback.Run(true);
  } else {
    for (std::set<int>::const_iterator it = crx_file.request_ids.begin();
         it != crx_file.request_ids.end(); ++it) {
      InProgressCheck& request = requests_in_progress_[*it];
      request.in_progress_ids_.remove(crx_file.info.extension_id);
      NotifyIfFinished(*it);
    }
    if (!crx_file.callback.is_null()) {
      crx_file.callback.Run(false);
    }
  }

  // If any files are available to update, start one.
  MaybeInstallCRXFile();
}

void ExtensionUpdater::NotifyStarted() {
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_UPDATING_STARTED,
      content::Source<Profile>(profile_),
      content::NotificationService::NoDetails());
}

void ExtensionUpdater::NotifyIfFinished(int request_id) {
  DCHECK(base::ContainsKey(requests_in_progress_, request_id));
  const InProgressCheck& request = requests_in_progress_[request_id];
  if (request.in_progress_ids_.empty()) {
    VLOG(2) << "Finished update check " << request_id;
    if (!request.callback.is_null())
      request.callback.Run();
    requests_in_progress_.erase(request_id);
  }
}

}  // namespace extensions
