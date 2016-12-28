// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/response_providers/delayed_response_provider.h"

#import <Foundation/Foundation.h>

#import "base/ios/weak_nsobject.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/foundation_util.h"
#import "ios/third_party/gcdwebserver/src/GCDWebServer/Responses/GCDWebServerDataResponse.h"

// A proxy class that will forward all messages to the |_response|.
// It will delay read operation  |_delay| seconds.
@interface DelayedGCDWebServerResponse : NSProxy {
  base::scoped_nsobject<GCDWebServerResponse> _response;
  NSTimeInterval _delay;
}

+ (instancetype)responseWithServerResponse:(GCDWebServerResponse*)response
                                     delay:(NSTimeInterval)delay;
- (instancetype)initWithServerResponse:(GCDWebServerResponse*)response
                                 delay:(NSTimeInterval)delay;
@end

@implementation DelayedGCDWebServerResponse
+ (instancetype)responseWithServerResponse:(GCDWebServerResponse*)response
                                     delay:(NSTimeInterval)delay {
  return [[[DelayedGCDWebServerResponse alloc] initWithServerResponse:response
                                                                delay:delay]
      autorelease];
}

- (instancetype)initWithServerResponse:(GCDWebServerResponse*)response
                                 delay:(NSTimeInterval)delay {
  _response.reset([response retain]);
  _delay = delay;
  return self;
}

#pragma mark - GCDWebServerResponse

- (void)performReadDataWithCompletion:
    (GCDWebServerBodyReaderCompletionBlock)block {
  [self asyncReadDataWithCompletion:block];
}

- (void)asyncReadDataWithCompletion:
    (GCDWebServerBodyReaderCompletionBlock)block {
  dispatch_async(dispatch_get_main_queue(), ^{
    base::WeakNSObject<GCDWebServerResponse> weakResponse(_response);
    if ([_response
            respondsToSelector:@selector(asyncReadDataWithCompletion:)]) {
      dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                   static_cast<int64_t>(_delay * NSEC_PER_SEC)),
                     dispatch_get_main_queue(), ^{
                       base::scoped_nsobject<GCDWebServerResponse> response(
                           [weakResponse retain]);
                       [response asyncReadDataWithCompletion:block];
                     });
    } else {
      dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                   static_cast<int64_t>(_delay * NSEC_PER_SEC)),
                     dispatch_get_main_queue(), ^{
                       base::scoped_nsobject<GCDWebServerResponse> response(
                           [weakResponse retain]);
                       if (!response) {
                         return;
                       }
                       NSError* error = nil;
                       NSData* data = [response readData:&error];
                       block(data, error);
                     });
    }
  });
}

#pragma mark - NSProxy

- (BOOL)conformsToProtocol:(Protocol*)protocol {
  return [[_response class] conformsToProtocol:protocol];
}

- (BOOL)respondsToSelector:(SEL)selector {
  return [[_response class] instancesRespondToSelector:selector];
}

- (NSMethodSignature*)methodSignatureForSelector:(SEL)selector {
  return [[_response class] instanceMethodSignatureForSelector:selector];
}

- (void)forwardInvocation:(NSInvocation*)invocation {
  SEL selector = [invocation selector];
  if ([_response respondsToSelector:selector])
    [invocation invokeWithTarget:_response];
}

@end

namespace web {

DelayedResponseProvider::DelayedResponseProvider(
    std::unique_ptr<web::ResponseProvider> delayed_provider,
    double delay)
    : web::ResponseProvider(),
      delayed_provider_(std::move(delayed_provider)),
      delay_(delay) {}

DelayedResponseProvider::~DelayedResponseProvider() {}

bool DelayedResponseProvider::CanHandleRequest(const Request& request) {
  return delayed_provider_->CanHandleRequest(request);
}

GCDWebServerResponse* DelayedResponseProvider::GetGCDWebServerResponse(
    const Request& request) {
  DelayedGCDWebServerResponse* response = [DelayedGCDWebServerResponse
      responseWithServerResponse:delayed_provider_->GetGCDWebServerResponse(
                                     request)
                           delay:delay_];
  return base::mac::ObjCCastStrict<GCDWebServerResponse>(response);
}

}  // namespace web
