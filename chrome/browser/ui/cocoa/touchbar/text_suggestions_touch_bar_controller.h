// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TOUCHBAR_TEXT_SUGGESTIONS_TOUCH_BAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_TOUCHBAR_TEXT_SUGGESTIONS_TOUCH_BAR_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include <memory>

#import "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/touchbar/web_textfield_touch_bar_controller.h"
#import "ui/base/cocoa/touch_bar_forward_declarations.h"

@class WebTextfieldTouchBarController;

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Range;
}  // namespace gfx

@interface TextSuggestionsTouchBarController
    : NSObject<NSTouchBarDelegate, NSCandidateListTouchBarItemDelegate>

- (instancetype)initWithWebContents:(content::WebContents*)webContents
                         controller:(WebTextfieldTouchBarController*)controller;

- (NSTouchBar*)makeTouchBar API_AVAILABLE(macos(10.12.2));

- (NSTouchBarItem*)touchBar:(NSTouchBar*)touchBar
      makeItemForIdentifier:(NSTouchBarItemIdentifier)identifier
    API_AVAILABLE(macos(10.12.2));

// Creates a NSCandidateListTouchBarItem that contains text suggestions
// based on the current text selection.
- (NSCandidateListTouchBarItem*)createCandidateListItem
    API_AVAILABLE(macos(10.12.2));

- (void)candidateListTouchBarItem:(NSCandidateListTouchBarItem*)anItem
     endSelectingCandidateAtIndex:(NSInteger)index
    API_AVAILABLE(macos(10.12.2));

- (void)updateTextSelection:(const base::string16&)text
                      range:(const gfx::Range&)range
                     offset:(size_t)offset;

// Returns a range from start to the end of the word that the cursor is
// currently in.
- (NSRange)editingWordRangeFromText:(const base::string16&)text
                     cursorPosition:(size_t)cursor;

- (void)requestSuggestions API_AVAILABLE(macos(10.12.2));

// Select the range of the editing word and replace it with a suggestion
// from the touch bar.
- (void)replaceEditingWordWithSuggestion:(NSString*)text;

@end

@interface TextSuggestionsTouchBarController (ExposedForTesting)

- (void)setWebContents:(content::WebContents*)webContents;
- (content::WebContents*)webContents;
- (void)setText:(NSString*)text;
- (NSString*)text;
- (void)setSelectionRange:(const gfx::Range&)range;
- (gfx::Range)selectionRange;
- (void)setSuggestions:(NSArray*)suggestions;
- (NSArray*)suggestions;
- (WebTextfieldTouchBarController*)controller;
- (void)setShouldIgnoreReplacementSelection:(BOOL)shouldIgnore;
- (void)setEditingWordRange:(const gfx::Range&)range offset:(size_t)offset;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TOUCHBAR_SUGGESTED_TEXT_TOUCH_BAR_CONTROLLER_H_