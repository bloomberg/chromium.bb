// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TELEMETRY_ANDROID_ANDROID_TELEMETRY_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_TELEMETRY_ANDROID_ANDROID_TELEMETRY_SERVICE_H_

#include <string>

#include "base/memory/weak_ptr.h"
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
  // Whether the ping can be sent, based on empty web_contents, or incognito
  // mode, or extended reporting opt-in status,
  bool CanSendPing(download::DownloadItem* item);

  // Fill the referrer chain in |report| with the actual referrer chain for the
  // given |web_contents|, as well as recent navigations.
  void FillReferrerChain(content::WebContents* web_contents,
                         ClientSafeBrowsingReportRequest* report);

  // If |safety_net_id_on_ui_thread_| isn't already set, post a task on the IO
  // thread to get the safety net ID of the device and then store that value on
  // the UI thread in |safety_net_id_on_ui_thread_|.
  void MaybeCaptureSafetyNetId();

  // Sets the relevant fields in an instance of
  // |ClientSafeBrowsingReportRequest| and sends it to the Safe Browsing
  // backend. The report may not be sent if the proto fails to serialize.
  void MaybeSendApkDownloadReport(download::DownloadItem* item);

  // Gets called on the UI thread when the |safety_net_id| of the device has
  // been captured. Sets |safety_net_id_on_ui_thread_| to the captured value.
  void SetSafetyNetIdOnUIThread(const std::string& safety_net_id);

  // Helper method to get prefs from |profile_|.
  const PrefService* GetPrefs();

  // Helper method to check if Safe Browsing is enabled.
  bool IsSafeBrowsingEnabled();

  // Profile associated with this instance. Unowned.
  Profile* profile_;

  std::string safety_net_id_on_ui_thread_;

  // Unowned.
  SafeBrowsingService* sb_service_;

  base::WeakPtrFactory<AndroidTelemetryService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AndroidTelemetryService);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_TELEMETRY_ANDROID_ANDROID_TELEMETRY_SERVICE_H_
