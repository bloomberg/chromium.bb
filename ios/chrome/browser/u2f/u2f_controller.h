// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_U2F_U2F_CONTROLLER_H_
#define IOS_CHROME_BROWSER_U2F_U2F_CONTROLLER_H_

#import <Foundation/Foundation.h>

#include "url/gurl.h"

namespace web {
class WebState;
}  // namespace web

// This class provides methods needed to handle FIDO U2F calls.
// U2FController shouldn't be used outside of this directory. Instead all users
// should use U2FTabHelper.
@interface U2FController : NSObject

// Generates the x-callback GURL for making FIDO U2F requests. Returns empty
// GURL if origin is not secure.
- (GURL)XCallbackFromRequestURL:(const GURL&)requestURL
                      originURL:(const GURL&)originURL
                         tabURL:(const GURL&)tabURL
                          tabID:(NSString*)tabID;

// Compares original URL of Tab when making the U2F call and current Tab URL. If
// they match, uses WebState to evaluate JS string extracted from U2F URL.
- (void)evaluateU2FResultFromU2FURL:(const GURL&)U2FURL
                           webState:(web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_U2F_U2F_CONTROLLER_H_
