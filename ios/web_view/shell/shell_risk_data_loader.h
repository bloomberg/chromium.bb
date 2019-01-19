// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_SHELL_SHELL_RISK_DATA_LOADER_H_
#define IOS_WEB_VIEW_SHELL_SHELL_RISK_DATA_LOADER_H_

#import <ChromeWebView/ChromeWebView.h>

NS_ASSUME_NONNULL_BEGIN

// Risk data loader for ios_web_view_shell.
@interface ShellRiskDataLoader : NSObject <CWVCreditCardVerifierDataSource>

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_SHELL_SHELL_RISK_DATA_LOADER_H_
