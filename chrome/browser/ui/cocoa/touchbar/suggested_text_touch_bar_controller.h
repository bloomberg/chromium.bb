// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TOUCHBAR_SUGGESTED_TEXT_TOUCH_BAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_TOUCHBAR_SUGGESTED_TEXT_TOUCH_BAR_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include <memory>

#import "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/touchbar/web_textfield_touch_bar_controller.h"
#import "ui/base/cocoa/touch_bar_forward_declarations.h"

@class WebTextfieldTouchBarController;

namespace content {
class WebContents;
}  // namespace content

@interface SuggestedTextTouchBarController
    : NSObject<NSTouchBarDelegate, NSCandidateListTouchBarItemDelegate>

- (instancetype)initWithWebContents:(content::WebContents*)webContents
                         controller:(WebTextfieldTouchBarController*)controller;

- (void)initObserver;

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

- (void)webContentsTextSelectionChanged:(NSString*)text
    API_AVAILABLE(macos(10.12.2));

- (void)requestSuggestionsForText:(NSString*)text API_AVAILABLE(macos(10.12.2));

- (void)replaceSelectedText:(NSString*)text;

@end

@interface SuggestedTextTouchBarController (ExposedForTesting)

- (void)setText:(NSString*)text;
- (NSString*)text;
- (void)setSuggestions:(NSArray*)suggestions;
- (NSArray*)suggestions;
- (WebTextfieldTouchBarController*)controller;
- (void)setWebContents:(content::WebContents*)webContents;
- (content::WebContents*)webContents;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TOUCHBAR_SUGGESTED_TEXT_TOUCH_BAR_CONTROLLER_H_