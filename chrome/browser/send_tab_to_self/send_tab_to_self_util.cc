// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync/device_info/device_info.h"
#include "components/sync/device_info/device_info_sync_service.h"
#include "components/sync/device_info/device_info_tracker.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace send_tab_to_self {

bool IsReceivingEnabled() {
  return base::FeatureList::IsEnabled(switches::kSyncSendTabToSelf);
}

bool IsSendingEnabled() {
  return IsReceivingEnabled() &&
         base::FeatureList::IsEnabled(kSendTabToSelfShowSendingUI);
}

bool IsUserSyncTypeActive(Profile* profile) {
  SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile);
  // The service will be null if the user is in incognito mode so better to
  // check for that.
  return service && service->GetSendTabToSelfModel() &&
         service->GetSendTabToSelfModel()->IsReady();
}

bool IsSyncingOnMultipleDevices(Profile* profile) {
  syncer::DeviceInfoSyncService* device_sync_service =
      DeviceInfoSyncServiceFactory::GetForProfile(profile);

  return device_sync_service && device_sync_service->GetDeviceInfoTracker() &&
         device_sync_service->GetDeviceInfoTracker()->CountActiveDevices() > 1;
}

bool IsContentRequirementsMet(const GURL& url, Profile* profile) {
  bool is_http_or_https = url.SchemeIsHTTPOrHTTPS();
  bool is_native_page = url.SchemeIs(content::kChromeUIScheme);
  bool is_incognito_mode =
      profile->GetProfileType() == Profile::INCOGNITO_PROFILE;
  return is_http_or_https && !is_native_page && !is_incognito_mode;
}

bool ShouldOfferFeature(content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // If sending is enabled, then so is receiving.
  return IsSendingEnabled() && IsUserSyncTypeActive(profile) &&
         IsSyncingOnMultipleDevices(profile) &&
         IsContentRequirementsMet(web_contents->GetURL(), profile);
}

bool ShouldOfferFeatureForLink(content::WebContents* web_contents,
                               const GURL& link_url) {
  if (!web_contents)
    return false;
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  return IsSendingEnabled() && IsUserSyncTypeActive(profile) &&
         IsSyncingOnMultipleDevices(profile) &&
         (IsContentRequirementsMet(web_contents->GetURL(), profile) ||
          IsContentRequirementsMet(link_url, profile));
}

}  // namespace send_tab_to_self
