// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/open_url_command.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#include "ios/web/public/referrer.h"
#include "url/gurl.h"

@implementation OpenUrlCommand {
  GURL _url;
  web::Referrer _referrer;
  base::scoped_nsobject<NSString> _windowName;
}

@synthesize inIncognito = _inIncognito;
@synthesize inBackground = _inBackground;
@synthesize fromChrome = _fromChrome;
@synthesize appendTo = _appendTo;

- (instancetype)initWithTag:(NSInteger)tag {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithURL:(const GURL&)url
                   referrer:(const web::Referrer&)referrer
                 windowName:(NSString*)windowName
                inIncognito:(BOOL)inIncognito
               inBackground:(BOOL)inBackground
                   appendTo:(OpenPosition)appendTo {
  if ((self = [super initWithTag:IDC_OPEN_URL])) {
    _url = url;
    _referrer = referrer;
    _windowName.reset([windowName copy]);
    _inIncognito = inIncognito;
    _inBackground = inBackground;
    _appendTo = appendTo;
  }
  return self;
}

- (instancetype)initWithURLFromChrome:(const GURL&)url {
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

@end
