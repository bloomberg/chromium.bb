// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_SAVE_CARD_BUBBLE_VIEW_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_SAVE_CARD_BUBBLE_VIEW_BRIDGE_H_

#include "base/gtest_prod_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/autofill/save_card_bubble_view.h"
#include "chrome/browser/ui/cocoa/base_bubble_controller.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/legal_message_line.h"

@class BrowserWindowController;
@class SaveCardBubbleViewCocoa;

namespace autofill {

class SaveCardBubbleController;

class SaveCardBubbleViewBridge : public SaveCardBubbleView {
 public:
  SaveCardBubbleViewBridge(SaveCardBubbleController* controller,
                           BrowserWindowController* browser_window_controller);

  // Returns the title that should be displayed in the bubble.
  base::string16 GetWindowTitle() const;

  // Returns the explanatory text that should be displayed in the bubble.
  // Returns an empty string if no message should be displayed.
  base::string16 GetExplanatoryMessage() const;

  // Returns the card that will be uploaded if the user accepts.
  CreditCard GetCard() const;

  // Returns the legal messages that should be displayed in the footer. Result
  // will be empty if no legal message should be shown.
  const LegalMessageLines GetLegalMessageLines() const;

  // Interaction.
  void OnSaveButton();
  void OnCancelButton();
  void OnLearnMoreClicked();
  void OnLegalMessageLinkClicked(const GURL& url);
  void OnBubbleClosed();

  // SaveCardBubbleView implementation:
  void Hide() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SaveCardBubbleViewTest, SaveShouldClose);
  FRIEND_TEST_ALL_PREFIXES(SaveCardBubbleViewTest, CancelShouldClose);
  FRIEND_TEST_ALL_PREFIXES(SaveCardBubbleViewTest, LearnMoreShouldNotClose);
  FRIEND_TEST_ALL_PREFIXES(SaveCardBubbleViewTest, LegalMessageShouldNotClose);
  FRIEND_TEST_ALL_PREFIXES(SaveCardBubbleViewTest, ReturnInvokesDefaultAction);
  FRIEND_TEST_ALL_PREFIXES(SaveCardBubbleViewTest, EscapeCloses);

  virtual ~SaveCardBubbleViewBridge();

  // Weak.
  SaveCardBubbleViewCocoa* view_controller_;

  // Weak. Queried for state and notified of user input.
  SaveCardBubbleController* controller_;

  DISALLOW_COPY_AND_ASSIGN(SaveCardBubbleViewBridge);
};

}  // namespace autofill

@interface SaveCardBubbleViewCocoa : BaseBubbleController<NSTextViewDelegate>

// Designated initializer. |bridge| must not be NULL.
- (id)initWithBrowserWindowController:
          (BrowserWindowController*)browserWindowController
                               bridge:
                                   (autofill::SaveCardBubbleViewBridge*)bridge;

- (void)onSaveButton:(id)sender;
- (void)onCancelButton:(id)sender;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_SAVE_CARD_BUBBLE_VIEW_BRIDGE_H_
