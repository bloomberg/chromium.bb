// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/open_url_command.h"

#include "base/mac/scoped_nsobject.h"
#include "ios/web/public/referrer.h"
#include "url/gurl.h"

@implementation OpenUrlCommand {
  GURL _url;
  web::Referrer _referrer;
  base::scoped_nsobject<NSString> _windowName;
  BOOL _inIncognito;
  BOOL _inBackground;
  BOOL _fromChrome;
  OpenPosition _appendTo;
}

@synthesize fromChrome = _fromChrome;

- (id)initWithURL:(const GURL&)url
         referrer:(const web::Referrer&)referrer
       windowName:(NSString*)windowName
      inIncognito:(BOOL)inIncognito
     inBackground:(BOOL)inBackground
         appendTo:(OpenPosition)appendTo {
  if ((self = [super init])) {
    _url = url;
    _referrer = referrer;
    _windowName.reset([windowName retain]);
    _inIncognito = inIncognito;
    _inBackground = inBackground;
    _appendTo = appendTo;
  }
  return self;
}

- (id)initWithURLFromChrome:(const GURL&)url {
  if ((self = [self initWithURL:url
                       referrer:web::Referrer()
                     windowName:nil
                    inIncognito:NO
                   inBackground:NO
                       appendTo:kLastTab])) {
    _fromChrome = YES;
  }
  return self;
}

- (const GURL&)url {
  return _url;
}

- (const web::Referrer&)referrer {
  return _referrer;
}

- (NSString*)windowName {
  return _windowName.get();
}

- (BOOL)inIncognito {
  return _inIncognito;
}

- (BOOL)inBackground {
  return _inBackground;
}

- (OpenPosition)appendTo {
  return _appendTo;
}

@end
