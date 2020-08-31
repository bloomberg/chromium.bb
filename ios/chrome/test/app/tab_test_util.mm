// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/tab_test_util.h"

#import <Foundation/Foundation.h>

#import "base/mac/foundation_util.h"
#import "ios/chrome/app/main_controller_private.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/metrics/tab_usage_recorder_browser_agent.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_title_util.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/main/scene_controller.h"
#import "ios/chrome/browser/ui/main/scene_controller_testing.h"
#import "ios/chrome/browser/ui/tab_grid/tab_switcher.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/chrome/test/app/chrome_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

namespace {

// Returns the tab model for the current mode (incognito or normal).
TabModel* GetCurrentTabModel() {
  return GetMainController().interfaceProvider.currentInterface.tabModel;
}

// Returns the WebStateList for the current mode. Or nullptr of there is no
// tabModel.
WebStateList* GetCurrentWebStateList() {
  TabModel* tab_model = GetCurrentTabModel();
  return tab_model ? tab_model.webStateList : nullptr;
}

// Returns the browser for the current mode.
Browser* GetCurrentBrowser() {
  return GetMainController().interfaceProvider.currentInterface.browser;
}

}  // namespace

BOOL IsIncognitoMode() {
  return GetMainController().interfaceProvider.currentInterface.incognito;
}

void OpenNewTab() {
  @autoreleasepool {  // Make sure that all internals are deallocated.
    OpenNewTabCommand* command = [OpenNewTabCommand command];
    if (GetForegroundActiveSceneController().isTabSwitcherActive) {
      // The TabGrid is currently presented.
      Browser* browser =
          GetForegroundActiveScene().interfaceProvider.mainInterface.browser;
      UrlLoadParams params = UrlLoadParams::InNewTab(GURL(kChromeUINewTabURL));
      [GetForegroundActiveSceneController().tabSwitcher
          dismissWithNewTabAnimationToBrowser:browser
                            withUrlLoadParams:params
                                      atIndex:INT_MAX];
      return;
    }
    id<ApplicationCommands, BrowserCommands> handler =
        chrome_test_util::HandlerForActiveBrowser();
    [handler openURLInNewTab:command];
  }
}

NSURL* SimulateExternalAppURLOpening() {
  NSURL* url = [NSURL URLWithString:@"http://www.example.com"];
  UIApplication* application = UIApplication.sharedApplication;
  id<UIApplicationDelegate> applicationDelegate = application.delegate;
  [applicationDelegate application:application openURL:url options:@{}];
  return url;
}

void SimulateAddAccountFromWeb() {
  id<ApplicationCommands, BrowserCommands> handler =
      chrome_test_util::HandlerForActiveBrowser();
  [handler showAddAccountFromViewController:(UIViewController*)
                                                GetForegroundActiveScene()
                                                    .interfaceProvider
                                                    .mainInterface.bvc];
}

void OpenNewIncognitoTab() {
  @autoreleasepool {  // Make sure that all internals are deallocated.
    OpenNewTabCommand* command = [OpenNewTabCommand incognitoTabCommand];
    if (GetForegroundActiveSceneController().isTabSwitcherActive) {
      // The TabGrid is currently presented.
      Browser* browser = GetForegroundActiveScene()
                             .interfaceProvider.incognitoInterface.browser;
      UrlLoadParams params = UrlLoadParams::InNewTab(GURL(kChromeUINewTabURL));
      [GetForegroundActiveSceneController().tabSwitcher
          dismissWithNewTabAnimationToBrowser:browser
                            withUrlLoadParams:params
                                      atIndex:INT_MAX];
      return;
    }
    id<ApplicationCommands, BrowserCommands> handler =
        chrome_test_util::HandlerForActiveBrowser();
    [handler openURLInNewTab:command];
  }
}

web::WebState* GetCurrentWebState() {
  WebStateList* web_state_list = GetCurrentWebStateList();
  return web_state_list ? web_state_list->GetActiveWebState() : nullptr;
}

web::WebState* GetNextWebState() {
  WebStateList* web_state_list = GetCurrentWebStateList();
  if (!web_state_list || web_state_list->count() < 2)
    return nullptr;
  int next_index = web_state_list->active_index() + 1;
  if (next_index >= web_state_list->count())
    next_index = 0;
  return web_state_list->GetWebStateAt(next_index);
}

web::WebState* GetWebStateAtIndexInCurrentMode(int index) {
  WebStateList* web_state_list = GetCurrentWebStateList();
  if (!web_state_list || !web_state_list->ContainsIndex(index))
    return nullptr;
  return web_state_list->GetWebStateAt(index);
}

NSString* GetCurrentTabTitle() {
  return tab_util::GetTabTitle(GetCurrentWebState());
}

NSString* GetNextTabTitle() {
  return tab_util::GetTabTitle(GetNextWebState());
}

void CloseCurrentTab() {
  WebStateList* web_state_list = GetCurrentWebStateList();
  if (!web_state_list ||
      web_state_list->active_index() == WebStateList::kInvalidIndex)
    return;
  web_state_list->CloseWebStateAt(web_state_list->active_index(),
                                  WebStateList::CLOSE_USER_ACTION);
}

void CloseTabAtIndex(NSUInteger index) {
  @autoreleasepool {  // Make sure that all internals are deallocated.
    [GetCurrentTabModel() closeTabAtIndex:index];
  }
}

NSUInteger GetIndexOfActiveNormalTab() {
  TabModel* model = chrome_test_util::GetForegroundActiveSceneController()
                        .interfaceProvider.mainInterface.tabModel;
  return model.webStateList->active_index();
}

void CloseAllTabsInCurrentMode() {
  [GetCurrentTabModel() closeAllTabs];
}

void CloseAllTabs() {
  if (GetIncognitoTabCount()) {
    [GetForegroundActiveSceneController()
            .interfaceProvider.incognitoInterface.tabModel closeAllTabs];
  }
  if (GetMainTabCount()) {
    [GetForegroundActiveScene()
            .interfaceProvider.mainInterface.tabModel closeAllTabs];
  }
}

void SelectTabAtIndexInCurrentMode(NSUInteger index) {
  @autoreleasepool {  // Make sure that all internals are deallocated.

    WebStateList* web_state_list = GetCurrentWebStateList();
    web_state_list->ActivateWebStateAt(static_cast<int>(index));
  }
}

NSUInteger GetMainTabCount() {
  return GetMainController().interfaceProvider.mainInterface.tabModel.count;
}

NSUInteger GetIncognitoTabCount() {
  return GetMainController()
      .interfaceProvider.incognitoInterface.tabModel.count;
}

BOOL ResetTabUsageRecorder() {
  TabUsageRecorderBrowserAgent* tab_usage_recorder =
      TabUsageRecorderBrowserAgent::FromBrowser(GetCurrentBrowser());
  if (!tab_usage_recorder)
    return NO;
  tab_usage_recorder->ResetAll();
  return YES;
}

BOOL SetCurrentTabsToBeColdStartTabs() {
  TabUsageRecorderBrowserAgent* tab_usage_recorder =
      TabUsageRecorderBrowserAgent::FromBrowser(GetCurrentBrowser());

  if (!tab_usage_recorder)
    return NO;
  WebStateList* web_state_list = GetCurrentBrowser()->GetWebStateList();

  std::vector<web::WebState*> web_states;
  web_states.reserve(web_state_list->count());
  for (int index = 0; index < web_state_list->count(); ++index) {
    web_states.push_back(web_state_list->GetWebStateAt(index));
  }
  tab_usage_recorder->InitialRestoredTabs(web_state_list->GetActiveWebState(),
                                          web_states);
  return YES;
}

BOOL SimulateTabsBackgrounding() {
  TabUsageRecorderBrowserAgent* tab_usage_recorder =
      TabUsageRecorderBrowserAgent::FromBrowser(GetCurrentBrowser());
  if (!tab_usage_recorder)
    return NO;
  tab_usage_recorder->AppDidEnterBackground();
  return YES;
}

void SaveSessionImmediately() {
  SessionRestorationBrowserAgent::FromBrowser(GetCurrentBrowser())
      ->SaveSession(true);
}

void EvictOtherTabModelTabs() {
  id<BrowserInterfaceProvider> provider = GetMainController().interfaceProvider;
  Browser* otherBrowser = IsIncognitoMode()
                              ? provider.mainInterface.browser
                              : provider.incognitoInterface.browser;
  // Disabling and enabling web usage will evict all web views.
  WebUsageEnablerBrowserAgent* enabler =
      WebUsageEnablerBrowserAgent::FromBrowser(otherBrowser);
  DCHECK(enabler);
  enabler->SetWebUsageEnabled(false);
  enabler->SetWebUsageEnabled(true);
}

BOOL CloseAllNormalTabs() {
  MainController* main_controller = GetMainController();
  DCHECK(main_controller);

  Browser* browser = main_controller.interfaceProvider.mainInterface.browser;
  DCHECK(browser);
  browser->GetWebStateList()->CloseAllWebStates(
      WebStateList::CLOSE_USER_ACTION);
  return YES;
}

BOOL CloseAllIncognitoTabs() {
  MainController* main_controller = GetMainController();
  DCHECK(main_controller);
  TabModel* tabModel =
      GetMainController().interfaceProvider.incognitoInterface.tabModel;
  DCHECK(tabModel);
  [tabModel closeAllTabs];
  return YES;
}

NSUInteger GetEvictedMainTabCount() {
  Browser* browser =
      GetMainController().interfaceProvider.mainInterface.browser;
  TabUsageRecorderBrowserAgent* tab_usage_recorder =
      TabUsageRecorderBrowserAgent::FromBrowser(browser);
  if (!tab_usage_recorder)
    return 0;
  return tab_usage_recorder->EvictedTabsMapSize();
}

}  // namespace chrome_test_util
