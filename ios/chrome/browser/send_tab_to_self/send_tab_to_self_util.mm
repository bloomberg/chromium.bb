// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/send_tab_to_self/send_tab_to_self_util.h"

#include "base/strings/utf_string_conversions.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/send_tab_to_self/send_tab_to_self_client_service_factory.h"
#include "ios/chrome/browser/sync/device_info_sync_service_factory.h"
#include "ios/chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/web_state/web_state.h"

#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace send_tab_to_self {

bool IsReceivingEnabled() {
  return base::FeatureList::IsEnabled(switches::kSyncSendTabToSelf);
}

bool IsSendingEnabled() {
  return IsReceivingEnabled() &&
         base::FeatureList::IsEnabled(kSendTabToSelfShowSendingUI);
}

bool IsUserSyncTypeActive(ios::ChromeBrowserState* browser_state) {
  SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForBrowserState(browser_state);
  // The service will be null if the user is in incognito mode so better to
  // check for that.
  return service && service->GetSendTabToSelfModel() &&
         service->GetSendTabToSelfModel()->IsReady();
}

bool IsSyncingOnMultipleDevices(ios::ChromeBrowserState* browser_state) {
  syncer::DeviceInfoSyncService* device_sync_service =
      DeviceInfoSyncServiceFactory::GetForBrowserState(browser_state);
  return device_sync_service && device_sync_service->GetDeviceInfoTracker() &&
         device_sync_service->GetDeviceInfoTracker()->CountActiveDevices() > 1;
}

bool IsContentRequirementsMet(const GURL& url,
                              ios::ChromeBrowserState* browser_state) {
  bool is_http_or_https = url.SchemeIsHTTPOrHTTPS();
  bool is_native_page = url.SchemeIs(kChromeUIScheme);
  bool is_incognito_mode = browser_state->IsOffTheRecord();
  return is_http_or_https && !is_native_page && !is_incognito_mode;
}

bool ShouldOfferFeature() {
  ios::ChromeBrowserStateManager* browser_state_manager =
      GetApplicationContext()->GetChromeBrowserStateManager();
  if (!browser_state_manager) {
    // Can happen in tests.
    return false;
  }

  ios::ChromeBrowserState* browser_state =
      browser_state_manager->GetLastUsedBrowserState();
  if (!browser_state) {
    return false;
  }

  // If there is no web state or it is not visible then nothing should be
  // shared.
  TabModel* tab_model =
      TabModelList::GetLastActiveTabModelForChromeBrowserState(browser_state);
  if (!tab_model) {
    return false;
  }

  WebStateList* web_state_list = tab_model.webStateList;
  if (!web_state_list) {
    return false;
  }

  web::WebState* web_state = web_state_list->GetActiveWebState();

  // If sending is enabled, then so is receiving.
  return IsSendingEnabled() && IsUserSyncTypeActive(browser_state) &&
         IsSyncingOnMultipleDevices(browser_state) &&
         IsContentRequirementsMet(web_state->GetVisibleURL(), browser_state);
}

void CreateNewEntry(ios::ChromeBrowserState* browser_state) {
  // If there is no web state or it is not visible then nothing should be
  // shared.
  TabModel* tab_model =
      TabModelList::GetLastActiveTabModelForChromeBrowserState(browser_state);

  WebStateList* web_state_list = tab_model.webStateList;
  if (!web_state_list) {
    return;
  }

  web::WebState* web_state = web_state_list->GetActiveWebState();
  if (!web_state) {
    return;
  }

  web::NavigationItem* cur_item =
      web_state->GetNavigationManager()->GetLastCommittedItem();
  if (!cur_item) {
    return;
  }

  GURL url = cur_item->GetURL();

  std::string title = base::UTF16ToUTF8(cur_item->GetTitle());

  base::Time navigation_time = cur_item->GetTimestamp();
  // TODO(crbug.com/946804) Add target device.
  std::string target_device;

  SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForBrowserState(browser_state)
          ->GetSendTabToSelfModel();

  model->AddEntry(url, title, navigation_time, target_device);
}

}  // namespace send_tab_to_self
