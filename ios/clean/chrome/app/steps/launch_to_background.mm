// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/app/steps/launch_to_background.h"

#include <memory>

#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/supports_user_data.h"
#include "ios/chrome/app/startup/ios_chrome_main.h"
#include "ios/chrome/app/startup/register_experimental_settings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/web/chrome_web_client.h"
#include "ios/clean/chrome/app/application_state.h"
#include "ios/web/public/web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Step that removes a previously-stored IOSChromeMain instance.
@interface StopChromeMain : NSObject<ApplicationStep>
@end

namespace {

// A SupportsUserData::Data struct to store an IOSChromeMain object and preserve
// its lifetime (by storing it in an ApplicationState's |state| property) beyond
// the running time of the -StartChromeMain step.
class ChromeMainContainer : public base::SupportsUserData::Data {
 public:
  explicit ChromeMainContainer(std::unique_ptr<IOSChromeMain> chrome_main)
      : main_(std::move(chrome_main)) {}

 private:
  std::unique_ptr<IOSChromeMain> main_;
};

// Key for storing the ChromeMainContainer.
const char kChromeMainKey[] = "chrome_main";

}  // namespace

@implementation SetupBundleAndUserDefaults

- (BOOL)canRunInState:(ApplicationState*)state {
  return state.phase == APPLICATION_BASIC;
}

- (void)runInState:(ApplicationState*)state {
  NSBundle* baseBundle = base::mac::OuterBundle();
  base::mac::SetBaseBundleID(
      base::SysNSStringToUTF8([baseBundle bundleIdentifier]).c_str());

  [RegisterExperimentalSettings
      registerExperimentalSettingsWithUserDefaults:[NSUserDefaults
                                                       standardUserDefaults]
                                            bundle:base::mac::
                                                       FrameworkBundle()];
}

@end

@implementation StartChromeMain

- (BOOL)canRunInState:(ApplicationState*)state {
  return state.phase == APPLICATION_BASIC;
}

- (void)runInState:(ApplicationState*)state {
  web::SetWebClient(new ChromeWebClient());

  // Create and persist an IOSChromeMain instance.
  state.persistentState->SetUserData(
      kChromeMainKey,
      new ChromeMainContainer(base::MakeUnique<IOSChromeMain>()));

  // Add a step to the termination steps of |state| that will stop and remove
  // the IOSChromeMain instance.
  [[state terminationSteps] addObject:[[StopChromeMain alloc] init]];

  state.phase = APPLICATION_BACKGROUNDED;
}

@end

@implementation StopChromeMain

- (BOOL)canRunInState:(ApplicationState*)state {
  return state.phase == APPLICATION_TERMINATING;
}

- (void)runInState:(ApplicationState*)state {
  // This should be the last step executed while the app is terminating.
  if (state.terminationSteps.count) {
    // There are other steps waiting to run, so add this to the end and return.
    [state.terminationSteps addObject:self];
    return;
  }
  state.persistentState->RemoveUserData(kChromeMainKey);
}

@end

@implementation SetBrowserState

- (BOOL)canRunInState:(ApplicationState*)state {
  ApplicationContext* applicationContext = GetApplicationContext();
  if (!applicationContext)
    return NO;

  return applicationContext->GetChromeBrowserStateManager() &&
         state.phase == APPLICATION_BACKGROUNDED;
}

- (void)runInState:(ApplicationState*)state {
  state.browserState = GetApplicationContext()
                           ->GetChromeBrowserStateManager()
                           ->GetLastUsedBrowserState();
}

@end
