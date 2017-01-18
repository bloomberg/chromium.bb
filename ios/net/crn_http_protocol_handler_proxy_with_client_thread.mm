// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/crn_http_protocol_handler_proxy_with_client_thread.h"

#include <stddef.h>

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#include "base/time/time.h"
#import "ios/net/protocol_handler_util.h"
#include "net/base/auth.h"
#include "net/url_request/url_request.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  NSURLProtocol* _protocol;  // weak
  // Thread used to call the client back.
  // This thread does not have a base::MessageLoop, and thus does not work with
  // the usual task posting functions.
  NSThread* _clientThread;  // weak
  // The run loop modes to use when posting tasks to |clientThread_|.
  base::scoped_nsobject<NSArray> _runLoopModes;
  // The request URL.
  base::scoped_nsobject<NSString> _url;
  // The creation time of the request.
  base::Time _creationTime;
  // |requestComplete_| is used in debug to check that the client is not called
  // after completion.
  BOOL _requestComplete;
  BOOL _paused;

  base::scoped_nsobject<NSMutableArray> _queuedInvocations;
}

// Performs the selector on |clientThread_| using |runLoopModes_|.
- (void)runInvocationQueueOnClientThread;
- (void)postToClientThread:(SEL)aSelector, ... NS_REQUIRES_NIL_TERMINATION;
- (void)invokeOnClientThread:(NSInvocation*)invocation;
// These functions are just wrappers around the corresponding
// NSURLProtocolClient methods, used for task posting.
- (void)didFailWithErrorOnClientThread:(NSError*)error;
- (void)didLoadDataOnClientThread:(NSData*)data;
- (void)didReceiveResponseOnClientThread:(NSURLResponse*)response;
- (void)wasRedirectedToRequestOnClientThread:(NSURLRequest*)request
                            redirectResponse:(NSURLResponse*)response;
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
      _runLoopModes.reset(@[ NSRunLoopCommonModes ]);
    else
      _runLoopModes.reset(@[ mode, NSRunLoopCommonModes ]);
    _queuedInvocations.reset([[NSMutableArray alloc] init]);
  }
  return self;
}

- (void)invalidate {
  DCHECK([NSThread currentThread] == _clientThread);
  _protocol = nil;
  _requestComplete = YES;
  // Note that there may still be queued invocations here, if the chrome network
  // stack continues to emit events after the system network stack has paused
  // the request, and then the system network stack destroys the request.
  _queuedInvocations.reset();
}

- (void)runInvocationQueueOnClientThread {
  DCHECK([NSThread currentThread] == _clientThread);
  DCHECK(!_requestComplete || !_protocol);
  // Each of the queued invocations may cause the system network stack to pause
  // this request, in which case |runInvocationQueueOnClientThread| should
  // immediately stop running further queued invocations. The queue will be
  // drained again the next time the system network stack calls |resume|.
  //
  // Specifically, the system stack can call back into |pause| with this
  // function still on the call stack. However, since new invocations are
  // enqueued on this thread via posted invocations, no new invocations can be
  // added while this function is running.
  while (!_paused && _queuedInvocations.get().count > 0) {
    NSInvocation* invocation = [_queuedInvocations objectAtIndex:0];
    // Since |_queuedInvocations| owns the only reference to each queued
    // invocation, this function has to retain another reference before removing
    // the queued invocation from the array.
    [invocation invoke];
    [_queuedInvocations removeObjectAtIndex:0];
  }
}

- (void)postToClientThread:(SEL)aSelector, ... {
  // Build an NSInvocation representing an invocation of |aSelector| on |self|
  // with the supplied varargs passed as arguments to the invocation.
  NSMethodSignature* sig = [self methodSignatureForSelector:aSelector];
  DCHECK(sig != nil);
  NSInvocation* inv = [NSInvocation invocationWithMethodSignature:sig];
  [inv setTarget:self];
  [inv setSelector:aSelector];
  [inv retainArguments];

  size_t arg_index = 2;
  va_list args;
  va_start(args, aSelector);
  __unsafe_unretained NSObject* arg = va_arg(args, NSObject*);
  while (arg != nil) {
    [inv setArgument:&arg atIndex:arg_index];
    arg = va_arg(args, NSObject*);
    arg_index++;
  }
  va_end(args);

  DCHECK(arg_index == sig.numberOfArguments);
  [self performSelector:@selector(invokeOnClientThread:)
               onThread:_clientThread
             withObject:inv
          waitUntilDone:NO
                  modes:_runLoopModes];
}

- (void)invokeOnClientThread:(NSInvocation*)invocation {
  DCHECK([NSThread currentThread] == _clientThread);
  DCHECK(!_requestComplete || !_protocol);
  if (!_paused) {
    [invocation invoke];
  } else {
    [_queuedInvocations addObject:invocation];
  }
}

#pragma mark Proxy methods called from any thread.

- (void)didFailWithNSErrorCode:(NSInteger)nsErrorCode
                  netErrorCode:(int)netErrorCode {
  DCHECK(_clientThread);
  if (!_protocol)
    return;
  NSError* error =
      net::GetIOSError(nsErrorCode, netErrorCode, _url, _creationTime);
  [self postToClientThread:@selector(didFailWithErrorOnClientThread:), error,
                           nil];
}

- (void)didLoadData:(NSData*)data {
  DCHECK(_clientThread);
  if (!_protocol)
    return;
  [self postToClientThread:@selector(didLoadDataOnClientThread:), data, nil];
}

- (void)didReceiveResponse:(NSURLResponse*)response {
  DCHECK(_clientThread);
  if (!_protocol)
    return;
  [self postToClientThread:@selector(didReceiveResponseOnClientThread:),
                           response, nil];
}

- (void)wasRedirectedToRequest:(NSURLRequest*)request
                 nativeRequest:(net::URLRequest*)nativeRequest
              redirectResponse:(NSURLResponse*)redirectResponse {
  DCHECK(_clientThread);
  if (!_protocol)
    return;
  [self postToClientThread:@selector(wasRedirectedToRequestOnClientThread:
                                                         redirectResponse:),
                           request, redirectResponse, nil];
}

- (void)didFinishLoading {
  DCHECK(_clientThread);
  if (!_protocol)
    return;
  [self postToClientThread:@selector(didFinishLoadingOnClientThread), nil];
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

#pragma mark Proxy methods called from the client thread.

- (void)didFailWithErrorOnClientThread:(NSError*)error {
  _requestComplete = YES;
  [[_protocol client] URLProtocol:_protocol didFailWithError:error];
}

- (void)didLoadDataOnClientThread:(NSData*)data {
  [[_protocol client] URLProtocol:_protocol didLoadData:data];
}

- (void)didReceiveResponseOnClientThread:(NSURLResponse*)response {
  [[_protocol client] URLProtocol:_protocol
               didReceiveResponse:response
               cacheStoragePolicy:NSURLCacheStorageNotAllowed];
}

- (void)wasRedirectedToRequestOnClientThread:(NSURLRequest*)request
                            redirectResponse:(NSURLResponse*)redirectResponse {
  [[_protocol client] URLProtocol:_protocol
           wasRedirectedToRequest:request
                 redirectResponse:redirectResponse];
}

- (void)didFinishLoadingOnClientThread {
  _requestComplete = YES;
  [[_protocol client] URLProtocolDidFinishLoading:_protocol];
}

- (void)pause {
  DCHECK([NSThread currentThread] == _clientThread);
  // It's legal (in fact, required) for |pause| to be called after the request
  // has already finished, so the usual invalidation DCHECK is missing here.
  _paused = YES;
}

- (void)resume {
  DCHECK([NSThread currentThread] == _clientThread);
  DCHECK(!_requestComplete || !_protocol);
  _paused = NO;
  [self runInvocationQueueOnClientThread];
}

@end
