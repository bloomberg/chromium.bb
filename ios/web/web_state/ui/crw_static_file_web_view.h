// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_STATIC_FILE_WEB_VIEW_H_
#define IOS_WEB_WEB_STATE_UI_CRW_STATIC_FILE_WEB_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/web/net/crw_request_tracker_delegate.h"

namespace web {
class BrowserState;
}

// A subclass of UIWebView that specializes in presenting static html file
// content. Use this type of UIWebView that needs to display static HTML
// content from application bundle.
// TODO(shreyasv): Remove this class, since nobody is using it.
@interface CRWStaticFileWebView : UIWebView<CRWRequestTrackerDelegate>

// Creates a new UIWebView associated with the given |browserState|.
// |browserState| may be nullptr.
- (instancetype)initWithFrame:(CGRect)frame
                 browserState:(web::BrowserState*)browserState;

// Returns whether the request should be allowed for rendering into a
// special UIWebView that allows static file content.
+ (BOOL)isStaticFileRequest:(NSURLRequest*)request;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_STATIC_FILE_WEB_VIEW_H_
