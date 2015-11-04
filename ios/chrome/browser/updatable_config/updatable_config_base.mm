// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/updatable_config/updatable_config_base.h"

#include "base/logging.h"
#import "base/mac/bind_objc_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/updatable_resource_provider.h"
#include "ios/web/public/web_thread.h"
#import "net/base/mac/url_conversions.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

@interface UpdatableConfigBase ()
// Returns the application ID to use for fetching updatable configuration.
+ (NSString*)defaultAppId;
// Fetches server-side updatable configuration plist.
- (void)checkUpdate;
#if !defined(NDEBUG)
// A method that will be executed on a delay if -startUpdate: was NOT called.
- (void)startUpdateNotCalled:(id)config;
// Schedules a call to -startUpdateNotCalled: for later to make sure that
// -startUpdate: will be called.
- (void)scheduleConsistencyCheck;
// Cancels the delayed call to -startUpdateNotCalled:.
- (void)cancelConsistencyCheck;
#endif  // !defined(NDEBUG)
@end

namespace {

#if !defined(NDEBUG)
// Global flag to enable or disable debug check that -startUpdate:
// has been called.
BOOL g_consistency_check_enabled = NO;
#endif

// Periodically fetch configuration updates from server.
const int64_t kPeriodicCheckInNanoseconds = 60 * 60 * 24 * NSEC_PER_SEC;

// Class to fetch config update |url| and also act as the delegate to
// handle callbacks from URLFetcher.
class ConfigFetcher : public net::URLFetcherDelegate {
 public:
  ConfigFetcher(UpdatableConfigBase* owner,
                id<UpdatableResourceDescriptorBridge> descriptor)
      : owner_(owner), descriptor_(descriptor) {}

  // Starts fetching |url| for updated configuration.
  void Fetch(const GURL& url, net::URLRequestContextGetter* context) {
    fetcher_ = net::URLFetcher::Create(url, net::URLFetcher::GET, this);
    fetcher_->SetRequestContext(context);
    fetcher_->Start();
  }

  void OnURLFetchComplete(const net::URLFetcher* fetcher) override {
    DCHECK_EQ(fetcher_, fetcher);
    NSData* responseData = nil;
    if (fetcher_->GetResponseCode() == net::HTTP_OK) {
      std::string response;
      fetcher_->GetResponseAsString(&response);
      responseData =
          [NSData dataWithBytes:response.c_str() length:response.length()];
    }
    fetcher_.reset();
    // If data was fetched, write the fetched data to local store in a
    // separate thread. Then update the resource descriptor that configuration
    // update is completed. Finally, schedule the next update check.
    web::WebThread::PostBlockingPoolTask(FROM_HERE, base::BindBlock(^{
      BOOL updateSuccess = NO;
      if (responseData) {
        NSString* path = [descriptor_ updateResourcePath];
        updateSuccess = [responseData writeToFile:path atomically:YES];
      }
      dispatch_after(DISPATCH_TIME_NOW, dispatch_get_main_queue(), ^() {
        [descriptor_ updateCheckDidFinishWithSuccess:updateSuccess];
      });
      dispatch_after(
          dispatch_time(DISPATCH_TIME_NOW, kPeriodicCheckInNanoseconds),
          dispatch_get_main_queue(), ^{
            [owner_ checkUpdate];
          });
    }));
  };

 private:
  UpdatableConfigBase* owner_;
  id<UpdatableResourceDescriptorBridge> descriptor_;
  scoped_ptr<net::URLFetcher> fetcher_;
};

}  // namespace

@implementation UpdatableConfigBase {
  base::scoped_nsprotocol<id<UpdatableResourceBridge>> _updatableResource;
  scoped_ptr<ConfigFetcher> _configFetcher;
  scoped_refptr<net::URLRequestContextGetter> _requestContextGetter;
}

+ (void)enableConsistencyCheck {
#if !defined(NDEBUG)
  g_consistency_check_enabled = YES;
#endif
}

// Overrides default designated initializer.
- (instancetype)initWithAppId:(NSString*)appId
                      version:(NSString*)appVersion
                        plist:(NSString*)plistName {
  self = [super init];
  if (self) {
    _updatableResource.reset([self newResource:plistName]);
    // UpdatableResourceBridge initializes the appId to what is in the
    // application bundle. The following overrides that with either the |appId|
    // passed in or a default based on experimental settings if |appId| is nil.
    if (!appId)
      appId = [UpdatableConfigBase defaultAppId];
    [[_updatableResource descriptor] setApplicationIdentifier:appId];
    // Overrides the default application version if necessary.
    if (appVersion)
      [[_updatableResource descriptor] setApplicationVersion:appVersion];
    // [UpdatableResourceBridge -loadDefaults] updates the resource in
    // two phases and is probably not MT safe. However,
    // initWithAppId:version:plist: is called from a singleton's
    // initialization loop and thus will not be called more than once.
    // TODO(crbug/545309): -loadDefaults accesses the file system to load in
    // the plist. This should be done via PostBlockingPoolTask.
    [_updatableResource loadDefaults];

    NSString* notificationName = ios::GetChromeBrowserProvider()
                                     ->GetUpdatableResourceProvider()
                                     ->GetUpdateNotificationName();
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(resourceDidUpdate:)
               name:notificationName
             object:[_updatableResource descriptor]];

#if !defined(NDEBUG)
    [self scheduleConsistencyCheck];
#endif
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
#if !defined(NDEBUG)
  [self cancelConsistencyCheck];
#endif
  [super dealloc];
}

- (void)startUpdate:(net::URLRequestContextGetter*)requestContextGetter {
#if !defined(NDEBUG)
  [self cancelConsistencyCheck];
#endif
  _requestContextGetter = requestContextGetter;
  [self checkUpdate];
}

- (void)stopUpdateChecks {
  _requestContextGetter = nullptr;
}

- (void)resourceDidUpdate:(NSNotification*)notification {
  id sender = [notification object];
  DCHECK([_updatableResource descriptor] == sender);
}

#pragma mark -
#pragma mark For Subclasses

- (id<UpdatableResourceBridge>)newResource:(NSString*)resourceName {
  // Subclasses must override this factory method.
  NOTREACHED();
  return nil;
}

- (id<UpdatableResourceBridge>)updatableResource {
  return _updatableResource.get();
}

#pragma mark -
#pragma mark For Debug Compilations

#if !defined(NDEBUG)
- (void)scheduleConsistencyCheck {
  if (!g_consistency_check_enabled)
    return;
  // Sets a delayed call that will cause a DCHECK if -startUpdate:
  // was not called.
  [self performSelector:@selector(startUpdateNotCalled:)
             withObject:self
             afterDelay:60.0];
}

- (void)cancelConsistencyCheck {
  if (!g_consistency_check_enabled)
    return;
  // Cancels the delayed error check since -startUpdate: has been called.
  // Added for completeness since singletons should never be deallocated.
  [NSObject
      cancelPreviousPerformRequestsWithTarget:self
                                     selector:@selector(startUpdateNotCalled:)
                                       object:self];
}

- (void)startUpdateNotCalled:(id)config {
  DCHECK(self == config);
  DCHECK(g_consistency_check_enabled);
  // Make sure that |startUpdate:| was called for this config.
  NOTREACHED() << "startUpdate: was not called for "
               << [[self description] UTF8String];
}
#endif  // !defined(NDEBUG)

#pragma mark -
#pragma mark For Unit Testing

- (void)setUpdatableResource:(id<UpdatableResourceBridge>)resource {
  _updatableResource.reset([resource retain]);
}

#pragma mark -
#pragma mark Private

+ (NSString*)defaultAppId {
  // During development and dogfooding, allow a different configuration
  // update file to be used for testing.
  NSString* flag = [[NSUserDefaults standardUserDefaults]
      stringForKey:@"UpdatableConfigLocation"];
  if ([flag length]) {
    if ([flag isEqualToString:@"Stable"])
      return @"com.google.chrome.ios";
    else if ([flag isEqualToString:@"Dogfood"])
      return @"com.google.chrome.ios.beta";
    else if ([flag isEqualToString:@"None"])
      return @"this.does.not.update";
  }
  return [[NSBundle mainBundle] bundleIdentifier];
}

- (void)checkUpdate {
  if (!_requestContextGetter.get())
    return;
  if (!_configFetcher) {
    _configFetcher.reset(
        new ConfigFetcher(self, [_updatableResource descriptor]));
  }
  GURL url = net::GURLWithNSURL([[_updatableResource descriptor] updateURL]);
  _configFetcher->Fetch(url, _requestContextGetter.get());
}

@end
