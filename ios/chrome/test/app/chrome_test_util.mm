// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/chrome_test_util.h"

#include "base/mac/foundation_util.h"
#import "breakpad/src/client/ios/BreakpadController.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/app/application_delegate/metrics_mediator_testing.h"
#import "ios/chrome/app/chrome_overlay_window.h"
#import "ios/chrome/app/main_application_delegate_testing.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/app/main_controller_private.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/metrics/previous_session_info.h"
#import "ios/chrome/browser/metrics/previous_session_info_private.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#import "ios/chrome/browser/ui/main/main_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_controller.h"
#import "ios/web/public/test/native_controller_test_util.h"

// Methods to access private members for testing.
@interface BreakpadController (Testing)
- (BOOL)isEnabled;
- (BOOL)isUploadingEnabled;
@end
@implementation BreakpadController (Testing)
- (BOOL)isEnabled {
  return started_;
}
- (BOOL)isUploadingEnabled {
  return enableUploads_;
}
@end

namespace {
// Returns the current tab model.
TabModel* GetCurrentTabModel() {
  MainController* main_controller = chrome_test_util::GetMainController();
  DCHECK(main_controller);
  BrowserViewController* main_bvc =
      [[main_controller browserViewInformation] mainBVC];
  BrowserViewController* current_bvc =
      [[main_controller browserViewInformation] currentBVC];

  return current_bvc == main_bvc
             ? [[main_controller browserViewInformation] mainTabModel]
             : [[main_controller browserViewInformation] otrTabModel];
}

// Returns the current tab.
Tab* GetCurrentTab() {
  TabModel* tab_model = GetCurrentTabModel();
  return [tab_model currentTab];
}

// Returns the original ChromeBrowserState if |incognito| is false. If
// |ingonito| is true, returns an off-the-record ChromeBrowserState.
ios::ChromeBrowserState* GetBrowserState(bool incognito) {
  std::vector<ios::ChromeBrowserState*> browser_states =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();
  DCHECK(!browser_states.empty());

  ios::ChromeBrowserState* browser_state = browser_states.front();
  DCHECK(!browser_state->IsOffTheRecord());

  return incognito ? browser_state->GetOffTheRecordChromeBrowserState()
                   : browser_state;
}

// Gets the root UIViewController.
UIViewController* GetActiveViewController() {
  UIWindow* main_window = [[UIApplication sharedApplication] keyWindow];
  DCHECK([main_window isKindOfClass:[ChromeOverlayWindow class]]);
  MainViewController* main_view_controller =
      base::mac::ObjCCast<MainViewController>([main_window rootViewController]);
  return main_view_controller.activeViewController;
}

}  // namespace

namespace chrome_test_util {

MainController* GetMainController() {
  return [MainApplicationDelegate sharedMainController];
}

DeviceSharingManager* GetDeviceSharingManager() {
  return [GetMainController() deviceSharingManager];
}

NewTabPageController* GetCurrentNewTabPageController() {
  web::WebState* web_state = GetCurrentWebState();
  id nativeController = web::test::GetCurrentNativeController(web_state);
  if (![nativeController isKindOfClass:[NewTabPageController class]])
    return nil;
  return base::mac::ObjCCast<NewTabPageController>(nativeController);
}

web::WebState* GetCurrentWebState() {
  return GetCurrentTab().webState;
}

ios::ChromeBrowserState* GetOriginalBrowserState() {
  return GetBrowserState(false);
}

ios::ChromeBrowserState* GetCurrentIncognitoBrowserState() {
  return GetBrowserState(true);
}

NSUInteger GetRegisteredKeyCommandsCount() {
  BrowserViewController* mainBVC =
      GetMainController().browserViewInformation.mainBVC;
  return mainBVC.keyCommands.count;
}

void RunCommandWithActiveViewController(GenericChromeCommand* command) {
  [GetActiveViewController() chromeExecuteCommand:command];
}

void RemoveAllInfoBars() {
  infobars::InfoBarManager* info_bar_manager = [GetCurrentTab() infoBarManager];
  if (info_bar_manager) {
    info_bar_manager->RemoveAllInfoBars(false /* animate */);
  }
}

void ClearPresentedState() {
  [GetMainController() dismissModalDialogsWithCompletion:nil];
}

void ResetAllWebViews() {
  id<BrowserViewInformation> browser_view_info =
      [GetMainController() browserViewInformation];
  [[browser_view_info mainTabModel] resetAllWebViews];
  [[browser_view_info otrTabModel] resetAllWebViews];
}

void SetBooleanLocalStatePref(const char* pref_name, bool value) {
  DCHECK(GetApplicationContext());
  DCHECK(GetApplicationContext()->GetLocalState());
  BooleanPrefMember pref;
  pref.Init(pref_name, GetApplicationContext()->GetLocalState());
  pref.SetValue(value);
}

void SetBooleanUserPref(ios::ChromeBrowserState* browser_state,
                        const char* pref_name,
                        bool value) {
  DCHECK(browser_state);
  DCHECK(browser_state->GetPrefs());
  BooleanPrefMember pref;
  pref.Init(pref_name, browser_state->GetPrefs());
  pref.SetValue(value);
}

void SetWWANStateTo(bool value) {
  MainController* mainController = chrome_test_util::GetMainController();
  net::NetworkChangeNotifier::ConnectionType connectionType =
      value ? net::NetworkChangeNotifier::CONNECTION_4G
            : net::NetworkChangeNotifier::CONNECTION_WIFI;
  [mainController.metricsMediator connectionTypeChanged:connectionType];
}

void SetFirstLaunchStateTo(bool value) {
  [[PreviousSessionInfo sharedInstance] setIsFirstSessionAfterUpgrade:value];
}

bool IsMetricsRecordingEnabled() {
  DCHECK(GetApplicationContext());
  DCHECK(GetApplicationContext()->GetMetricsService());
  return GetApplicationContext()->GetMetricsService()->recording_active();
}

bool IsMetricsReportingEnabled() {
  DCHECK(GetApplicationContext());
  DCHECK(GetApplicationContext()->GetMetricsService());
  return GetApplicationContext()->GetMetricsService()->reporting_active();
}

bool IsBreakpadEnabled() {
  return [[BreakpadController sharedInstance] isEnabled];
}

bool IsBreakpadReportingEnabled() {
  return [[BreakpadController sharedInstance] isUploadingEnabled];
}

bool IsFirstLaunchAfterUpgrade() {
  return [chrome_test_util::GetMainController() isFirstLaunchAfterUpgrade];
}

void OpenChromeFromExternalApp(const GURL& url) {
  [[[UIApplication sharedApplication] delegate]
      applicationWillResignActive:[UIApplication sharedApplication]];
  [GetMainController() setStartupParametersWithURL:url];

  [[[UIApplication sharedApplication] delegate]
      applicationDidBecomeActive:[UIApplication sharedApplication]];
}

}  // namespace chrome_test_util
