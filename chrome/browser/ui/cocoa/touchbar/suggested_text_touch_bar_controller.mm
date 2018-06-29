// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/touchbar/suggested_text_touch_bar_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#import "ui/base/cocoa/touch_bar_util.h"
#include "ui/gfx/range/range.h"

namespace {
// Touch bar identifier.
NSString* const kSuggestedTextTouchBarId = @"suggested-text";

// Touch bar item identifiers.
NSString* const kSuggestedTextItemsTouchId = @"SUGGESTED-TEXT-ITEMS";
}  // namespace

namespace text_observer {
class WebContentsTextObserver : public content::WebContentsObserver {
 public:
  WebContentsTextObserver(content::WebContents* web_contents,
                          SuggestedTextTouchBarController* owner)
      : WebContentsObserver(web_contents), owner_(owner) {}

  void DidChangeTextSelection(const base::string16& text,
                              const gfx::Range& range) override {
    if (@available(macOS 10.12.2, *)) {
      [owner_ webContentsTextSelectionChanged:base::SysUTF16ToNSString(text)
                                        range:range.ToNSRange()];
    }
  }

  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    if (@available(macOS 10.12.2, *)) {
      [owner_ webContentsFinishedLoading];
    }
  }

 private:
  SuggestedTextTouchBarController* owner_;  // weak
};
}  // namespace text_observer

@interface SuggestedTextTouchBarController () {
  // An observer for text selection changes.
  std::unique_ptr<text_observer::WebContentsTextObserver> observer_;

  // The WebContents in which text is being selected and replaced.
  content::WebContents* webContents_;  // weak

  // The WebTextfieldTouchBarController that invalidates the touch bar.
  WebTextfieldTouchBarController* controller_;  // weak

  // The text on which suggestions are based.
  base::scoped_nsobject<NSString> text_;

  // A list of NSTextCheckingResults containing text suggestions for |text_| at
  // |selectionRange_|.
  base::scoped_nsobject<NSArray> suggestions_;

  // The currently selected range. If the range has length = 0, it is simply a
  // cursor position.
  NSRange selectionRange_;
}
@end

@implementation SuggestedTextTouchBarController

- (instancetype)initWithWebContents:(content::WebContents*)webContents
                         controller:
                             (WebTextfieldTouchBarController*)controller {
  if ((self = [super init])) {
    webContents_ = webContents;
    controller_ = controller;
  }

  return self;
}

- (void)initObserver {
  observer_.reset(
      new text_observer::WebContentsTextObserver(webContents_, self));
}

- (NSTouchBar*)makeTouchBar {
  if (!webContents_->IsFocusedElementEditable())
    return nil;

  base::scoped_nsobject<NSTouchBar> touchBar([[ui::NSTouchBar() alloc] init]);
  [touchBar
      setCustomizationIdentifier:ui::GetTouchBarId(kSuggestedTextTouchBarId)];
  [touchBar setDelegate:self];

  [touchBar setDefaultItemIdentifiers:@[ kSuggestedTextItemsTouchId ]];
  return touchBar.autorelease();
}

- (NSTouchBarItem*)touchBar:(NSTouchBar*)touchBar
      makeItemForIdentifier:(NSTouchBarItemIdentifier)identifier {
  if (![identifier hasSuffix:kSuggestedTextItemsTouchId])
    return nil;

  return [self createCandidateListItem];
}

- (NSCandidateListTouchBarItem*)createCandidateListItem {
  NSCandidateListTouchBarItem* candidateListItem =
      [[NSCandidateListTouchBarItem alloc]
          initWithIdentifier:kSuggestedTextItemsTouchId];
  candidateListItem.delegate = self;
  [candidateListItem setCandidates:suggestions_
                  forSelectedRange:selectionRange_
                          inString:text_];
  return candidateListItem;
}

- (void)candidateListTouchBarItem:(NSCandidateListTouchBarItem*)anItem
     endSelectingCandidateAtIndex:(NSInteger)index {
  if (index == NSNotFound)
    return;

  NSTextCheckingResult* selectedResult = anItem.candidates[index];
  [self replaceSelectedText:[selectedResult replacementString]];
}

- (void)webContentsTextSelectionChanged:(NSString*)text range:(NSRange)range {
  if (webContents_->IsFocusedElementEditable()) {
    [self setText:text];
    selectionRange_ = range;
    [self requestSuggestionsForText:text inRange:range];
  } else {
    [controller_ invalidateTouchBar];
  }
}

- (void)webContentsFinishedLoading {
  if (webContents_->IsFocusedElementEditable()) {
    // TODO(tnijssen): Actually request the current text and cursor position.
    [self setText:@""];
    selectionRange_ = NSMakeRange(0, 0);
    [self requestSuggestionsForText:@"" inRange:NSMakeRange(0, 0)];
  }
}

- (void)requestSuggestionsForText:(NSString*)text inRange:(NSRange)range {
  NSSpellChecker* spell_checker = [NSSpellChecker sharedSpellChecker];
  [spell_checker
      requestCandidatesForSelectedRange:range
                               inString:text
                                  types:NSTextCheckingAllSystemTypes
                                options:nil
                 inSpellDocumentWithTag:0
                      completionHandler:^(
                          NSInteger sequenceNumber,
                          NSArray<NSTextCheckingResult*>* candidates) {
                        suggestions_.reset([candidates copy]);
                        [controller_ invalidateTouchBar];
                      }];
}

- (void)replaceSelectedText:(NSString*)text {
  // TODO(tnijssen): Create a method that replaces a range of text within
  // WebContents rather than relying on WebContents::Replace() which doesn't.
  webContents_->Replace(base::SysNSStringToUTF16(text));
}

- (void)setText:(NSString*)text {
  text_.reset([text copy]);
}

- (NSString*)text {
  return text_;
}

- (void)setSuggestions:(NSArray*)suggestions {
  suggestions_.reset([suggestions copy]);
}

- (NSArray*)suggestions {
  return suggestions_;
}

- (WebTextfieldTouchBarController*)controller {
  return controller_;
}

- (void)setWebContents:(content::WebContents*)webContents {
  webContents_ = webContents;
}

- (content::WebContents*)webContents {
  return webContents_;
}

- (void)setSelectionRange:(NSRange)range {
  selectionRange_ = range;
}

- (NSRange)selectionRange {
  return selectionRange_;
}

@end
