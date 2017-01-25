// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_WEB_VIEW_SHELL_SHELL_DELEGATE_H_
#define IOS_WEB_VIEW_SHELL_SHELL_DELEGATE_H_

#import <UIKit/UIKit.h>

#import "ios/web_view/public/criwv_delegate.h"

// Shell-specific implementation of CRIWVDelegate.
@interface ShellDelegate : NSObject<CRIWVDelegate>
@end

#endif  // IOS_WEB_VIEW_SHELL_SHELL_DELEGATE_H_
