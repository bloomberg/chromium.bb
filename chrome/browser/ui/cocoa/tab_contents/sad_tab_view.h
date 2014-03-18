// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_VIEW_H_

#include "base/mac/scoped_nsobject.h"
#include "ui/base/cocoa/base_view.h"

#import <Cocoa/Cocoa.h>

@class SadTabController;
@class HyperlinkTextView;

// A view that displays the "sad tab" (aka crash page).
@interface SadTabView : BaseView<NSTextViewDelegate> {
 @private
  IBOutlet NSImageView* image_;
  base::scoped_nsobject<NSTextField> title_;
  base::scoped_nsobject<NSTextField> message_;
  base::scoped_nsobject<HyperlinkTextView> help_;

  base::scoped_nsobject<NSColor> backgroundColor_;
  NSSize messageSize_;

  // A weak reference to the parent controller.
  IBOutlet SadTabController* controller_;
}

// Designated initializer is -initWithFrame: .

// Called by SadTabController to remove the help text and link.
- (void)removeHelpText;

// Sets |help_| based on |helpPlaceholder_|, sets |helpPlaceholder_| to nil.
- (void)initializeHelpText;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_VIEW_H_
