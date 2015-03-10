// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/crn_http_protocol_handler_proxy_with_client_thread.h"

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#include "base/time/time.h"
#import "ios/net/protocol_handler_util.h"
#include "net/base/auth.h"
#include "net/url_request/url_request.h"

// When the protocol is invalidated, no synchronization (lock) is needed:
// - The actual calls to the protocol and its invalidation are all done on
//   clientThread_ and thus are serialized.
// - When a proxy method is called, the protocol is compared to nil. There may
//   be a conflict at this point, in the case the protocol is being invalidated
//   during this comparison. However, in such a case, the actual value of the
//   pointer does not matter: an invalid pointer will behave as a valid one and
//   post a task on the clientThread_, and that task will be handled correctly,
//   as described by the item above.

@interface CRNHTTPProtocolHandlerProxyWithClientThread () {
  __weak NSURLProtocol* _protocol;
  // Thread used to call the client back.
  // This thread does not have a base::MessageLoop, and thus does not work with
  // the usual task posting functions.
  __weak NSThread* _clientThread;
  // The run loop modes to use when posting tasks to |clientThread_|.
  base::scoped_nsobject<NSArray> _runLoopModes;
  // The request URL.
  base::scoped_nsobject<NSString> _url;
  // The creation time of the request.
  base::Time _creationTime;
  // |requestComplete_| is used in debug to check that the client is not called
  // after completion.
  BOOL _requestComplete;
}

// Performs the selector on |clientThread_| using |runLoopModes_|.
- (void)performSelectorOnClientThread:(SEL)aSelector withObject:(id)arg;
// These functions are just wrappers around the corresponding
// NSURLProtocolClient methods, used for task posting.
- (void)didFailWithErrorOnClientThread:(NSError*)error;
- (void)didLoadDataOnClientThread:(NSData*)data;
- (void)didReceiveResponseOnClientThread:(NSURLResponse*)response;
- (void)wasRedirectedToRequestOnClientThread:(NSArray*)params;
- (void)didFinishLoadingOnClientThread;
@end

@implementation CRNHTTPProtocolHandlerProxyWithClientThread

- (instancetype)initWithProtocol:(NSURLProtocol*)protocol
                    clientThread:(NSThread*)clientThread
                     runLoopMode:(NSString*)mode {
  DCHECK(protocol);
  DCHECK(clientThread);
  if ((self = [super init])) {
    _protocol = protocol;
    _url.reset([[[[protocol request] URL] absoluteString] copy]);
    _creationTime = base::Time::Now();
    _clientThread = clientThread;
    // Use the common run loop mode in addition to the client thread mode, in
    // hope that our tasks are executed even if the client thread changes mode
    // later on.
    if ([mode isEqualToString:NSRunLoopCommonModes])
      _runLoopModes.reset([@[ NSRunLoopCommonModes ] retain]);
    else
      _runLoopModes.reset([@[ mode, NSRunLoopCommonModes ] retain]);
  }
  return self;
}

- (void)invalidate {
  DCHECK([NSThread currentThread] == _clientThread);
  _protocol = nil;
  _requestComplete = YES;
}

- (void)performSelectorOnClientThread:(SEL)aSelector withObject:(id)arg {
  [self performSelector:aSelector
               onThread:_clientThread
             withObject:arg
          waitUntilDone:NO
                  modes:_runLoopModes];
}

#pragma mark Proxy methods called from any thread.

- (void)didFailWithNSErrorCode:(NSInteger)nsErrorCode
                  netErrorCode:(int)netErrorCode {
  DCHECK(_clientThread);
  if (!_protocol)
    return;
  NSError* error =
      net::GetIOSError(nsErrorCode, netErrorCode, _url, _creationTime);
  [self performSelectorOnClientThread:@selector(didFailWithErrorOnClientThread:)
                           withObject:error];
}

- (void)didLoadData:(NSData*)data {
  DCHECK(_clientThread);
  if (!_protocol)
    return;
  [self performSelectorOnClientThread:@selector(didLoadDataOnClientThread:)
                           withObject:data];
}

- (void)didReceiveResponse:(NSURLResponse*)response {
  DCHECK(_clientThread);
  if (!_protocol)
    return;
  [self
      performSelectorOnClientThread:@selector(didReceiveResponseOnClientThread:)
                         withObject:response];
}

- (void)wasRedirectedToRequest:(NSURLRequest*)request
                 nativeRequest:(net::URLRequest*)nativeRequest
              redirectResponse:(NSURLResponse*)redirectResponse {
  DCHECK(_clientThread);
  if (!_protocol)
    return;
  [self performSelectorOnClientThread:@selector(
                                          wasRedirectedToRequestOnClientThread:)
                           withObject:@[ request, redirectResponse ]];
}

- (void)didFinishLoading {
  DCHECK(_clientThread);
  if (!_protocol)
    return;
  [self performSelectorOnClientThread:@selector(didFinishLoadingOnClientThread)
                           withObject:nil];
}

// Feature support methods that don't forward to the NSURLProtocolClient.
- (void)didCreateNativeRequest:(net::URLRequest*)nativeRequest {
  // no-op.
}

- (void)didRecieveAuthChallenge:(net::AuthChallengeInfo*)authInfo
                  nativeRequest:(const net::URLRequest&)nativeRequest
                       callback:(const network_client::AuthCallback&)callback {
  // If we get this far, authentication has failed.
  base::string16 empty;
  callback.Run(false, empty, empty);
}

- (void)cancelAuthRequest {
  // no-op.
}

- (void)setUnderlyingClient:(id<CRNNetworkClientProtocol>)underlyingClient {
  // This is the lowest level.
  DCHECK(!underlyingClient);
}

#pragma mark Proxy methods called from the client thread.

- (void)didFailWithErrorOnClientThread:(NSError*)error {
  DCHECK([NSThread currentThread] == _clientThread);
  DCHECK(!_requestComplete || !_protocol);
  _requestComplete = YES;
  [[_protocol client] URLProtocol:_protocol didFailWithError:error];
}

- (void)didLoadDataOnClientThread:(NSData*)data {
  DCHECK([NSThread currentThread] == _clientThread);
  DCHECK(!_requestComplete || !_protocol);
  [[_protocol client] URLProtocol:_protocol didLoadData:data];
}

- (void)didReceiveResponseOnClientThread:(NSURLResponse*)response {
  DCHECK([NSThread currentThread] == _clientThread);
  DCHECK(!_requestComplete || !_protocol);
  [[_protocol client] URLProtocol:_protocol
               didReceiveResponse:response
               cacheStoragePolicy:NSURLCacheStorageNotAllowed];
}

- (void)wasRedirectedToRequestOnClientThread:(NSArray*)params {
  DCHECK([NSThread currentThread] == _clientThread);
  DCHECK_EQ(2u, [params count]);
  DCHECK([params[0] isKindOfClass:[NSURLRequest class]]);
  DCHECK([params[1] isKindOfClass:[NSURLResponse class]]);
  DCHECK(!_requestComplete || !_protocol);
  [[_protocol client] URLProtocol:_protocol
           wasRedirectedToRequest:params[0]
                 redirectResponse:params[1]];
}

- (void)didFinishLoadingOnClientThread {
  DCHECK([NSThread currentThread] == _clientThread);
  DCHECK(!_requestComplete || !_protocol);
  _requestComplete = YES;
  [[_protocol client] URLProtocolDidFinishLoading:_protocol];
}

@end
