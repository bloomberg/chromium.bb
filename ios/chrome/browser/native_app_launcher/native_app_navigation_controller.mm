// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/native_app_launcher/native_app_navigation_controller.h"

#import <StoreKit/StoreKit.h>

#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/image_fetcher/ios/ios_image_data_fetcher_wrapper.h"
#include "components/infobars/core/infobar_manager.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/installation_notifier.h"
#include "ios/chrome/browser/native_app_launcher/native_app_infobar_delegate.h"
#include "ios/chrome/browser/native_app_launcher/native_app_navigation_util.h"
#import "ios/chrome/browser/open_url_util.h"
#import "ios/chrome/browser/tabs/tab.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/native_app_launcher/native_app_metadata.h"
#import "ios/public/provider/chrome/browser/native_app_launcher/native_app_types.h"
#import "ios/public/provider/chrome/browser/native_app_launcher/native_app_whitelist_manager.h"
#include "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_thread.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

@interface NativeAppNavigationController ()
// Shows a native app infobar by looking at the page's URL and by checking
// wheter that infobar should be bypassed or not.
- (void)showInfoBarIfNecessary;

// Returns a pointer to the NSMutableSet of |_appsPossiblyBeingInstalled|
- (NSMutableSet*)appsPossiblyBeingInstalled;

// Records what type of infobar was opened.
- (void)recordInfobarDisplayedOfType:(NativeAppControllerType)type
                    onLinkNavigation:(BOOL)isLinkNavigation;

@end

@implementation NativeAppNavigationController {
  // WebState provides access to the *TabHelper objects. This will eventually
  // replace the need to have |_tab| in this object.
  web::WebState* _webState;
  // ImageFetcher needed to fetch the icons.
  std::unique_ptr<image_fetcher::IOSImageDataFetcherWrapper> _imageFetcher;
  // DEPRECATED: Tab hosting the infobar and is also used for accessing Tab
  // states such as navigation manager and whether it is a pre-rendered tab.
  // Use |webState| whenever possible.
  __weak Tab* _tab;
  id<NativeAppMetadata> _metadata;
  // A set of appIds encoded as NSStrings.
  NSMutableSet* _appsPossiblyBeingInstalled;
}

// Designated initializer. Use this instead of -init.
- (instancetype)initWithWebState:(web::WebState*)webState
            requestContextGetter:(net::URLRequestContextGetter*)context
                             tab:(Tab*)tab {
  self = [super init];
  if (self) {
    DCHECK(context);
    _imageFetcher = base::MakeUnique<image_fetcher::IOSImageDataFetcherWrapper>(
        context, web::WebThread::GetBlockingPool());
    DCHECK(webState);
    _webState = webState;
    // Allows |tab| to be nil for unit testing. If not nil, it should have the
    // same webState.
    DCHECK(!tab || [tab webState] == webState);
    _tab = tab;
    _appsPossiblyBeingInstalled = [[NSMutableSet alloc] init];
  }
  return self;
}

- (void)copyStateFrom:(NativeAppNavigationController*)controller {
  DCHECK(controller);
  _appsPossiblyBeingInstalled = [[NSMutableSet alloc]
      initWithSet:[controller appsPossiblyBeingInstalled]];
  for (NSString* appIdString in _appsPossiblyBeingInstalled) {
    DCHECK([appIdString isKindOfClass:[NSString class]]);
    NSURL* appURL =
        [ios::GetChromeBrowserProvider()->GetNativeAppWhitelistManager()
            schemeForAppId:appIdString];
    [[InstallationNotifier sharedInstance]
        registerForInstallationNotifications:self
                                withSelector:@selector(appDidInstall:)
                                   forScheme:[appURL scheme]];
  }
  [self showInfoBarIfNecessary];
}

- (void)dealloc {
  [[InstallationNotifier sharedInstance] unregisterForNotifications:self];
}

- (NSMutableSet*)appsPossiblyBeingInstalled {
  return _appsPossiblyBeingInstalled;
}

- (void)showInfoBarIfNecessary {
  // Find a potential matching native app.
  GURL pageURL = _webState->GetLastCommittedURL();
  _metadata = [ios::GetChromeBrowserProvider()->GetNativeAppWhitelistManager()
      nativeAppForURL:pageURL];
  if (!_metadata || [_metadata shouldBypassInfoBars])
    return;

  // Select the infobar type.
  NativeAppControllerType type;
  bool isLinkNavigation = native_app_launcher::IsLinkNavigation(_webState);
  if ([_metadata canOpenURL:pageURL]) {  // App is installed.
    type = isLinkNavigation && ![_metadata shouldAutoOpenLinks]
               ? NATIVE_APP_OPEN_POLICY_CONTROLLER
               : NATIVE_APP_LAUNCHER_CONTROLLER;
  } else {  // App is not installed.
    // Check if the user already opened the store for this app.
    if ([_appsPossiblyBeingInstalled containsObject:[_metadata appId]])
      return;
    type = NATIVE_APP_INSTALLER_CONTROLLER;
  }
  // Inform the metadata that an infobar of |type| will be shown so that metrics
  // and ignored behavior can be handled.
  [_metadata willBeShownInInfobarOfType:type];
  // Display the proper infobar.
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(_webState);
  NativeAppInfoBarDelegate::Create(infoBarManager, self, pageURL, type);
  [self recordInfobarDisplayedOfType:type onLinkNavigation:isLinkNavigation];
}

- (void)recordInfobarDisplayedOfType:(NativeAppControllerType)type
                    onLinkNavigation:(BOOL)isLinkNavigation {
  switch (type) {
    case NATIVE_APP_INSTALLER_CONTROLLER:
      base::RecordAction(
          isLinkNavigation
              ? UserMetricsAction("MobileGALInstallInfoBarLinkNavigation")
              : UserMetricsAction("MobileGALInstallInfoBarDirectNavigation"));
      break;
    case NATIVE_APP_LAUNCHER_CONTROLLER:
      base::RecordAction(UserMetricsAction("MobileGALLaunchInfoBar"));
      break;
    case NATIVE_APP_OPEN_POLICY_CONTROLLER:
      base::RecordAction(UserMetricsAction("MobileGALOpenPolicyInfoBar"));
      break;
    default:
      NOTREACHED();
      break;
  }
}

#pragma mark -
#pragma mark NativeAppNavigationControllerProtocol methods

- (NSString*)appId {
  return [_metadata appId];
}

- (NSString*)appName {
  return [_metadata appName];
}

- (void)fetchSmallIconWithCompletionBlock:(void (^)(UIImage*))block {
  [_metadata fetchSmallIconWithImageFetcher:_imageFetcher.get()
                            completionBlock:block];
}

- (void)openStore {
  // Register to get a notification when the app is installed.
  [[InstallationNotifier sharedInstance]
      registerForInstallationNotifications:self
                              withSelector:@selector(appDidInstall:)
                                 forScheme:[_metadata anyScheme]];
  NSString* appIdString = [self appId];
  // Defensively early return if native app metadata returns an nil string for
  // appId which can cause subsequent -addObject: to throw exceptions.
  if (![appIdString length])
    return;
  DCHECK(![_appsPossiblyBeingInstalled containsObject:appIdString]);
  [_appsPossiblyBeingInstalled addObject:appIdString];
  // TODO(crbug.com/684063): Preferred method is to add a helper object to
  // WebState and use the helper object to launch Store Kit.
  [_tab openAppStore:appIdString];
}

- (void)launchApp:(const GURL&)URL {
  // TODO(crbug.com/353957): Pass the ChromeIdentity to
  // -launchURLWithURL:identity:
  GURL launchURL([_metadata launchURLWithURL:URL identity:nil]);
  if (launchURL.is_valid()) {
    OpenUrlWithCompletionHandler(net::NSURLWithGURL(launchURL), nil);
  }
}

- (void)updateMetadataWithUserAction:(NativeAppActionType)userAction {
  [_metadata updateWithUserAction:userAction];
}

#pragma mark -
#pragma mark CRWWebControllerObserver methods

- (void)pageLoaded:(CRWWebController*)webController {
  if (![_tab isPrerenderTab])
    [self showInfoBarIfNecessary];
}

- (void)webControllerWillClose:(CRWWebController*)webController {
  [webController removeObserver:self];
}

- (void)appDidInstall:(NSNotification*)notification {
  [self removeAppFromNotification:notification];
  [self showInfoBarIfNecessary];
}

- (void)removeAppFromNotification:(NSNotification*)notification {
  DCHECK([[notification object] isKindOfClass:[InstallationNotifier class]]);
  NSString* schemeOfInstalledApp = [notification name];
  __block NSString* appIDToRemove = nil;
  [_appsPossiblyBeingInstalled
      enumerateObjectsUsingBlock:^(id appID, BOOL* stop) {
        NSURL* appURL =
            [ios::GetChromeBrowserProvider()->GetNativeAppWhitelistManager()
                schemeForAppId:appID];
        if ([[appURL scheme] isEqualToString:schemeOfInstalledApp]) {
          appIDToRemove = appID;
          *stop = YES;
        }
      }];
  DCHECK(appIDToRemove);
  [_appsPossiblyBeingInstalled removeObject:appIDToRemove];
}

@end
