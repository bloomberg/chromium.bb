// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#import <WebKit/WebKit.h>

#include "base/ios/ios_util.h"
#import "base/ios/weak_nsobject.h"
#import "base/logging.h"
#import "ios/web/alloc_with_zone_interceptor.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/web_state/js/page_script_util.h"
#import "ios/web/web_state/ui/crw_wk_script_message_router.h"
#import "ios/web/web_state/web_view_internal_creation_util.h"

#if !defined(NDEBUG)

namespace {
BOOL gAllowWKProcessPoolCreation = NO;

// TODO(eugenebut): Cleanup this macro, once all bots switched to iOS9 SDK
// (crbug.com/523365).
#if defined(__IPHONE_9_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_9_0
// By default WKProcessPool creation is not allowed by embedder to prevent
// issues with browsing data clearing. However some iOS system methods do create
// WKProcessPool inside, which is perfectly fine and should be allowed. This
// method whitelists given |klass| with given |selector|, so creation of
// WKProcessPool is allowed inside that selector call. This function currently
// supports Objective-C methods with up to 4 arguments and needs to be updated
// if support for more arguments is required.
void AllowWKProcessPoolCreation(Class klass, SEL selector) {
  Method method = class_getInstanceMethod(klass, selector);
  IMP originalImp = method_getImplementation(method);
  IMP safeImp = imp_implementationWithBlock(
      ^(id self, id arg1, id arg2, id arg3, id arg4) {
        BOOL oldAllowWKProcessPoolCreation = gAllowWKProcessPoolCreation;
        gAllowWKProcessPoolCreation = YES;
        id result = originalImp(self, selector, arg1, arg2, arg3, arg4);
        gAllowWKProcessPoolCreation = oldAllowWKProcessPoolCreation;
        return result;
      });

  method_setImplementation(method, safeImp);
}
#endif  // __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_9_0
}

@interface WKProcessPool (CRWAdditions)
@end

@implementation WKProcessPool (CRWAdditions)

+ (void)load {
  id (^allocator)(Class klass, NSZone* zone) = ^id(Class klass, NSZone* zone) {
    if (gAllowWKProcessPoolCreation || web::IsWebViewAllocInitAllowed()) {
      return NSAllocateObject(klass, 0, zone);
    }
    // You have hit this because you are trying to create a WKProcessPool
    // directly or indirectly (f.e. by creating WKWebViewConfiguration
    // manually). Please use GetWebViewConfiguration() to get
    // WKWebViewConfiguration object.
    NOTREACHED();
    return nil;
  };
  web::AddAllocWithZoneMethod([WKProcessPool class], allocator);

// TODO(eugenebut): Cleanup this macro, once all bots switched to iOS9 SDK
// (crbug.com/523365).
#if defined(__IPHONE_9_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_9_0
  if (!base::ios::IsRunningOnIOS9OrLater())
    return;

  // Make sure that WKWebsiteDataStore is allowed to create WKProcessPool for
  // internal implementation purposes.
  AllowWKProcessPoolCreation(
      [WKWebsiteDataStore class],
      @selector(fetchDataRecordsOfTypes:completionHandler:completionHandler:));
  AllowWKProcessPoolCreation(
      [WKWebsiteDataStore class],
      @selector(removeDataOfTypes:forDataRecords:completionHandler:));
  AllowWKProcessPoolCreation(
      [WKWebsiteDataStore class],
      @selector(removeDataOfTypes:modifiedSince:completionHandler:));
#endif  // __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_9_0
}

@end

#endif  // !defined(NDEBUG)

namespace web {

namespace {
// A key used to associate a WKWebViewConfigurationProvider with a BrowserState.
const char kWKWebViewConfigProviderKeyName[] = "wk_web_view_config_provider";

// Returns an autoreleased instance of WKUserScript to be added to
// configuration's userContentController.
WKUserScript* GetEarlyPageScript() {
  return [[[WKUserScript alloc]
        initWithSource:GetEarlyPageScript(WK_WEB_VIEW_TYPE)
         injectionTime:WKUserScriptInjectionTimeAtDocumentStart
      forMainFrameOnly:YES] autorelease];
}

}  // namespace

// static
WKWebViewConfigurationProvider&
WKWebViewConfigurationProvider::FromBrowserState(BrowserState* browser_state) {
  DCHECK([NSThread isMainThread]);
  DCHECK(browser_state);
  if (!browser_state->GetUserData(kWKWebViewConfigProviderKeyName)) {
    bool is_off_the_record = browser_state->IsOffTheRecord();
    browser_state->SetUserData(
        kWKWebViewConfigProviderKeyName,
        new WKWebViewConfigurationProvider(is_off_the_record));
  }
  return *(static_cast<WKWebViewConfigurationProvider*>(
      browser_state->GetUserData(kWKWebViewConfigProviderKeyName)));
}

WKWebViewConfigurationProvider::WKWebViewConfigurationProvider(
    bool is_off_the_record)
    : is_off_the_record_(is_off_the_record) {}

WKWebViewConfigurationProvider::~WKWebViewConfigurationProvider() {
}

WKWebViewConfiguration*
WKWebViewConfigurationProvider::GetWebViewConfiguration() {
  DCHECK([NSThread isMainThread]);
  if (!configuration_) {
    configuration_.reset([[WKWebViewConfiguration alloc] init]);
// TODO(eugenebut): Cleanup this macro, once all bots switched to iOS9 SDK
// (crbug.com/523365).
#if defined(__IPHONE_9_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_9_0
    if (is_off_the_record_ && base::ios::IsRunningOnIOS9OrLater()) {
      // WKWebsiteDataStore is iOS9 only.
      [configuration_
          setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
    }
#endif  // defined(__IPHONE_9_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >=
        // __IPHONE_9_0
    // setJavaScriptCanOpenWindowsAutomatically is required to support popups.
    [[configuration_ preferences] setJavaScriptCanOpenWindowsAutomatically:YES];
    [[configuration_ userContentController] addUserScript:GetEarlyPageScript()];
#if !defined(NDEBUG)
    // Lazily load WKProcessPool. -[[WKProcessPool alloc] init] call is not
    // allowed except when creating config object inside this class.
    // Unmanaged creation of WKProcessPool may lead to issues with cookie
    // clearing and Browsing Data Partitioning implementation.
    gAllowWKProcessPoolCreation = YES;
    CHECK([configuration_ processPool]);
    gAllowWKProcessPoolCreation = NO;
#endif  // !defined(NDEBUG)
  }
  // Prevent callers from changing the internals of configuration.
  return [[configuration_ copy] autorelease];
}

CRWWKScriptMessageRouter*
WKWebViewConfigurationProvider::GetScriptMessageRouter() {
  DCHECK([NSThread isMainThread]);
  if (!router_) {
    WKUserContentController* userContentController =
        [GetWebViewConfiguration() userContentController];
    router_.reset([[CRWWKScriptMessageRouter alloc]
        initWithUserContentController:userContentController]);
  }
  return router_;
}

void WKWebViewConfigurationProvider::Purge() {
  DCHECK([NSThread isMainThread]);
#if !defined(NDEBUG) || !defined(DCHECK_ALWAYS_ON)  // Matches DCHECK_IS_ON.
  base::WeakNSObject<id> weak_configuration(configuration_);
  base::WeakNSObject<id> weak_router(router_);
  base::WeakNSObject<id> weak_process_pool([configuration_ processPool]);
#endif  // !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  configuration_.reset();
  router_.reset();
  // Make sure that no one retains configuration, router, processPool.
  DCHECK(!weak_configuration);
  DCHECK(!weak_router);
  // TODO(crbug.com/522672): Enable this DCHECK.
  // DCHECK(!weak_process_pool);
}

}  // namespace web
