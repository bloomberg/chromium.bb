// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_WEB_VIEW_CONFIGURATION_SYNC_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_WEB_VIEW_CONFIGURATION_SYNC_H_

#import "ios/web_view/public/cwv_web_view_configuration.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVAuthenticationController;

@interface CWVWebViewConfiguration (Sync)

// This web view configuration's authentication controller.
// Nil if CWVWebViewConfiguration is created with +incognitoConfiguration.
@property(nonatomic, readonly, nullable)
    CWVAuthenticationController* authenticationController;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_WEB_VIEW_CONFIGURATION_SYNC_H_
