// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "cronet_c_for_grpc.h"

// A block, that takes a request, and returns YES if the request should
// be handled.
typedef BOOL (^RequestFilterBlock)(NSURLRequest* request);

// Interface for installing Cronet.
CRONET_EXPORT
@interface Cronet : NSObject

// Sets whether HTTP/2 should be supported by CronetEngine. This method only has
// any effect before |start| is called.
+ (void)setHttp2Enabled:(BOOL)http2Enabled;

// Sets whether QUIC should be supported by CronetEngine. This method only has
// any effect before |start| is called.
+ (void)setQuicEnabled:(BOOL)quicEnabled;

// Adds hint that host supports QUIC on altPort. This method only has any effect
// before |start| is called.
+ (void)addQuicHint:(NSString*)host port:(int)port altPort:(int)altPort;

// Sets the User-Agent request header string to be sent with all requests.
// If |partial| is set to YES, then actual user agent value is based on device
// model, OS version, and |userAgent| argument. For example "Foo/3.0.0.0" is
// sent as "Mozilla/5.0 (iPhone; CPU iPhone OS 9_3 like Mac OS X)
// AppleWebKit/601.1 (KHTML, like Gecko) Foo/3.0.0.0 Mobile/15G31
// Safari/601.1.46".
// If |partial| is set to NO, then |userAgent| value is complete value sent to
// the remote. For Example: "Foo/3.0.0.0" is sent as "Foo/3.0.0.0".
//
// This method only has any effect before |start| is called.
+ (void)setUserAgent:(NSString*)userAgent partial:(BOOL)partial;

// Sets SSLKEYLogFileName to export SSL key for Wireshark decryption of packet
// captures. This method only has any effect before |start| is called.
+ (void)setSslKeyLogFileName:(NSString*)sslKeyLogFileName;

// Sets the block used to determine whether or not Cronet should handle the
// request. If the block is not set, Cronet will handle all requests. Cronet
// retains strong reference to the block, which can be released by calling this
// method with nil block.
+ (void)setRequestFilterBlock:(RequestFilterBlock)block;

// Starts CronetEngine. It is recommended to call this method on the application
// main thread. If the method is called on any thread other than the main one,
// the method will internally try to execute synchronously using the main GCD
// queue. Please make sure that the main thread is not blocked by a job
// that calls this method; otherwise, a deadlock can occur.
+ (void)start;

// Registers Cronet as HttpProtocol Handler. Once registered, Cronet intercepts
// and handles all requests made through NSURLConnection and shared
// NSURLSession.
// This method must be called after |start|.
+ (void)registerHttpProtocolHandler;

// Unregister Cronet as HttpProtocol Handler. This means that Cronet will stop
// intercepting requests, however, it won't tear down the Cronet environment.
// This method must be called after |start|.
+ (void)unregisterHttpProtocolHandler;

// Installs Cronet into NSURLSessionConfiguration so that all
// NSURLSessions created with this configuration will use the Cronet stack.
// Note that all Cronet settings are global and are shared between
// all NSURLSessions & NSURLConnections that use the Cronet stack.
// This method must be called after |start|.
+ (void)installIntoSessionConfiguration:(NSURLSessionConfiguration*)config;

// Starts net-internals logging to a file named |fileName| in the application
// temporary directory. |fileName| must not be empty. Log level is determined
// by |logBytes| - if YES then LOG_ALL otherwise LOG_ALL_BUT_BYTES. If the file
// exists it is truncated before starting. If actively logging the call is
// ignored.
+ (void)startNetLogToFile:(NSString*)fileName logBytes:(BOOL)logBytes;

// Stop net-internals logging and flush file to disk. If a logging session is
// not in progress this call is ignored.
+ (void)stopNetLog;

// Returns the full user-agent that the stack uses.
// This is the exact string servers will see.
+ (NSString*)getUserAgent;

// Get a pointer to global instance of cronet_engine for GRPC C API.
+ (cronet_engine*)getGlobalEngine;

// Returns differences in metrics collected by Cronet since the last call to
// getGlobalMetricsDeltas, serialized as a [protobuf]
// (https://developers.google.com/protocol-buffers).
//
// Cronet starts collecting these metrics after the first call to
// getGlobalMetricsDeltras, so the first call returns no
// useful data as no metrics have yet been collected.
+ (NSData*)getGlobalMetricsDeltas;

// Sets Host Resolver Rules for testing.
// This method only has any effect before |start| is called.
+ (void)setHostResolverRulesForTesting:(NSString*)hostResolverRulesForTesting;

// Enables TestCertVerifier which accepts all certificates for testing.
// This method only has any effect before |start| is called.
+ (void)enableTestCertVerifierForTesting;

@end
