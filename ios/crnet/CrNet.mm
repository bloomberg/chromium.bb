// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/crnet/CrNet.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/net/crn_http_protocol_handler.h"
#import "ios/crnet/crnet_environment.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

static CrNetEnvironment* g_chrome_net = NULL;

static BOOL g_http2_enabled = YES;
static BOOL g_quic_enabled = NO;
static BOOL g_sdch_enabled = NO;
static BOOL g_user_agent_partial = NO;
static NSString* g_user_agent = nil;
static NSString* g_sdch_pref_store_filename = nil;
static RequestFilterBlock g_request_filter_block = nil;

@implementation CrNet

+ (void)setHttp2Enabled:(BOOL)http2Enabled {
  g_http2_enabled = http2Enabled;
}

+ (void)setQuicEnabled:(BOOL)quicEnabled {
  g_quic_enabled = quicEnabled;
}

+ (void)setSDCHEnabled:(BOOL)sdchEnabled
         withPrefStore:(NSString*)filename {
  g_sdch_enabled = sdchEnabled;
  g_sdch_pref_store_filename = filename;
}

+ (void)setPartialUserAgent:(NSString *)userAgent {
  [self setUserAgent:userAgent partial:YES];
}

+ (void)setUserAgent:(NSString*)userAgent partial:(bool)partial {
  g_user_agent = userAgent;
  g_user_agent_partial = partial;
}

+ (void)installInternal {
  CrNetEnvironment::Initialize();
  std::string user_agent = base::SysNSStringToUTF8(g_user_agent);
  g_chrome_net = new CrNetEnvironment(user_agent, g_user_agent_partial == YES);

  g_chrome_net->set_spdy_enabled(g_http2_enabled);
  g_chrome_net->set_quic_enabled(g_quic_enabled);
  g_chrome_net->set_sdch_enabled(g_sdch_enabled);
  if (g_sdch_pref_store_filename) {
    std::string filename = base::SysNSStringToUTF8(g_sdch_pref_store_filename);
    g_chrome_net->set_sdch_pref_store_filename(filename);
  }

  g_chrome_net->Install();
  g_chrome_net->SetHTTPProtocolHandlerRegistered(true);
  g_chrome_net->SetRequestFilterBlock(g_request_filter_block);
}

+ (void)install {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    if (![NSThread isMainThread]) {
      dispatch_sync(dispatch_get_main_queue(), ^(void) {
        [self installInternal];
      });
    } else {
      [self installInternal];
    }
  });
}

+ (void)installIntoSessionConfiguration:(NSURLSessionConfiguration*)config {
  g_chrome_net->InstallIntoSessionConfiguration(config);
}

+ (void)installWithPartialUserAgent:(NSString *)partialUserAgent {
  [self setPartialUserAgent:partialUserAgent];
  [self install];
}

+ (void)installWithPartialUserAgent:(NSString *)partialUserAgent
           enableDataReductionProxy:(BOOL)enableDataReductionProxy {
  LOG(ERROR) << "enableDataReductionProxy is no longer respected. The "
      << "functionality has been removed from CrNet.";

  [self setPartialUserAgent:partialUserAgent];
  [self install];
}

+ (void)installWithPartialUserAgent:(NSString *)partialUserAgent
             withRequestFilterBlock:(RequestFilterBlock)requestFilterBlock {
  [self setPartialUserAgent:partialUserAgent];
  [self setRequestFilterBlock:requestFilterBlock];
  [self install];
}

+ (void)setRequestFilterBlock:(RequestFilterBlock)block {
  if (g_chrome_net)
    g_chrome_net->SetRequestFilterBlock(block);
  else
    g_request_filter_block = block;
}

+ (void)uninstall {
  if (g_chrome_net) {
    g_chrome_net->SetHTTPProtocolHandlerRegistered(false);
  }
}

+ (void)startNetLogToFile:(NSString *)fileName logBytes:(BOOL)logBytes {
  if (g_chrome_net && [fileName length]) {
    g_chrome_net->StartNetLog([fileName UTF8String], logBytes);
  }
}

+ (void)stopNetLog {
  if (g_chrome_net) {
    return g_chrome_net->StopNetLog();
  }
}

+ (NSString *)userAgent {
  if (!g_chrome_net) {
    return nil;
  }

  std::string user_agent = g_chrome_net->user_agent();
  return [NSString stringWithCString:user_agent.c_str()
                            encoding:[NSString defaultCStringEncoding]];
}

+ (void)closeAllSpdySessions {
  if (g_chrome_net) {
    return g_chrome_net->CloseAllSpdySessions();
  }
}

+ (void)clearCacheWithCompletionCallback:(ClearCacheCallback)clearCacheCallback {
  if (g_chrome_net) {
    g_chrome_net->ClearCache(clearCacheCallback);
  }
}

@end
