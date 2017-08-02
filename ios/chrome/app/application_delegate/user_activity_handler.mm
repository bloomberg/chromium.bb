// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/user_activity_handler.h"

#import <CoreSpotlight/CoreSpotlight.h>
#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"
#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/handoff/handoff_utility.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#include "ios/chrome/app/application_mode.h"
#import "ios/chrome/app/spotlight/actions_spotlight_manager.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#include "ios/chrome/browser/app_startup_parameters.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/metrics/first_user_action_recorder.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/u2f/u2f_controller.h"
#import "ios/chrome/browser/ui/main/browser_view_information.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

namespace {
// Constants for 3D touch application static shortcuts.
NSString* const kShortcutNewTab = @"OpenNewTab";
NSString* const kShortcutNewIncognitoTab = @"OpenIncognitoTab";
NSString* const kShortcutVoiceSearch = @"OpenVoiceSearch";
NSString* const kShortcutQRScanner = @"OpenQRScanner";
}  // namespace

@interface UserActivityHandler ()
// Handles the 3D touch application static items. Does nothing if in first run.
+ (BOOL)handleShortcutItem:(UIApplicationShortcutItem*)shortcutItem
        startupInformation:(id<StartupInformation>)startupInformation;
// Routes Universal 2nd Factor (U2F) callback to the correct Tab.
+ (void)routeU2FURL:(const GURL&)URL
    browserViewInformation:(id<BrowserViewInformation>)browserViewInformation;
@end

@implementation UserActivityHandler

#pragma mark - Public methods.

+ (BOOL)continueUserActivity:(NSUserActivity*)userActivity
         applicationIsActive:(BOOL)applicationIsActive
                   tabOpener:(id<TabOpening>)tabOpener
          startupInformation:(id<StartupInformation>)startupInformation {
  NSURL* webpageURL = userActivity.webpageURL;

  if ([userActivity.activityType
          isEqualToString:handoff::kChromeHandoffActivityType]) {
    // App was launched by iOS as a result of Handoff.
    NSString* originString = base::mac::ObjCCast<NSString>(
        userActivity.userInfo[handoff::kOriginKey]);
    handoff::Origin origin = handoff::OriginFromString(originString);
    UMA_HISTOGRAM_ENUMERATION("IOS.Handoff.Origin", origin,
                              handoff::ORIGIN_COUNT);
  } else if ([userActivity.activityType
                 isEqualToString:NSUserActivityTypeBrowsingWeb]) {
    // App was launched as the result of a Universal Link navigation. The value
    // of userActivity.webpageURL is not used. The only supported action
    // at this time is opening a New Tab Page.
    GURL newTabURL(kChromeUINewTabURL);
    webpageURL = net::NSURLWithGURL(newTabURL);
    AppStartupParameters* startupParams =
        [[AppStartupParameters alloc] initWithExternalURL:newTabURL];
    [startupInformation setStartupParameters:startupParams];
    base::RecordAction(base::UserMetricsAction("IOSLaunchedByUniversalLink"));
  } else if (spotlight::IsSpotlightAvailable() &&
             [userActivity.activityType
                 isEqualToString:CSSearchableItemActionType]) {
    // App was launched by iOS as the result of a tap on a Spotlight Search
    // result.
    NSString* itemID =
        [userActivity.userInfo objectForKey:CSSearchableItemActivityIdentifier];
    spotlight::Domain domain = spotlight::SpotlightDomainFromString(itemID);
    UMA_HISTOGRAM_ENUMERATION("IOS.Spotlight.Origin", domain,
                              spotlight::DOMAIN_COUNT);

    if (!itemID) {
      return NO;
    }
    if (domain == spotlight::DOMAIN_ACTIONS) {
      webpageURL =
          [NSURL URLWithString:base::SysUTF8ToNSString(kChromeUINewTabURL)];
      AppStartupParameters* startupParams = [[AppStartupParameters alloc]
          initWithExternalURL:GURL(kChromeUINewTabURL)];
      BOOL startupParamsSet = spotlight::SetStartupParametersForSpotlightAction(
          itemID, startupParams);
      if (!startupParamsSet) {
        return NO;
      }
      [startupInformation setStartupParameters:startupParams];
    } else if (!webpageURL && base::ios::IsRunningOnIOS10OrLater()) {
      // spotlight::GetURLForSpotlightItemID uses CSSearchQuery, which is only
      // supported from iOS 10.
      spotlight::GetURLForSpotlightItemID(itemID, ^(NSURL* contentURL) {
        if (!contentURL) {
          return;
        }
        dispatch_async(dispatch_get_main_queue(), ^{
          // Update the isActive flag as it may have changed during the async
          // calls.
          BOOL isActive = [[UIApplication sharedApplication]
                              applicationState] == UIApplicationStateActive;
          [self continueUserActivityURL:contentURL
                    applicationIsActive:isActive
                              tabOpener:tabOpener
                     startupInformation:startupInformation];
        });
      });
      return YES;
    }
  } else {
    // Do nothing for unknown activity type.
    return NO;
  }

  return [self continueUserActivityURL:webpageURL
                   applicationIsActive:applicationIsActive
                             tabOpener:tabOpener
                    startupInformation:startupInformation];
}

+ (BOOL)continueUserActivityURL:(NSURL*)webpageURL
            applicationIsActive:(BOOL)applicationIsActive
                      tabOpener:(id<TabOpening>)tabOpener
             startupInformation:(id<StartupInformation>)startupInformation {
  if (!webpageURL)
    return NO;

  GURL webpageGURL(net::GURLWithNSURL(webpageURL));
  if (!webpageGURL.is_valid())
    return NO;

  if (applicationIsActive && ![startupInformation isPresentingFirstRunUI]) {
    // The app is already active so the applicationDidBecomeActive: method will
    // never be called. Open the requested URL immediately.
    ApplicationMode targetMode =
        [[startupInformation startupParameters] launchInIncognito]
            ? ApplicationMode::INCOGNITO
            : ApplicationMode::NORMAL;
    [tabOpener dismissModalsAndOpenSelectedTabInMode:targetMode
                                             withURL:webpageGURL
                                          transition:ui::PAGE_TRANSITION_LINK
                                          completion:^{
                                            [startupInformation
                                                setStartupParameters:nil];
                                          }];
    return YES;
  }

  // Don't record the first action as a user action, since it will not be
  // initiated by the user.
  [startupInformation resetFirstUserActionRecorder];

  if (![startupInformation startupParameters]) {
    AppStartupParameters* startupParams =
        [[AppStartupParameters alloc] initWithExternalURL:webpageGURL];
    [startupInformation setStartupParameters:startupParams];
  }
  return YES;
}

+ (void)performActionForShortcutItem:(UIApplicationShortcutItem*)shortcutItem
                   completionHandler:(void (^)(BOOL succeeded))completionHandler
                           tabOpener:(id<TabOpening>)tabOpener
                  startupInformation:(id<StartupInformation>)startupInformation
              browserViewInformation:
                  (id<BrowserViewInformation>)browserViewInformation {
  BOOL handledShortcutItem =
      [UserActivityHandler handleShortcutItem:shortcutItem
                           startupInformation:startupInformation];
  if (handledShortcutItem) {
    [UserActivityHandler
        handleStartupParametersWithTabOpener:tabOpener
                          startupInformation:startupInformation
                      browserViewInformation:browserViewInformation];
  }
  completionHandler(handledShortcutItem);
}

+ (BOOL)willContinueUserActivityWithType:(NSString*)userActivityType {
  return
      [userActivityType isEqualToString:handoff::kChromeHandoffActivityType] ||
      (spotlight::IsSpotlightAvailable() &&
       [userActivityType isEqualToString:CSSearchableItemActionType]);
}

+ (void)handleStartupParametersWithTabOpener:(id<TabOpening>)tabOpener
                          startupInformation:
                              (id<StartupInformation>)startupInformation
                      browserViewInformation:
                          (id<BrowserViewInformation>)browserViewInformation {
  DCHECK([startupInformation startupParameters]);
  // Do not load the external URL if the user has not accepted the terms of
  // service. This corresponds to the case when the user installed Chrome,
  // has never launched it and attempts to open an external URL in Chrome.
  if ([startupInformation isPresentingFirstRunUI])
    return;

  // Check if it's an U2F call. If so, route it to correct tab.
  // If not, open or reuse tab in main BVC.
  if ([U2FController
          isU2FURL:[[startupInformation startupParameters] externalURL]]) {
    [UserActivityHandler routeU2FURL:[[startupInformation startupParameters]
                                         externalURL]
              browserViewInformation:browserViewInformation];
    // It's OK to clear startup parameters here because routeU2FURL works
    // synchronously.
    [startupInformation setStartupParameters:nil];
  } else {
    // The app is already active so the applicationDidBecomeActive: method
    // will never be called. Open the requested URL after all modal UIs have
    // been dismissed. |_startupParameters| must be retained until all deferred
    // modal UIs are dismissed and tab opened with requested URL.
    ApplicationMode targetMode =
        [[startupInformation startupParameters] launchInIncognito]
            ? ApplicationMode::INCOGNITO
            : ApplicationMode::NORMAL;
    [tabOpener dismissModalsAndOpenSelectedTabInMode:targetMode
                                             withURL:[[startupInformation
                                                         startupParameters]
                                                         externalURL]
                                          transition:ui::PAGE_TRANSITION_LINK
                                          completion:^{
                                            [startupInformation
                                                setStartupParameters:nil];
                                          }];
  }
}

#pragma mark - Internal methods.

+ (BOOL)handleShortcutItem:(UIApplicationShortcutItem*)shortcutItem
        startupInformation:(id<StartupInformation>)startupInformation {
  if ([startupInformation isPresentingFirstRunUI])
    return NO;

  AppStartupParameters* startupParams = [[AppStartupParameters alloc]
      initWithExternalURL:GURL(kChromeUINewTabURL)];

  if ([shortcutItem.type isEqualToString:kShortcutNewTab]) {
    base::RecordAction(UserMetricsAction("ApplicationShortcut.NewTabPressed"));
    [startupInformation setStartupParameters:startupParams];
    return YES;

  } else if ([shortcutItem.type isEqualToString:kShortcutNewIncognitoTab]) {
    base::RecordAction(
        UserMetricsAction("ApplicationShortcut.NewIncognitoTabPressed"));
    [startupParams setLaunchInIncognito:YES];
    [startupInformation setStartupParameters:startupParams];
    return YES;

  } else if ([shortcutItem.type isEqualToString:kShortcutVoiceSearch]) {
    base::RecordAction(
        UserMetricsAction("ApplicationShortcut.VoiceSearchPressed"));
    [startupParams setPostOpeningAction:START_VOICE_SEARCH];
    [startupInformation setStartupParameters:startupParams];
    return YES;

  } else if ([shortcutItem.type isEqualToString:kShortcutQRScanner]) {
    base::RecordAction(
        UserMetricsAction("ApplicationShortcut.ScanQRCodePressed"));
    [startupParams setPostOpeningAction:START_QR_CODE_SCANNER];
    [startupInformation setStartupParameters:startupParams];
    return YES;
  }

  NOTREACHED();
  return NO;
}

+ (void)routeU2FURL:(const GURL&)URL
    browserViewInformation:(id<BrowserViewInformation>)browserViewInformation {
  // Retrieve the designated TabID from U2F URL.
  NSString* tabID = [U2FController tabIDFromResponseURL:URL];
  if (!tabID) {
    return;
  }

  // TODO(crbug.com/619598): move this code to BrowserViewInformation to hide
  // implementation details of TabModel.
  // Iterate through mainTabModel and OTRTabModel to find the corresponding tab.
  NSArray* tabModels = @[
    [browserViewInformation mainTabModel], [browserViewInformation otrTabModel]
  ];
  for (TabModel* tabModel in tabModels) {
    for (Tab* tab in tabModel) {
      if ([tab.tabId isEqualToString:tabID]) {
        [tab evaluateU2FResultFromURL:URL];
        return;
      }
    }
  }
}

@end
