// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/touchbar/suggested_text_touch_bar_controller.h"

#include "base/i18n/break_iterator.h"
#include "base/strings/sys_string_conversions.h"
#include "content/public/browser/render_widget_host_view.h"
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

  void UpdateWebContents(content::WebContents* web_contents) {
    Observe(web_contents);
  }

  void DidChangeTextSelection(const base::string16& text,
                              const gfx::Range& range) override {
    [owner_ updateTextSelection:text range:range];
  }

  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    [owner_ updateTextSelection:base::string16() range:gfx::Range()];
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

  // The range of the word that's currently being edited. Used when replacing
  // the current editing word with a selected suggestion. If length = 0, there
  // is no word currently being edited and the suggestion will be placed at the
  // cursor.
  NSRange editingWordRange_;
}
@end

@implementation SuggestedTextTouchBarController

- (BOOL)isTextfieldFocused {
  return webContents_ && webContents_->IsFocusedElementEditable();
}

- (instancetype)initWithWebContents:(content::WebContents*)webContents
                         controller:
                             (WebTextfieldTouchBarController*)controller {
  if ((self = [super init])) {
    webContents_ = webContents;
    controller_ = controller;
    observer_.reset(
        new text_observer::WebContentsTextObserver(webContents_, self));
  }

  return self;
}

- (NSTouchBar*)makeTouchBar {
  if (![self isTextfieldFocused])
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

  [candidateListItem setCandidates:suggestions_
                  forSelectedRange:selectionRange_
                          inString:text_];

  candidateListItem.delegate = self;
  if (selectionRange_.length)
    candidateListItem.collapsed = YES;

  return candidateListItem;
}

- (void)candidateListTouchBarItem:(NSCandidateListTouchBarItem*)anItem
     endSelectingCandidateAtIndex:(NSInteger)index {
  if (index == NSNotFound)
    return;

  if (anItem) {
    NSTextCheckingResult* selectedResult = anItem.candidates[index];
    [self replaceEditingWordWithSuggestion:[selectedResult replacementString]];
  }

  ui::LogTouchBarUMA(ui::TouchBarAction::TEXT_SUGGESTION);
}

- (void)updateTextSelection:(const base::string16&)text
                      range:(const gfx::Range&)range {
  if (@available(macOS 10.12.2, *)) {
    if (![self isTextfieldFocused]) {
      [controller_ invalidateTouchBar];
      return;
    }

    text_.reset([base::SysUTF16ToNSString(text) retain]);
    selectionRange_ = range.ToNSRange();
    editingWordRange_ =
        [self editingWordRangeFromText:text cursorPosition:range.start()];
    [self requestSuggestions];
  }
}

- (NSRange)editingWordRangeFromText:(const base::string16&)text
                     cursorPosition:(size_t)cursor {
  // The cursor should not be off the end of the text.
  DCHECK(cursor <= text.length());

  // Default range is just the cursor position. This is used if BreakIterator
  // cannot be initialized, there is no text in the textfield, or the cursor is
  // at the front of a word.
  size_t location = cursor;
  size_t length = 0;

  base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_WORD);

  if (iter.Init()) {
    // Repeat iter.Advance() until end of line is reached or
    // current iterator position passes cursor position.
    while (iter.pos() < cursor && iter.Advance()) {
    }

    // If BreakIterator stopped at the end of a word, the cursor is in/at the
    // end of a word so the editing word range is [word start, word end].
    if (iter.IsWord()) {
      location = iter.prev();
      length = iter.pos() - iter.prev();
    }

    // The suggestion text returned by AppKit has the necessary trailing
    // whitespace which should replace the existing trailing whitespace.
    // If the BreakIterator just iterated over whitespace or iterates over
    // whitespace when it advances, modify the length of the editing word
    // range to include the whitespace.
    if ((iter.pos() && !iter.IsWord()) || (iter.Advance() && !iter.IsWord()))
      length = iter.pos() - location;
  }

  return NSMakeRange(location, length);
}

- (void)requestSuggestions {
  NSSpellChecker* spell_checker = [NSSpellChecker sharedSpellChecker];
  [spell_checker
      requestCandidatesForSelectedRange:selectionRange_
                               inString:text_
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

- (void)replaceEditingWordWithSuggestion:(NSString*)text {
  // If the editing word is not selected in its entirety, modify the selection
  // to cover the current editing word.
  if (!NSEqualRanges(editingWordRange_, selectionRange_)) {
    int start_adjust = editingWordRange_.location - selectionRange_.location;
    int end_adjust = (editingWordRange_.location + editingWordRange_.length) -
                     (selectionRange_.location + selectionRange_.length);
    webContents_->AdjustSelectionByCharacterOffset(start_adjust, end_adjust,
                                                   false);
  }
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
  observer_->UpdateWebContents(webContents);

  if (![self isTextfieldFocused]) {
    [controller_ invalidateTouchBar];
    return;
  }

  const base::string16 text =
      webContents_->GetTopLevelRenderWidgetHostView()->GetSurroundingText();
  const gfx::Range range =
      webContents_->GetTopLevelRenderWidgetHostView()->GetSelectedRange();
  if (range.IsValid())
    [self updateTextSelection:text range:range];
  else
    [self updateTextSelection:base::string16() range:gfx::Range()];
}

- (content::WebContents*)webContents {
  return webContents_;
}

- (void)setSelectionRange:(const gfx::Range&)range {
  selectionRange_ = range.ToNSRange();
}

- (gfx::Range)selectionRange {
  return gfx::Range(selectionRange_);
}

@end
