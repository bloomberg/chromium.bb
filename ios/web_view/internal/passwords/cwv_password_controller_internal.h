// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_PASSWORD_CONTROLLER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_PASSWORD_CONTROLLER_INTERNAL_H_

#import "ios/web/public/web_state/web_state.h"
#import "ios/web_view/public/cwv_password_controller.h"

NS_ASSUME_NONNULL_BEGIN

@interface CWVPasswordController ()

- (instancetype)initWithWebState:(web::WebState*)webState;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_PASSWORD_CONTROLLER_INTERNAL_H_
