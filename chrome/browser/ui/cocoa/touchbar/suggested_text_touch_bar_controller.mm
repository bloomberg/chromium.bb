// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/touchbar/suggested_text_touch_bar_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#import "ui/base/cocoa/touch_bar_util.h"

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

  void DidChangeTextSelection(base::string16 new_selected_text) override {
    if (@available(macOS 10.12.2, *)) {
      [owner_ webContentsTextSelectionChanged:base::SysUTF16ToNSString(
                                                  new_selected_text)];
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

  // A list of NSTextCheckingResults containing text suggestions for |text_|.
  base::scoped_nsobject<NSArray> suggestions_;
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
  if (!webContents_->IsFocusedElementEditable() || ![text_ length])
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
                  forSelectedRange:NSMakeRange(0, [text_ length])
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

- (void)webContentsTextSelectionChanged:(NSString*)text {
  if (webContents_->IsFocusedElementEditable()) {
    [self setText:text];
    [self requestSuggestionsForText:text];
  } else {
    [controller_ invalidateTouchBar];
  }
}

- (void)requestSuggestionsForText:(NSString*)text {
  NSSpellChecker* spell_checker = [NSSpellChecker sharedSpellChecker];
  [spell_checker
      requestCandidatesForSelectedRange:NSMakeRange(0, [text length])
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
  // Strip the string of leading and trailing whitespace for replacement.
  text = [text
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];
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

@end
