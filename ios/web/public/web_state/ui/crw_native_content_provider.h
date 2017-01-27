// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_NATIVE_CONTENT_PROVIDER_H_
#define IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_NATIVE_CONTENT_PROVIDER_H_
#import <UIKit/UIKit.h>

class GURL;

@protocol CRWNativeContent;

namespace web {
class WebState;
}

// Provide a controller to a native view representing a given URL.
@protocol CRWNativeContentProvider

// Returns whether the Provider has a controller for the given URL.
- (BOOL)hasControllerForURL:(const GURL&)url;

// Returns an autoreleased controller for driving a native view contained
// within the web content area. This may return nil if the url is unsupported.
// |url| will be of the form "chrome://foo".
// |webState| is the webState that triggered the navigation to |url|.
- (id<CRWNativeContent>)controllerForURL:(const GURL&)url
                                webState:(web::WebState*)webState;

// Returns an autoreleased controller for driving a native view contained
// within the web content area. The native view will contain an error page
// with information appropriate for the problem described in |error|.
// |isPost| indicates whether the error was for a post request.
- (id<CRWNativeContent>)controllerForURL:(const GURL&)url
                               withError:(NSError*)error
                                  isPost:(BOOL)isPost;

@end

#endif  // IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_NATIVE_CONTENT_PROVIDER_H_
