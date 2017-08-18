// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/tab_restore_service_delegate_impl_ios.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/ios/ios_serialized_navigation_builder.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/public/navigation_item.h"
#import "ios/web/web_state/web_state_impl.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TabRestoreServiceDelegateImplIOS::TabRestoreServiceDelegateImplIOS(
    ios::ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {}

TabRestoreServiceDelegateImplIOS::~TabRestoreServiceDelegateImplIOS() {}

TabModel* TabRestoreServiceDelegateImplIOS::tab_model() const {
  return GetLastActiveTabModelForChromeBrowserState(browser_state_);
}

void TabRestoreServiceDelegateImplIOS::ShowBrowserWindow() {
  // No need to do anything here, as the singleton browser "window" is already
  // shown.
}

const SessionID& TabRestoreServiceDelegateImplIOS::GetSessionID() const {
  return session_id_;
}

int TabRestoreServiceDelegateImplIOS::GetTabCount() const {
  return [tab_model() count];
}

int TabRestoreServiceDelegateImplIOS::GetSelectedIndex() const {
  TabModel* tabModel = tab_model();
  return [tabModel indexOfTab:[tabModel currentTab]];
}

std::string TabRestoreServiceDelegateImplIOS::GetAppName() const {
  return std::string();
}

sessions::LiveTab* TabRestoreServiceDelegateImplIOS::GetLiveTabAt(
    int index) const {
  return nullptr;
}

sessions::LiveTab* TabRestoreServiceDelegateImplIOS::GetActiveLiveTab() const {
  return nullptr;
}

bool TabRestoreServiceDelegateImplIOS::IsTabPinned(int index) const {
  // Not supported by iOS.
  return false;
}

const gfx::Rect TabRestoreServiceDelegateImplIOS::GetRestoredBounds() const {
  // Not supported by iOS.
  return gfx::Rect();
}

ui::WindowShowState TabRestoreServiceDelegateImplIOS::GetRestoredState() const {
  // Not supported by iOS.
  return ui::SHOW_STATE_NORMAL;
}

std::string TabRestoreServiceDelegateImplIOS::GetWorkspace() const {
  // Not supported by iOS.
  return std::string();
}

sessions::LiveTab* TabRestoreServiceDelegateImplIOS::AddRestoredTab(
    const std::vector<sessions::SerializedNavigationEntry>& navigations,
    int tab_index,
    int selected_navigation,
    const std::string& extension_app_id,
    bool select,
    bool pin,
    bool from_last_session,
    const sessions::PlatformSpecificTabData* tab_platform_data,
    const std::string& user_agent_override) {
  DCHECK_LT(selected_navigation, static_cast<int>(navigations.size()));

  web::WebState::CreateParams params(browser_state_);
  std::unique_ptr<web::WebStateImpl> web_state(new web::WebStateImpl(params));
  std::vector<std::unique_ptr<web::NavigationItem>> items =
      sessions::IOSSerializedNavigationBuilder::ToNavigationItems(navigations);
  web_state->GetNavigationManagerImpl().ReplaceSessionHistory(
      std::move(items), selected_navigation);

  // TODO(crbug.com/661636): Handle tab-switch animation somehow...
  WebStateList* web_state_list = [tab_model() webStateList];
  web_state_list->InsertWebState(
      tab_index, std::move(web_state),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener());
  return nullptr;
}

sessions::LiveTab* TabRestoreServiceDelegateImplIOS::ReplaceRestoredTab(
    const std::vector<sessions::SerializedNavigationEntry>& navigations,
    int selected_navigation,
    bool from_last_session,
    const std::string& extension_app_id,
    const sessions::PlatformSpecificTabData* tab_platform_data,
    const std::string& user_agent_override) {
  DCHECK_LT(selected_navigation, static_cast<int>(navigations.size()));

  // Desktop Chrome creates a new tab and deletes the old one. The net result
  // is that the NTP is not in the session history of the restored tab. Do
  // the same by replacing the tab's navigation history which will also force it
  // to reload.
  Tab* tab = tab_model().currentTab;
  [tab replaceHistoryWithNavigations:navigations
                        currentIndex:selected_navigation];
  return nullptr;
}

void TabRestoreServiceDelegateImplIOS::CloseTab() {
  WebStateList* web_state_list = [tab_model() webStateList];
  web_state_list->CloseWebStateAt(web_state_list->active_index());
}
