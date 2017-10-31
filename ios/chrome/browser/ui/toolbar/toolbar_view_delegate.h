// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_VIEW_DELEGATE_H_

#import <Foundation/Foundation.h>

// A delegate protocol to pass UIView information about the toolbar.
// TODO(crbug.com/683793): Remove this protocol when toolbars are presented via
// UIViewControllers, as this can already be accomplished through that API.
@protocol ToolbarViewDelegate<NSObject>

// Called when the toolbar view performs a layout pass.
- (void)toolbarDidLayout;

// Called when the toolbar view is transferred to a new parent window.
- (void)windowDidChange;

// Called when the toolbar view's trait collection changes.
- (void)traitCollectionDidChange;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_VIEW_DELEGATE_H_
