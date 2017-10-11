// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/tab_test_util.h"

#import <Foundation/Foundation.h>

#import "base/mac/foundation_util.h"
#import "ios/chrome/app/main_controller_private.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_controller.h"
#include "ios/chrome/browser/experimental_flags.h"
#import "ios/chrome/browser/metrics/tab_usage_recorder.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_private.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/testing/wait_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

namespace {

// Returns the tab model for the current mode (incognito or normal).
TabModel* GetCurrentTabModel() {
  return IsIncognitoMode()
             ? [[GetMainController() browserViewInformation] otrTabModel]
             : [[GetMainController() browserViewInformation] mainTabModel];
}

}  // namespace

BOOL IsIncognitoMode() {
  MainController* main_controller = GetMainController();
  BrowserViewController* otr_bvc =
      [[main_controller browserViewInformation] otrBVC];
  BrowserViewController* current_bvc =
      [[main_controller browserViewInformation] currentBVC];
  return otr_bvc == current_bvc;
}

void OpenNewTab() {
  @autoreleasepool {  // Make sure that all internals are deallocated.
    OpenNewTabCommand* command = [OpenNewTabCommand command];
    [chrome_test_util::DispatcherForActiveViewController() openNewTab:command];
  }
}

void OpenNewIncognitoTab() {
  @autoreleasepool {  // Make sure that all internals are deallocated.
    OpenNewTabCommand* command = [OpenNewTabCommand incognitoTabCommand];
    [chrome_test_util::DispatcherForActiveViewController() openNewTab:command];
  }
}

Tab* GetCurrentTab() {
  TabModel* tab_model = GetCurrentTabModel();
  return tab_model.currentTab;
}

Tab* GetNextTab() {
  TabModel* tabModel = GetCurrentTabModel();
  NSUInteger tabCount = [tabModel count];
  if (tabCount < 2)
    return nil;
  Tab* currentTab = [tabModel currentTab];
  NSUInteger nextTabIndex = [tabModel indexOfTab:currentTab] + 1;
  if (nextTabIndex >= tabCount)
    nextTabIndex = 0;
  return [tabModel tabAtIndex:nextTabIndex];
}

void CloseCurrentTab() {
  TabModel* tab_model = GetCurrentTabModel();
  [tab_model closeTab:tab_model.currentTab];
}

void CloseTabAtIndex(NSUInteger index) {
  @autoreleasepool {  // Make sure that all internals are deallocated.
    [GetCurrentTabModel() closeTabAtIndex:index];
  }
}

void CloseAllTabsInCurrentMode() {
  [GetCurrentTabModel() closeAllTabs];
}

void CloseAllTabs() {
  if (GetIncognitoTabCount()) {
    [[[GetMainController() browserViewInformation] otrTabModel] closeAllTabs];
  }
  if (GetMainTabCount()) {
    [[[GetMainController() browserViewInformation] mainTabModel] closeAllTabs];
  }
}

void SelectTabAtIndexInCurrentMode(NSUInteger index) {
  @autoreleasepool {  // Make sure that all internals are deallocated.
    TabModel* tab_model = GetCurrentTabModel();
    [tab_model setCurrentTab:[tab_model tabAtIndex:index]];
  }
}

NSUInteger GetMainTabCount() {
  return [[[GetMainController() browserViewInformation] mainTabModel] count];
}

NSUInteger GetIncognitoTabCount() {
  return [[[GetMainController() browserViewInformation] otrTabModel] count];
}

BOOL ResetTabUsageRecorder() {
  if (!GetCurrentTabModel().tabUsageRecorder)
    return NO;
  GetCurrentTabModel().tabUsageRecorder->ResetAll();
  return YES;
}

BOOL SetCurrentTabsToBeColdStartTabs() {
  if (!GetCurrentTabModel().tabUsageRecorder)
    return NO;
  TabModel* tab_model = GetCurrentTabModel();
  std::vector<web::WebState*> web_states;
  for (Tab* tab in tab_model) {
    web_states.push_back(tab.webState);
  }
  tab_model.tabUsageRecorder->InitialRestoredTabs(tab_model.currentTab.webState,
                                                  web_states);
  return YES;
}

BOOL SimulateTabsBackgrounding() {
  if (!GetCurrentTabModel().tabUsageRecorder)
    return NO;
  GetCurrentTabModel().tabUsageRecorder->AppDidEnterBackground();
  return YES;
}

void EvictOtherTabModelTabs() {
  TabModel* otherTabModel =
      IsIncognitoMode()
          ? [GetMainController().browserViewInformation mainTabModel]
          : [GetMainController().browserViewInformation otrTabModel];
  // Disabling and enabling web usage will evict all web views.
  otherTabModel.webUsageEnabled = NO;
  otherTabModel.webUsageEnabled = YES;
}

void CloseAllIncognitoTabs() {
  MainController* main_controller = chrome_test_util::GetMainController();
  DCHECK(main_controller);
  TabModel* tabModel = [[main_controller browserViewInformation] otrTabModel];
  DCHECK(tabModel);
  [tabModel closeAllTabs];
  if (!IsIPadIdiom()) {
    // If the OTR BVC is active, wait until it isn't (since all of the
    // tabs are now closed)
    testing::WaitUntilConditionOrTimeout(testing::kWaitForUIElementTimeout, ^{
      return !IsIncognitoMode();
    });
  }
}

NSUInteger GetEvictedMainTabCount() {
  if (![[GetMainController() browserViewInformation] mainTabModel]
           .tabUsageRecorder)
    return 0;
  return [[GetMainController() browserViewInformation] mainTabModel]
      .tabUsageRecorder->EvictedTabsMapSize();
}

}  // namespace chrome_test_util
