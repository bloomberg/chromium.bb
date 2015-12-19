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
  // Sets up the controller for local save and shows the bubble.
  // |save_card_callback| will be invoked if and when the Save button is
  // pressed.
  void ShowBubbleForLocalSave(const base::Closure& save_card_callback);

  // Sets up the controller for upload and shows the bubble.
  // |save_card_callback| will be invoked if and when the Save button is
  // pressed. The contents of |legal_message| will be displayed in the bubble.
  //
  // Example of valid |legal_message| data:
  // {
  //   "line" : [ {
  //     "template" : "The legal documents are: {0} and {1}",
  //     "template_parameter" : [ {
  //       "display_text" : "Terms of Service",
  //       "url": "http://www.example.com/tos"
  //     }, {
  //       "display_text" : "Privacy Policy",
  //       "url": "http://www.example.com/pp"
  //     } ],
  //   }, {
  //     "template" : "This is the second line and it has no parameters"
  //   } ]
  // }
  //
  // Caveats:
  // 1. '{' and '}' may be displayed by escaping them with an apostrophe in the
  //    template string, e.g. "template" : "Here is a literal '{'"
  // 2. Two or more consecutive dollar signs in the template string will not
  //    expand correctly.
  // 3. "${" anywhere in the template string is invalid.
  // 4. "\n" embedded anywhere in the template string, or an empty template
  //    string, can be used to separate paragraphs. It is not possible to create
  //    a completely blank line by using two consecutive newlines (they will be
  //    treated as a single newline by views::StyledLabel).
  void ShowBubbleForUpload(const base::Closure& save_card_callback,
                           scoped_ptr<base::DictionaryValue> legal_message);

  void ReshowBubble();

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
  void OnLegalMessageLinkClicked(const GURL& url) override;
  void OnBubbleClosed() override;

  const LegalMessageLines& GetLegalMessageLines() const override;

 protected:
  explicit SaveCardBubbleControllerImpl(content::WebContents* web_contents);
  ~SaveCardBubbleControllerImpl() override;

  // Returns the time elapsed since |timer_| was initialized.
  // Exists for testing.
  virtual base::TimeDelta Elapsed() const;

  // content::WebContentsObserver:
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;

 private:
  friend class content::WebContentsUserData<SaveCardBubbleControllerImpl>;

  void ShowBubble();

  // Update the visibility and toggled state of the Omnibox save card icon.
  void UpdateIcon();

  void OpenUrl(const GURL& url);

  // Weak reference. Will be nullptr if no bubble is currently shown.
  SaveCardBubbleView* save_card_bubble_view_;

  // Callback to run if user presses Save button in the bubble.
  // If save_card_callback_.is_null() is true then no bubble is available to
  // show and the icon is not visible.
  base::Closure save_card_callback_;

  // Governs whether the upload or local save version of the UI should be shown.
  bool is_uploading_;

  // Whether ReshowBubble() has been called since ShowBubbleFor*() was called.
  bool is_reshow_;

  // If no legal message should be shown then this variable is an empty vector.
  LegalMessageLines legal_message_lines_;

  // Used to measure the amount of time on a page; if it's less than some
  // reasonable limit, then don't close the bubble upon navigation.
  scoped_ptr<base::ElapsedTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(SaveCardBubbleControllerImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_SAVE_CARD_BUBBLE_CONTROLLER_IMPL_H_
