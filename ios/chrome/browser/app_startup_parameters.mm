// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_startup_parameters.h"

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/xcallback_parameters.h"
#include "url/gurl.h"

@implementation AppStartupParameters {
  GURL _externalURL;
  base::scoped_nsobject<XCallbackParameters> _xCallbackParameters;
  BOOL _launchVoiceSearch;
  BOOL _launchInIncognito;
}

@synthesize launchVoiceSearch = _launchVoiceSearch;
@synthesize launchInIncognito = _launchInIncognito;

- (const GURL&)externalURL {
  return _externalURL;
}

- (XCallbackParameters*)xCallbackParameters {
  return _xCallbackParameters.get();
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
    _externalURL = GURL(externalURL);
    _xCallbackParameters.reset([xCallbackParameters retain]);
  }
  return self;
}

- (NSString*)description {
  return [NSString stringWithFormat:@"ExternalURL: %s \nXCallbackParams: %@",
                                    _externalURL.spec().c_str(),
                                    _xCallbackParameters.get()];
}

@end
