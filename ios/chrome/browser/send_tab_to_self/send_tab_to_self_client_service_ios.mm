// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/send_tab_to_self/send_tab_to_self_client_service_ios.h"

#include <memory>
#include <string>
#include <vector>

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/infobar_utils.h"
#include "ios/chrome/browser/send_tab_to_self/ios_send_tab_to_self_infobar_delegate.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace send_tab_to_self {

SendTabToSelfClientServiceIOS::SendTabToSelfClientServiceIOS(
    ios::ChromeBrowserState* browser_state,
    SendTabToSelfModel* model)
    : model_(model), browser_state_(browser_state) {
  model_->AddObserver(this);
}

SendTabToSelfClientServiceIOS::~SendTabToSelfClientServiceIOS() {
  model_->RemoveObserver(this);
  model_ = nullptr;
}

void SendTabToSelfClientServiceIOS::SendTabToSelfModelLoaded() {
  // TODO(crbug.com/949756): Push changes that happened before the model was
  // loaded.
}

void SendTabToSelfClientServiceIOS::EntriesAddedRemotely(
    const std::vector<const SendTabToSelfEntry*>& new_entries) {
  // TODO(crbug.com/953513): Use utils file instead.
  if (!base::FeatureList::IsEnabled(switches::kSyncSendTabToSelf)) {
    return;
  }

  if (new_entries.empty()) {
    return;
  }

  TabModel* tab_model =
      TabModelList::GetLastActiveTabModelForChromeBrowserState(browser_state_);
  if (!tab_model) {
    return;
  }

  WebStateList* web_state_list = tab_model.webStateList;
  if (!web_state_list) {
    return;
  }

  // If there is no web state or it is not visible, we cannot add an infobar to
  // it.
  // TODO(crbug.com/944602): Wait until a web state is active and add the info
  // bar then.
  web::WebState* web_state = web_state_list->GetActiveWebState();
  if (!web_state || !web_state->IsVisible()) {
    NOTIMPLEMENTED();
    return;
  }

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);

  // Since we can only show one infobar at the time, pick the most recent entry.
  // TODO(crbug.com/944602): Create a function that returns the most recently
  // shared entry.
  infobar_manager->AddInfoBar(CreateConfirmInfoBar(
      IOSSendTabToSelfInfoBarDelegate::Create(new_entries.back())));
}

void SendTabToSelfClientServiceIOS::EntriesRemovedRemotely(
    const std::vector<std::string>& guids) {
  if (!base::FeatureList::IsEnabled(switches::kSyncSendTabToSelf)) {
    return;
  }
  NOTIMPLEMENTED();
}

}  // namespace send_tab_to_self
