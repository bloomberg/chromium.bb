// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_startup_parameters.h"

#include "base/logging.h"
#include "ios/chrome/browser/experimental_flags.h"
#import "ios/chrome/browser/xcallback_parameters.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation AppStartupParameters {
  GURL _externalURL;
  BOOL _launchVoiceSearch;
  BOOL _launchInIncognito;
  BOOL _launchQRScanner;
}

@synthesize launchVoiceSearch = _launchVoiceSearch;
@synthesize launchInIncognito = _launchInIncognito;
@synthesize xCallbackParameters = _xCallbackParameters;

- (const GURL&)externalURL {
  return _externalURL;
}


- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithExternalURL:(const GURL&)externalURL {
  return [self initWithExternalURL:externalURL xCallbackParameters:nil];
}

- (instancetype)initWithExternalURL:(const GURL&)externalURL
                xCallbackParameters:(XCallbackParameters*)xCallbackParameters {
  self = [super init];
  if (self) {
    _externalURL = externalURL;
    _xCallbackParameters = xCallbackParameters;
  }
  return self;
}

- (NSString*)description {
  return [NSString stringWithFormat:@"ExternalURL: %s \nXCallbackParams: %@",
                                    _externalURL.spec().c_str(),
                                    _xCallbackParameters];
}

#pragma mark Property implementation.

- (BOOL)launchQRScanner {
  return _launchQRScanner && experimental_flags::IsQRCodeReaderEnabled();
}

- (void)setLaunchQRScanner:(BOOL)launch {
  _launchQRScanner = launch;
}

@end
