// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_POPUP_CELL_H_
#define CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_POPUP_CELL_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "components/omnibox/browser/autocomplete_match.h"

class OmniboxPopupViewMac;

@interface OmniboxPopupCellData : NSObject<NSCopying> {
 @private
  // Left hand side of the separator (e.g. a hyphen).
  NSAttributedString* contents_;
  // Right hand side of the separator (e.g. a hyphen).
  NSAttributedString* description_;

  // NOTE: While |prefix_| is used only for postfix suggestions, it still needs
  // to be a member of the class. This allows the |NSAttributedString| instance
  // to stay alive between the call to |drawTitle| and the actual paint event
  // which accesses the |NSAttributedString| instance.
  NSAttributedString* prefix_;

  // Common icon that shows next to most rows in the list.
  NSImage* image_;
  // Uncommon icon that only shows on answer rows (e.g. weather).
  NSImage* answerImage_;

  // The offset at which the infinite suggestion contents should be displayed.
  CGFloat contentsOffset_;

  BOOL isContentsRTL_;

  // Is this suggestion an answer or calculator result.
  bool isAnswer_;

  AutocompleteMatch::Type matchType_;
}

@property(readonly, retain, nonatomic) NSAttributedString* contents;
@property(readonly, retain, nonatomic) NSAttributedString* description;
@property(readonly, retain, nonatomic) NSAttributedString* prefix;
@property(readonly, retain, nonatomic) NSImage* image;
@property(retain, nonatomic) NSImage* incognitoImage;
@property(readonly, retain, nonatomic) NSImage* answerImage;
@property(readonly, nonatomic) CGFloat contentsOffset;
@property(readonly, nonatomic) BOOL isContentsRTL;
@property(readonly, nonatomic) bool isAnswer;
@property(readonly, nonatomic) AutocompleteMatch::Type matchType;

- (instancetype)initWithMatch:(const AutocompleteMatch&)match
               contentsOffset:(CGFloat)contentsOffset
                        image:(NSImage*)image
                  answerImage:(NSImage*)answerImage
                 forDarkTheme:(BOOL)isDarkTheme;

// Returns the width of the match contents.
- (CGFloat)getMatchContentsWidth;

@end

// Overrides how cells are displayed.  Uses information from
// OmniboxPopupCellData to draw suggestion results.
@interface OmniboxPopupCell : NSCell

- (void)drawMatchWithFrame:(NSRect)cellFrame inView:(NSView*)controlView;

// Returns the offset of the start of the contents in the input text for the
// given match. It is costly to compute this offset, so it is computed once and
// shared by all OmniboxPopupCell instances through OmniboxPopupViewMac parent.
+ (CGFloat)computeContentsOffset:(const AutocompleteMatch&)match;

+ (NSAttributedString*)createSeparatorStringForDarkTheme:(BOOL)isDarkTheme;

@end

const CGFloat kContentLineHeight = 25.0;

#endif  // CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_POPUP_CELL_H_
