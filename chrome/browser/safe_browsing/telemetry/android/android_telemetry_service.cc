// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/telemetry/android/android_telemetry_service.h"

#include <algorithm>

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/download/public/common/download_item.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/triggers/trigger_manager.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"

namespace safe_browsing {

namespace {
// MIME-type for APKs.
const char kApkMimeType[] = "application/vnd.android.package-archive";

bool IsFeatureEnabled() {
  return base::FeatureList::IsEnabled(safe_browsing::kTelemetryForApkDownloads);
}
}  // namespace

AndroidTelemetryService::AndroidTelemetryService(
    SafeBrowsingService* sb_service,
    Profile* profile)
    : TelemetryService(),
      profile_(profile),
      sb_service_(sb_service),
      trigger_manager_(sb_service->trigger_manager()) {
  DCHECK(profile_);
  DCHECK(IsSafeBrowsingEnabled());
  if (!IsFeatureEnabled()) {
    return;
  }

  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(profile_);
  if (download_manager) {
    // Look for new downloads being created.
    download_manager->AddObserver(this);
  }
}

AndroidTelemetryService::~AndroidTelemetryService() {
  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(profile_);
  if (download_manager) {
    download_manager->RemoveObserver(this);
  }
}

void AndroidTelemetryService::OnDownloadCreated(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  DCHECK(IsFeatureEnabled());

  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(item);
  if (!web_contents) {
    // TODO(vakh): This can happen sometimes on a browser launch. Identify this
    // case and document it better.
    return;
  }

  if (item->GetMimeType() != kApkMimeType) {
    return;
  }

  item->AddObserver(this);
  StartThreatDetailsCollection(web_contents);
}

void AndroidTelemetryService::OnDownloadUpdated(download::DownloadItem* item) {
  DCHECK(IsFeatureEnabled());
  DCHECK_EQ(kApkMimeType, item->GetMimeType());

  if (item->GetState() == download::DownloadItem::COMPLETE) {
    // Download completed. Send report, if allowed.
    content::WebContents* web_contents =
        content::DownloadItemUtils::GetWebContents(item);
    FinishCollectingThreatDetails(web_contents);
  }
}

const PrefService* AndroidTelemetryService::GetPrefs() {
  return profile_->GetPrefs();
}

bool AndroidTelemetryService::IsSafeBrowsingEnabled() {
  return GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled);
}

void AndroidTelemetryService::StartThreatDetailsCollection(
    content::WebContents* web_contents) {
  security_interstitials::UnsafeResource resource;
  resource.threat_type = SB_THREAT_TYPE_APK_DOWNLOAD;
  resource.url = web_contents->GetLastCommittedURL();
  resource.web_contents_getter = resource.GetWebContentsGetter(
      web_contents->GetMainFrame()->GetProcess()->GetID(),
      web_contents->GetMainFrame()->GetRoutingID());

  TriggerManagerReason reason;
  // Ignores the return of |StartCollectingThreatDetails()| here and
  // let TriggerManager decide whether it should start data
  // collection.
  trigger_manager_->StartCollectingThreatDetailsWithReason(
      safe_browsing::TriggerType::APK_DOWNLOAD, web_contents, resource,
      sb_service_->GetURLLoaderFactory(),
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      TriggerManager::GetSBErrorDisplayOptions(*GetPrefs(), web_contents),
      &reason);
  // TODO(vakh): Log |reason|.
}

void AndroidTelemetryService::FinishCollectingThreatDetails(
    content::WebContents* web_contents) {
  trigger_manager_->FinishCollectingThreatDetails(
      safe_browsing::TriggerType::APK_DOWNLOAD, web_contents,
      base::TimeDelta::FromMilliseconds(0), /*did_proceed=*/false,
      /*num_visit=*/0,
      TriggerManager::GetSBErrorDisplayOptions(*GetPrefs(), web_contents));
}

}  // namespace safe_browsing
