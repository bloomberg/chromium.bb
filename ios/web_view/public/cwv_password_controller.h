// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_PASSWORD_CONTROLLER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_PASSWORD_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

CWV_EXPORT
// Exposes features that allow saving entered passwords as well as autofilling
// password forms.
@interface CWVPasswordController : NSObject

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_PASSWORD_CONTROLLER_H_
