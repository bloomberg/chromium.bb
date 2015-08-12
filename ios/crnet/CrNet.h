// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
@interface CrNet : NSObject

// Sets whether SPDY should be supported by CrNet. This method only has any
// effect before |install| is called.
+ (void)setSpdyEnabled:(BOOL)spdyEnabled;

// Sets whether QUIC should be supported by CrNet. This method only has any
// effect before |install| is called.
+ (void)setQuicEnabled:(BOOL)quicEnabled;

// Sets whether SDCH should be supported by CrNet. This method only has any
// effect before |install| is called. The |filename| argument is used to specify
// which file should be used for SDCH persistence metadata. If |filename| is
// nil, persistence is not enabled. The default is for SDCH to be disabled.
+ (void)setSDCHEnabled:(BOOL)sdchEnabled
         withPrefStore:(NSString*)filename;

// Set the alternate protocol threshold. Servers announce alternate protocols
// with a probability value; any alternate protocol whose probability value is
// greater than this value will be used, so |alternateProtocolThreshold| == 0
// implies any announced alternate protocol will be used, and
// |alternateProtocolThreshold| == 1 implies no alternate protocol will ever be
// used. Note that individual alternate protocols must also be individually
// enabled to be considered; currently the only alternate protocol is QUIC (SPDY
// is not controlled by this mechanism).
//
// For example, imagine your service has two frontends a.service.com and
// b.service.com, and you would like to divide your users into three classes:
//   Users who use QUIC for both a and b
//   Users who use QUIC for a but not b
//   Users who use QUIC for neither a nor b
// You can achieve that effect with:
//   a.service.com advertises QUIC with p=0.67
//   b.service.com advertises QUIC with p=0.33
//   alternateProtocolThreshold set to a uniform random number in [0,1]
// Now equal proportions of users will fall into the three experimental groups.
//
// The default for this value is 1.0, i.e. all alternate protocols disabled.
+ (void)setAlternateProtocolThreshold:(double)alternateProtocolThreshold;

// |userAgent| is expected to be of the form Product/Version.
// Example: Foo/3.0.0.0
//
// This method only has any effect before |install| is called.
+ (void)setPartialUserAgent:(NSString *)userAgent;

// Set the block used to determine whether or not CrNet should handle the
// request. If this is not set, CrNet will handle all requests.
// Must not be called while requests are in progress. This method can be called
// either before or after |install|.
+ (void)setRequestFilterBlock:(RequestFilterBlock)block;

// Installs CrNet. Once installed, CrNet intercepts and handles all
// NSURLConnection and NSURLRequests issued by the app, including UIWebView page
// loads.
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
