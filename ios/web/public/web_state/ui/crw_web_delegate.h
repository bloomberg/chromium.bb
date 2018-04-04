// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_WEB_DELEGATE_H_
#define IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_WEB_DELEGATE_H_

#include <stdint.h>
#import <UIKit/UIKit.h>
#include <vector>

#import "base/ios/block_types.h"
#include "ios/web/public/favicon_url.h"
#include "ios/web/public/ssl_status.h"
#import "ios/web/public/web_state/ui/crw_native_content.h"
#import "ios/web/public/web_state/web_state.h"
#include "ui/base/page_transition_types.h"

class GURL;
@class CRWWebController;

// Methods implemented by the delegate of the CRWWebController.
// DEPRECATED, do not conform to this protocol and do not add any methods to it.
// Use web::WebStateDelegate instead.
// TODO(crbug.com/674991): Remove this protocol.
@protocol CRWWebDelegate<NSObject>

// Called when an external app needs to be opened, it also passes |linkClicked|
// to track if this call was a result of user action or not. Returns YES iff
// |URL| is launched in an external app.
// |sourceURL| is the original URL that triggered the navigation to |URL|.
- (BOOL)openExternalURL:(const GURL&)URL
              sourceURL:(const GURL&)sourceURL
            linkClicked:(BOOL)linkClicked;

@optional

// Called to ask CRWWebDelegate if |CRWWebController| should open the given URL.
// CRWWebDelegate can intercept the request by returning NO and processing URL
// in its own way.
- (BOOL)webController:(CRWWebController*)webController
        shouldOpenURL:(const GURL&)url
      mainDocumentURL:(const GURL&)mainDocumentURL;

// Called to ask if external URL should be opened. External URL is one that
// cannot be presented by CRWWebController.
- (BOOL)webController:(CRWWebController*)webController
    shouldOpenExternalURL:(const GURL&)URL;

// Called to retrieve the height of any header that is overlaying on top of the
// native content. This can be used to implement, for e.g. a toolbar that
// changes height dynamically. Returning a non-zero height affects the visible
// frame shown by the CRWWebController. 0.0 is assumed if not implemented.
// TODO(crbug.com/674991) These should be removed when native content is
// removed.
- (CGFloat)nativeContentHeaderHeightForWebController:
    (CRWWebController*)webController;

// Called to retrieve the height of any footer that is overlaying the bottom of
// the native content. This can be used to implement, for e.g. a toolbar that
// changes height dynamically. Returning a non-zero height affects the visible
// frame shown by the CRWWebController. 0.0 is assumed if not implemented.
// TODO(crbug.com/674991) These should be removed when native content is
// removed.
- (CGFloat)nativeContentFooterHeightForWebController:
    (CRWWebController*)webController;

@end

#endif  // IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_WEB_DELEGATE_H_
