// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TELEMETRY_ANDROID_ANDROID_TELEMETRY_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_TELEMETRY_ANDROID_ANDROID_TELEMETRY_SERVICE_H_

#include <vector>

#include "chrome/browser/safe_browsing/telemetry/telemetry_service.h"
#include "components/download/public/common/download_item.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "content/public/browser/download_manager.h"

class Profile;
class PrefService;

namespace content {
class WebContents;
}

namespace safe_browsing {

class SafeBrowsingService;
class TriggerManager;

// This class is used to send telemetry information to Safe Browsing for
// security related incidents. The information is sent only if:
// 1. The user has opted in to extended reporting, AND
// 2. The security incident did not happen in an incognito window.
// As the name suggests, this works only on Android.

// The following events are currently considered security related incidents from
// the perspective of this class:
// 1. Downloading off-market APKs. See: go/zurkon-v1-referrer-dd

class AndroidTelemetryService : public download::DownloadItem::Observer,
                                public content::DownloadManager::Observer,
                                public TelemetryService {
 public:
  AndroidTelemetryService(SafeBrowsingService* sb_service, Profile* profile);
  ~AndroidTelemetryService() override;

  // content::DownloadManager::Observer.
  // Called when a new download is created.
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;

  // download::DownloadItem::Observer
  // Called when the state of a download item changes.
  void OnDownloadUpdated(download::DownloadItem* download) override;

 private:
  // Calls into |trigger_manager_| to start collecting information about the
  // download event. This information is then sent to Safe Browsing service
  // in the form of a |ClientSafeBrowsingReportRequest| message.
  // If the download gets cancelled, the report isn't sent.
  void StartThreatDetailsCollection(content::WebContents* web_contents);

  // Calls into |trigger_manager_| to prepare to send the referrer chain and
  // other information to Safe Browsing service. Only called when the download
  // completes and only for users who have opted into extended reporting.
  void FinishCollectingThreatDetails(content::WebContents* web_contents);

  // Helper method to get prefs from |profile_|.
  const PrefService* GetPrefs();

  // Helper method to check if Safe Browsing is enabled.
  bool IsSafeBrowsingEnabled();

  // Profile associated with this instance. Unowned.
  Profile* profile_;

  // Unowned.
  SafeBrowsingService* sb_service_;

  // Used to send the ClientSafeBrowsingReportRequest report. Unowned.
  TriggerManager* trigger_manager_;

  DISALLOW_COPY_AND_ASSIGN(AndroidTelemetryService);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_TELEMETRY_ANDROID_ANDROID_TELEMETRY_SERVICE_H_
