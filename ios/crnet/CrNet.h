// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CRNET_CRNET_H_
#define IOS_CRNET_CRNET_H_

#import <Foundation/Foundation.h>

// A block, that takes a request, and returns YES if the request should
// be handled.
typedef BOOL(^RequestFilterBlock)(NSURLRequest *request);


// A callback, called when the clearCache message has completed. The |errorCode|
// is a network stack error code indicating whether the clear request succeeded
// or not. Even if the request failed, the cache may have been cleared anyway,
// or it may not have; it is not useful to retry a failing cache-clear attempt.
// The only real use of |errorCode| is for error reporting and statistics
// gathering.
typedef void(^ClearCacheCallback)(int errorCode);

// Interface for installing CrNet.
__attribute__((visibility("default")))
@interface CrNet : NSObject

// Sets whether HTTP/2 should be supported by CrNet. This method only has
// any effect before |install| is called.
+ (void)setHttp2Enabled:(BOOL)http2Enabled;

// Sets whether QUIC should be supported by CrNet. This method only has any
// effect before |install| is called.
+ (void)setQuicEnabled:(BOOL)quicEnabled;

// Sets whether SDCH should be supported by CrNet. This method only has any
// effect before |install| is called. The |filename| argument is used to specify
// which file should be used for SDCH persistence metadata. If |filename| is
// nil, persistence is not enabled. The default is for SDCH to be disabled.
+ (void)setSDCHEnabled:(BOOL)sdchEnabled
         withPrefStore:(NSString *)filename;

// Set partial UserAgent. This function is a deprecated shortcut for:
//    [CrNet setUserAgent:userAgent partial:YES];
// See the documentation for |setUserAgent| for details about the |userAgent|
// argument.
// This method only has any effect before |install| is called.
+ (void)setPartialUserAgent:(NSString *)userAgent;

// |userAgent| is expected to be the user agent value sent to remote.
// If |partial| is set to YES, then actual user agent value is based on device
// model, OS version, and |userAgent| argument. For example "Foo/3.0.0.0" is
// sent as "Mozilla/5.0 (iPhone; CPU iPhone OS 9_3 like Mac OS X)
// AppleWebKit/601.1 (KHTML, like Gecko) Foo/3.0.0.0 Mobile/15G31
// Safari/601.1.46".
// If partial is set to NO, then |userAgent| value is complete value sent to
// the remote. For Example: "Foo/3.0.0.0" is sent as "Foo/3.0.0.0".
//
// This method only has any effect before |install| is called.
+ (void)setUserAgent:(NSString*)userAgent partial:(bool)partial;

// Set the block used to determine whether or not CrNet should handle the
// request. If this is not set, CrNet will handle all requests.
// Must not be called while requests are in progress. This method can be called
// either before or after |install|.
+ (void)setRequestFilterBlock:(RequestFilterBlock)block;

// Installs CrNet. Once installed, CrNet intercepts and handles all
// NSURLConnection and NSURLRequests issued by the app, including UIWebView page
// loads. It is recommended to call this method on the application main thread.
// If the method is called on any thread other than the main one, the method
// will internally try to execute synchronously using the main GCD queue.
// Please make sure that the main thread is not blocked by a job
// that calls this method; otherwise, a deadlock can occur.
+ (void)install;

// Installs CrNet into an NSURLSession, passed in by the caller. Note that this
// NSURLSession will share settings with the sharedSession, which the |install|
// method installs CrNet into. This method must be called after |install|.
+ (void)installIntoSessionConfiguration:(NSURLSessionConfiguration*)config;

// Installs CrNet. This function is a deprecated shortcut for:
//    [CrNet setPartialUserAgent:userAgent];
//    [CrNet install];
// See the documentation for |setPartialUserAgent| for details about the
// |userAgent| argument.
+ (void)installWithPartialUserAgent:(NSString *)userAgent
    __attribute__((deprecated));

// Installs CrNet. This function is a deprecated shortcut for:
//    [CrNet setPartialUserAgent:userAgent];
//    [CrNet install];
// The |enableDataReductionProxy| argument is ignored since data reduction proxy
// support is currently missing from CrNet. See |setPartialUserAgent| for
// details about the |userAgent| argument.
+ (void)installWithPartialUserAgent:(NSString *)userAgent
           enableDataReductionProxy:(BOOL)enableDataReductionProxy
           __attribute__((deprecated));

// Installs CrNet. This function is a deprecated shortcut for:
//    [CrNet setPartialUserAgent:userAgent];
//    [CrNet setRequestFilterBlock:block];
//    [CrNet install];
// See |setPartialUserAgent| and |setRequestFilterBlock| for details about the
// |userAgent| and |requestFilterBlock| arguments respectively.
+ (void)installWithPartialUserAgent:(NSString *)userAgent
             withRequestFilterBlock:(RequestFilterBlock)requestFilterBlock
    __attribute__((deprecated));

// Starts net-internals logging to a file named |fileName| in the application
// temporary directory. |fileName| must not be empty. Log level is determined
// by |logBytes| - if YES then LOG_ALL otherwise LOG_ALL_BUT_BYTES. If the file
// exists it is truncated before starting. If actively logging the call is
// ignored.
+ (void)startNetLogToFile:(NSString *)fileName logBytes:(BOOL)logBytes;

// Stop net-internals logging and flush file to disk. If a logging session is
// not in progress this call is ignored.
+ (void)stopNetLog;

// Closes all current SPDY sessions. Do not do this unless you know what
// you're doing.
// TODO(alokm): This is a hack. Remove it later.
+ (void)closeAllSpdySessions;

// "Uninstalls" CrNet. This means that CrNet will stop intercepting requests.
// However, it won't tear down all of the CrNet environment.
+ (void)uninstall;

// Returns the full user-agent that the stack uses.
// This is the exact string servers will see.
+ (NSString *)userAgent;

// Clears CrNet's http cache. The supplied callback, if not nil, is run on an
// unspecified thread.
+ (void)clearCacheWithCompletionCallback:(ClearCacheCallback)completionBlock;

@end

#endif  // IOS_CRNET_CRNET_H_
