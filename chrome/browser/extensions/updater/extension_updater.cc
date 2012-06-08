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
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/extensions/updater/extension_downloader.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
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
  CheckNow();
  will_check_soon_ = false;
}

void ExtensionUpdater::AddToDownloader(const ExtensionSet* extensions,
    const std::list<std::string>& pending_ids) {
  for (ExtensionSet::const_iterator extension_iter = extensions->begin();
       extension_iter != extensions->end(); ++extension_iter) {
    const Extension& extension = **extension_iter;
    if (!Extension::IsAutoUpdateableLocation(extension.location())) {
      VLOG(2) << "Extension " << extension.id() << " is not auto updateable";
      continue;
    }
    // An extension might be overwritten by policy, and have its update url
    // changed. Make sure existing extensions aren't fetched again, if a
    // pending fetch for an extension with the same id already exists.
    std::list<std::string>::const_iterator pending_id_iter = std::find(
        pending_ids.begin(), pending_ids.end(), extension.id());
    if (pending_id_iter == pending_ids.end()) {
      if (downloader_->AddExtension(extension))
        in_progress_ids_.push_back(extension.id());
    }
  }
}

void ExtensionUpdater::CheckNow() {
  VLOG(2) << "Starting update check";
  DCHECK(alive_);
  NotifyStarted();

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
  pending_extension_manager->GetPendingIdsForUpdateCheck(&pending_ids);

  std::list<std::string>::const_iterator iter;
  for (iter = pending_ids.begin(); iter != pending_ids.end(); ++iter) {
    const PendingExtensionInfo* info = pending_extension_manager->GetById(
        *iter);
    if (!Extension::IsAutoUpdateableLocation(info->install_source())) {
      VLOG(2) << "Extension " << *iter << " is not auto updateable";
      continue;
    }
    if (downloader_->AddPendingExtension(*iter, info->update_url()))
      in_progress_ids_.push_back(*iter);
  }

  AddToDownloader(service_->extensions(), pending_ids);
  AddToDownloader(service_->disabled_extensions(), pending_ids);

  // Start a fetch of the blacklist if needed.
  if (blacklist_checks_enabled_) {
    ManifestFetchData::PingData ping_data;
    ping_data.rollcall_days =
        CalculatePingDays(extension_prefs_->BlacklistLastPingDay());
    downloader_->StartBlacklistUpdate(
        prefs_->GetString(kExtensionBlacklistUpdateVersion), ping_data);
  }

  // StartAllPending() will call OnExtensionUpdateCheckStarted() for each
  // extension that is going to be checked.
  downloader_->StartAllPending();

  NotifyIfFinished();
}

void ExtensionUpdater::OnExtensionDownloadFailed(const std::string& id,
                                                 Error error,
                                                 const PingResult& ping) {
  DCHECK(alive_);
  UpdatePingData(id, ping);
  in_progress_ids_.remove(id);
  NotifyIfFinished();
}

void ExtensionUpdater::OnExtensionDownloadFinished(const std::string& id,
                                                   const FilePath& path,
                                                   const GURL& download_url,
                                                   const std::string& version,
                                                   const PingResult& ping) {
  DCHECK(alive_);
  UpdatePingData(id, ping);

  VLOG(2) << download_url << " written to " << path.value();

  FetchedCRXFile fetched(id, path, download_url);
  fetched_crx_files_.push(fetched);

  // MaybeInstallCRXFile() removes extensions from |in_progress_ids_| after
  // starting the crx installer.
  MaybeInstallCRXFile();
}

void ExtensionUpdater::OnBlacklistDownloadFinished(
    const std::string& data,
    const std::string& package_hash,
    const std::string& version,
    const PingResult& ping) {
  DCHECK(alive_);
  UpdatePingData(ExtensionDownloader::kBlacklistAppID, ping);
  in_progress_ids_.remove(ExtensionDownloader::kBlacklistAppID);
  NotifyIfFinished();

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

  // Tell ExtensionService to update prefs.
  service_->UpdateExtensionBlacklist(blacklist);

  // Update the pref value for blacklist version
  prefs_->SetString(kExtensionBlacklistUpdateVersion, version);
}

bool ExtensionUpdater::GetPingDataForExtension(
    const std::string& id,
    ManifestFetchData::PingData* ping_data) {
  DCHECK(alive_);
  ping_data->rollcall_days = CalculatePingDays(
      extension_prefs_->LastPingDay(id));
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
  *version = extension->version()->GetString();
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

  while (!fetched_crx_files_.empty() && !crx_install_is_running_) {
    const FetchedCRXFile& crx_file = fetched_crx_files_.top();

    VLOG(2) << "updating " << crx_file.id << " with " << crx_file.path.value();

    // The ExtensionService is now responsible for cleaning up the temp file
    // at |crx_file.path|.
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
    in_progress_ids_.remove(crx_file.id);
    fetched_crx_files_.pop();
  }

  NotifyIfFinished();
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

void ExtensionUpdater::NotifyStarted() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_UPDATING_STARTED,
      content::Source<Profile>(profile_),
      content::NotificationService::NoDetails());
}

void ExtensionUpdater::NotifyIfFinished() {
  if (in_progress_ids_.empty()) {
    VLOG(1) << "Sending EXTENSION_UPDATING_FINISHED";
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_UPDATING_FINISHED,
        content::Source<Profile>(profile_),
        content::NotificationService::NoDetails());
  }
}

}  // namespace extensions
