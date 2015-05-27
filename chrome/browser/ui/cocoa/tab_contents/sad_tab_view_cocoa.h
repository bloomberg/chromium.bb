// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_VIEW_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_VIEW_COCOA_H_

#include "base/mac/scoped_nsobject.h"

#import <Cocoa/Cocoa.h>

@class HyperlinkTextView;

// A view that displays the "sad tab" (aka crash page).
@interface SadTabView : NSView<NSTextViewDelegate> {
 @private
  base::scoped_nsobject<NSImageView> image_;
  base::scoped_nsobject<NSTextField> title_;
  base::scoped_nsobject<NSTextField> message_;
  base::scoped_nsobject<HyperlinkTextView> help_;

  NSSize messageSize_;
}

// Designated initializer is -initWithFrame: .

// Called by SadTabController to remove the help text and link.
- (void)removeHelpText;

// Sets |help_| based on |helpPlaceholder_|, sets |helpPlaceholder_| to nil.
- (void)initializeHelpText;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_VIEW_COCOA_H_
