// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_SAVE_CARD_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_SAVE_CARD_BUBBLE_CONTROLLER_IMPL_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/autofill/save_card_bubble_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

// Implementation of per-tab class to control the save credit card bubble and
// Omnibox icon.
class SaveCardBubbleControllerImpl
    : public SaveCardBubbleController,
      public content::WebContentsObserver,
      public content::WebContentsUserData<SaveCardBubbleControllerImpl> {
 public:
  // Sets up the controller for local save. |save_card_callback| will be invoked
  // if and when the Save button is pressed.
  void InitializeForLocalSave(const base::Closure& save_card_callback);

  // Sets up the controller for upload. |save_card_callback| will be invoked if
  // and when the Save button is pressed. The contents of |legal_message| will
  // be displayed in the bubble.
  void InitializeForUpload(const base::Closure& save_card_callback,
                           scoped_ptr<base::DictionaryValue> legal_message);

  // InitializeForLocalSave() or InitializeForUpload() must be called first.
  void ShowBubble(bool user_action);

  // Returns true if Omnibox save credit card icon should be visible.
  bool IsIconVisible() const;

  // Returns nullptr if no bubble is currently shown.
  SaveCardBubbleView* save_card_bubble_view() const;

  // SaveCardBubbleController:
  base::string16 GetWindowTitle() const override;
  base::string16 GetExplanatoryMessage() const override;
  void OnSaveButton() override;
  void OnCancelButton() override;
  void OnLearnMoreClicked() override;
  void OnBubbleClosed() override;

 private:
  friend class content::WebContentsUserData<SaveCardBubbleControllerImpl>;

  explicit SaveCardBubbleControllerImpl(content::WebContents* web_contents);
  ~SaveCardBubbleControllerImpl() override;

  // Update the visibility and toggled state of the Omnibox save card icon.
  void UpdateIcon();

  // content::WebContentsObserver:
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;

  // Weak reference. Will be nullptr if no bubble is currently shown.
  SaveCardBubbleView* save_card_bubble_view_;

  // Callback to run if user presses Save button in the bubble.
  // If save_card_callback_.is_null() is true then no bubble is available to
  // show and the icon is not visible.
  base::Closure save_card_callback_;

  // Governs whether the upload or local save version of the UI should be shown.
  bool is_uploading_;

  // Used to measure the amount of time on a page; if it's less than some
  // reasonable limit, then don't close the bubble upon navigation.
  scoped_ptr<base::ElapsedTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(SaveCardBubbleControllerImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_SAVE_CARD_BUBBLE_CONTROLLER_IMPL_H_
