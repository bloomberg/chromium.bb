// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NET_CRW_URL_VERIFYING_PROTOCOL_HANDLER_H_
#define IOS_WEB_NET_CRW_URL_VERIFYING_PROTOCOL_HANDLER_H_

#import <UIKit/UIKit.h>

#include "ios/web/public/web_state/url_verification_constants.h"

class GURL;

namespace web {

// The URL used for verification. Requests to this URL must not be blocked (by
// Content Security Policy for example).
extern const char kURLForVerification[];

}  // namespace web

// Protocol handler used to verify that a site is not trying to spoof its URL.
// This handler will handle a specific URL, and use [NSURLRequest
// mainDocumentURL] on this request to check that the url returned by
// window.location.href has the same origin as the URL that webkit knows about.
@interface CRWURLVerifyingProtocolHandler : NSURLProtocol

// Returns the URL of the given UIWebView. Moreover, this method will set the
// trustLevel enum to the appropriate level from a security point of view. This
// method will execute JavaScript on the UIWebView and must not be called from
// inside a javascript execution context.
+ (GURL)currentURLForWebView:(UIWebView*)webView
    trustLevel:(web::URLVerificationTrustLevel*)trustLevel;

// Class initialization function that should be called as early as possible to
// initialize and thus reduce overhead when this class is first called.
// Returns YES if initialization completed.
+ (BOOL)preInitialize;

@end

#endif  // IOS_WEB_NET_CRW_URL_VERIFYING_PROTOCOL_HANDLER_H_
