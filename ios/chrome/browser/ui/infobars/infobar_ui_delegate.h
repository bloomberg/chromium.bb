// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_UI_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_UI_DELEGATE_H_

#import <UIKit/UIKit.h>

class InfoBarControllerDelegate;

// Protocol to communicate with the Infobar container.
@protocol InfobarUIDelegate

// Removes the view from the View Hierarchy.
- (void)removeView;

// Detaches view from its delegate. After this function is called, no user
// interaction can be handled.
// TODO(crbug.com/1372916): This nils the View's delegate, once we
// start using ViewControllers it might not be needed.
- (void)detachView;

// The Infobar UIView.
@property(nonatomic, readonly) UIView* view;

// The InfobarControllerDelegate.
@property(nonatomic, assign) InfoBarControllerDelegate* delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_UI_DELEGATE_H_
