// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_VIEW_H_
#pragma once

#include "base/mac/cocoa_protocols.h"
#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/base_view.h"

#import <Cocoa/Cocoa.h>

@class SadTabController;
@class HyperlinkTextView;

// A view that displays the "sad tab" (aka crash page).
@interface SadTabView : BaseView<NSTextViewDelegate> {
 @private
  IBOutlet NSImageView* image_;
  IBOutlet NSTextField* title_;
  IBOutlet NSTextField* message_;
  IBOutlet NSTextField* helpPlaceholder_;

  scoped_nsobject<NSColor> backgroundColor_;
  NSSize messageSize_;

  // Text fields don't work as well with embedded links as text views, but
  // text views cannot conveniently be created in IB. The xib file contains
  // a text field |helpPlaceholder_| that's replaced by this text view |help_|
  // in -awakeFromNib.
  scoped_nsobject<HyperlinkTextView> help_;

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
