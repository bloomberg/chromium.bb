// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_WEB_VIEW_PASSWORD_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_WEB_VIEW_PASSWORD_H_

#import "cwv_web_view.h"

@class CWVPasswordController;

@interface CWVWebView (Password)

// The web view's password controller.
@property(nonatomic, readonly) CWVPasswordController* passwordController;

@end

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_WEB_VIEW_PASSWORD_H_
