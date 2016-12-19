// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/app/startup/client_registration.h"

#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/web/public/web_client.h"

@implementation ClientRegistration

+ (void)registerClients {
  web::SetWebClient(new ChromeWebClient());
}

@end
