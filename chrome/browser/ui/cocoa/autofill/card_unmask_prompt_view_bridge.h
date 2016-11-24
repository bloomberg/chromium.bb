// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_BRIDGE_H_

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"
#include "components/autofill/core/browser/ui/card_unmask_prompt_view.h"

namespace content {
class WebContents;
}

@class CardUnmaskPromptViewCocoa;

namespace autofill {

class CardUnmaskPromptController;

class CardUnmaskPromptViewBridge : public CardUnmaskPromptView,
                                   public ConstrainedWindowMacDelegate {
 public:
  explicit CardUnmaskPromptViewBridge(CardUnmaskPromptController* controller,
                                      content::WebContents* web_contents);
  ~CardUnmaskPromptViewBridge() override;

  // CardUnmaskPromptView implementation:
  void Show() override;
  void ControllerGone() override;
  void DisableAndWaitForVerification() override;
  void GotVerificationResult(const base::string16& error_message,
                             bool allow_retry) override;

  // ConstrainedWindowMacDelegate implementation:
  void OnConstrainedWindowClosed(ConstrainedWindowMac* window) override;

  CardUnmaskPromptController* GetController();
  content::WebContents* GetWebContents();

  void PerformClose();

 private:
  std::unique_ptr<ConstrainedWindowMac> constrained_window_;
  base::scoped_nsobject<CardUnmaskPromptViewCocoa> view_controller_;

  // The controller |this| queries for logic and state.
  CardUnmaskPromptController* controller_;

  content::WebContents* web_contents_;
  base::WeakPtrFactory<CardUnmaskPromptViewBridge> weak_ptr_factory_;
};

}  // namespace autofill

@interface CardUnmaskPromptViewCocoa
    : NSViewController<NSWindowDelegate, NSTextFieldDelegate>

// Designated initializer. |bridge| must not be NULL.
- (id)initWithBridge:(autofill::CardUnmaskPromptViewBridge*)bridge;

- (void)setProgressOverlayText:(const base::string16&)text
                   showSpinner:(BOOL)showSpinner;
- (void)setRetriableErrorMessage:(const base::string16&)text;
- (void)setPermanentErrorMessage:(const base::string16&)text;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_BRIDGE_H_
