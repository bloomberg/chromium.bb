// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_startup_parameters.h"

#include "base/logging.h"
#import "ios/chrome/browser/xcallback_parameters.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation AppStartupParameters {
  GURL _externalURL;
}

@synthesize launchVoiceSearch = _launchVoiceSearch;
@synthesize launchInIncognito = _launchInIncognito;
@synthesize xCallbackParameters = _xCallbackParameters;
@synthesize launchFocusOmnibox = _launchFocusOmnibox;
@synthesize launchQRScanner = _launchQRScanner;

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
  NSMutableString* description = [NSMutableString
      stringWithFormat:@"ExternalURL: %s \nXCallbackParams: %@",
                       _externalURL.spec().c_str(), _xCallbackParameters];

  if (self.launchQRScanner) {
    [description appendString:@", should launch QR scanner"];
  }

  if (self.launchInIncognito) {
    [description appendString:@", should launch in incognito"];
  }

  if (self.launchFocusOmnibox) {
    [description appendString:@", should focus omnibox"];
  }

  if (self.launchVoiceSearch) {
    [description appendString:@", should launch voice search"];
  }

  return description;
}

@end
