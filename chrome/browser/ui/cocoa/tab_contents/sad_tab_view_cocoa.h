// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_VIEW_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_VIEW_COCOA_H_

#import <Cocoa/Cocoa.h>

#import "ui/base/cocoa/controls/hyperlink_text_view.h"

@class SadTabView;

@protocol SadTabViewDelegate
- (void)sadTabViewButtonClicked:(SadTabView*)sadTabView;
- (void)sadTabView:(SadTabView*)sadTabView
    helpLinkClickedWithURL:(NSString*)url;
@end

// A view that displays the "sad tab" (aka crash page).
@interface SadTabView : NSView

@property(nonatomic, assign) id<SadTabViewDelegate> delegate;

- (void)setTitle:(int)title;
- (void)setMessage:(int)message;
- (void)setButtonTitle:(int)buttonTitle;
- (void)setHelpLinkTitle:(int)helpLinkTitle URL:(NSString*)url;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_VIEW_COCOA_H_
