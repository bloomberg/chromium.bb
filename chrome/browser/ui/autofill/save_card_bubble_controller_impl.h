// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_SAVE_CARD_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_SAVE_CARD_BUBBLE_CONTROLLER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/timer/elapsed_timer.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/ui/save_card_bubble_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class PrefService;

namespace autofill {

// Implementation of per-tab class to control the save credit card bubble and
// Omnibox icon.
class SaveCardBubbleControllerImpl
    : public SaveCardBubbleController,
      public content::WebContentsObserver,
      public content::WebContentsUserData<SaveCardBubbleControllerImpl> {
 public:
  ~SaveCardBubbleControllerImpl() override;

  // Sets up the controller for local save and shows the bubble.
  // |save_card_callback| will be invoked if and when the Save button is
  // pressed.
  void ShowBubbleForLocalSave(const CreditCard& card,
                              const base::Closure& save_card_callback);

  // Sets up the controller for upload and shows the bubble.
  // |save_card_callback| will be invoked if and when the Save button is
  // pressed. The contents of |legal_message| will be displayed in the bubble.
  // A field requesting CVC will appear in the bubble if
  // |should_cvc_be_requested| is true.
  void ShowBubbleForUpload(const CreditCard& card,
                           std::unique_ptr<base::DictionaryValue> legal_message,
                           bool should_cvc_be_requested,
                           const base::Closure& save_card_callback);

  void HideBubble();
  void ReshowBubble();

  // Returns true if Omnibox save credit card icon should be visible.
  bool IsIconVisible() const;

  // Returns nullptr if no bubble is currently shown.
  SaveCardBubbleView* save_card_bubble_view() const;

  // SaveCardBubbleController:
  base::string16 GetWindowTitle() const override;
  base::string16 GetExplanatoryMessage() const override;
  const CreditCard GetCard() const override;
  int GetCvcImageResourceId() const override;
  bool ShouldRequestCvcFromUser() const override;
  base::string16 GetCvcEnteredByUser() const override;
  void OnSaveButton(const base::string16& cvc = base::string16()) override;
  void OnCancelButton() override;
  void OnLearnMoreClicked() override;
  void OnLegalMessageLinkClicked(const GURL& url) override;
  void OnBubbleClosed() override;

  const LegalMessageLines& GetLegalMessageLines() const override;
  void ContinueToRequestCvcStage() override;

  // Used to check if an entered CVC value is a valid CVC for the current card.
  // Valid CVCs are a certain length for their card type (4 for AMEX, 3
  // otherwise) and are comprised only of numbers.
  bool InputCvcIsValid(const base::string16& input_text) const override;

 protected:
  explicit SaveCardBubbleControllerImpl(content::WebContents* web_contents);

  // Returns the time elapsed since |timer_| was initialized.
  // Exists for testing.
  virtual base::TimeDelta Elapsed() const;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WasHidden() override;
  void WebContentsDestroyed() override;

 private:
  friend class content::WebContentsUserData<SaveCardBubbleControllerImpl>;

  void ShowBubble();

  // Update the visibility and toggled state of the Omnibox save card icon.
  void UpdateIcon();

  void OpenUrl(const GURL& url);

  // Weak reference. Will be nullptr if no bubble is currently shown.
  SaveCardBubbleView* save_card_bubble_view_;

  // Weak reference to read & write |kAutofillAcceptSaveCreditCardPromptState|.
  PrefService* pref_service_;

  // Callback to run if user presses Save button in the bubble.
  // If save_card_callback_.is_null() is true then no bubble is available to
  // show and the icon is not visible.
  base::Closure save_card_callback_;

  // Governs whether the upload or local save version of the UI should be shown.
  bool is_uploading_ = false;

  // Whether ReshowBubble() has been called since ShowBubbleFor*() was called.
  bool is_reshow_ = false;

  // Whether the upload save version of the UI should ask the user for CVC.
  bool should_cvc_be_requested_ = false;

  // The value of the CVC entered by the user (if it was requested).
  base::string16 cvc_entered_by_user_;

  // When true, the upload save version of the UI is currently in its second
  // stage, which asks the user for CVC in order to save.
  bool is_currently_requesting_cvc_ = false;

  // Contains the details of the card that will be saved if the user accepts.
  CreditCard card_;

  // If no legal message should be shown then this variable is an empty vector.
  LegalMessageLines legal_message_lines_;

  // Used to measure the amount of time on a page; if it's less than some
  // reasonable limit, then don't close the bubble upon navigation.
  std::unique_ptr<base::ElapsedTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(SaveCardBubbleControllerImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_SAVE_CARD_BUBBLE_CONTROLLER_IMPL_H_
