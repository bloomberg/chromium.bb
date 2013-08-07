// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CREDIT_CARD_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CREDIT_CARD_BUBBLE_CONTROLLER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class Profile;

namespace content {
class WebContents;
}

namespace gfx {
class Image;
}

namespace ui {
class Range;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace autofill {

class AutofillCreditCardBubble;

////////////////////////////////////////////////////////////////////////////////
//
// AutofillCreditCardBubbleController
//
//  A class to show a bubble after successfully submitting the Autofill dialog.
//  The bubble is shown when a new credit card is saved locally or to explain
//  generated Online Wallet cards to the user. This class does not own the shown
//  bubble, but only has a weak reference to it (it is often owned by the native
//  platform's ui toolkit).
//
////////////////////////////////////////////////////////////////////////////////
class AutofillCreditCardBubbleController
    : public content::WebContentsObserver,
      public content::WebContentsUserData<AutofillCreditCardBubbleController> {
 public:
  virtual ~AutofillCreditCardBubbleController();

  // Registers preferences this class cares about.
  static void RegisterUserPrefs(user_prefs::PrefRegistrySyncable* registry);

  // Shows a clickable icon in the omnibox that informs the user about generated
  // (fronting) cards and how they are used to bill their original (backing)
  // card. Additionally, if |ShouldDisplayBubbleInitially()| is true, the bubble
  // will be shown initially (doesn't require being clicked).
  static void ShowGeneratedCardUI(content::WebContents* contents,
                                  const base::string16& backing_card_name,
                                  const base::string16& fronting_card_name);

  // Show a bubble and clickable omnibox icon notifying the user that new credit
  // card data has been saved. This bubble always shows initially.
  static void ShowNewCardSavedBubble(content::WebContents* contents,
                                     const base::string16& new_card_name);

  // content::WebContentsObserver implementation.
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

  // Returns whether |bubble_| is currently in the process of hiding.
  bool IsHiding() const;

  // The image that should be shown as an icon in the omnibox.
  gfx::Image AnchorIcon() const;

  // The title of the bubble. May be empty.
  base::string16 BubbleTitle() const;

  // The main text of the bubble.
  base::string16 BubbleText() const;

  // Ranges of text styles in the bubble's main content.
  const std::vector<ui::Range>& BubbleTextRanges() const;

  // The text of the link shown at the bubble of the bubble.
  base::string16 LinkText() const;

  // Called when the anchor for this bubble is clicked. Shows a new bubble.
  void OnAnchorClicked();

  // Called when the link at the bottom of the bubble is clicked. Opens and
  // navigates a new tab to an informational page and hides the bubble.
  void OnLinkClicked();

  // The web contents that successfully submitted the Autofill dialog (causing
  // this bubble to show).
  content::WebContents* web_contents() { return web_contents_; }
  const content::WebContents* web_contents() const { return web_contents_; }

 protected:
  // Creates a bubble connected to |web_contents|. Its content will be based on
  // which |ShowAs*()| method is called.
  explicit AutofillCreditCardBubbleController(content::WebContents* contents);

  // Returns a base::WeakPtr that references |this|. Exposed for testing.
  base::WeakPtr<AutofillCreditCardBubbleController> GetWeakPtr();

  // Creates and returns an Autofill credit card bubble. Exposed for testing.
  virtual base::WeakPtr<AutofillCreditCardBubble> CreateBubble();

  // Returns a weak reference to |bubble_|. May be invalid/NULL.
  virtual base::WeakPtr<AutofillCreditCardBubble> bubble();

  // Returns whether the bubble can currently show itself.
  virtual bool CanShow() const;

  // Whether the generated card bubble should be shown initially when showing
  // the anchor icon. This does not affect whether the generated card's icon
  // will show in the omnibox.
  bool ShouldDisplayBubbleInitially() const;

  // Show a bubble to educate the user about generated (fronting) cards and how
  // they are used to bill their original (backing) card. Exposed for testing.
  virtual void ShowAsGeneratedCardBubble(
      const base::string16& backing_card_name,
      const base::string16& fronting_card_name);

  // Shows a bubble notifying the user that new credit card data has been saved.
  // Exposed for testing.
  virtual void ShowAsNewCardSavedBubble(const base::string16& new_card_name);

 private:
  friend class content::WebContentsUserData<AutofillCreditCardBubbleController>;

  // Nukes the state of this controller and hides |bubble_| (if it exists).
  void Reset();

  // Generates the correct bubble text and text highlighting ranges for the
  // types of bubble being shown. Must be called before showing the bubble.
  void SetUp();

  // Returns whether the controller has been setup as a certain type of bubble.
  bool IsSetUp() const;

  // Returns whether the bubble is an educational bubble about generated cards.
  bool IsGeneratedCardBubble() const;

  // Hides any existing bubbles and creates and shows a new one. Called in
  // response to successful Autofill dialog submission or anchor click.
  void Show(bool was_anchor_click);

  // Updates the omnibox icon that |bubble_| will be anchored to.
  void UpdateAnchor();

  // Hides |bubble_| (if it exists and isn't already hiding).
  void Hide();

  // The web contents associated with this bubble.
  content::WebContents* const web_contents_;

  // The newly saved credit card.
  base::string16 new_card_name_;

  // The generated credit card number and associated backing card.
  base::string16 fronting_card_name_;
  base::string16 backing_card_name_;

  // Bubble contents text and text ranges generated in |SetUp()|.
  base::string16 bubble_text_;
  std::vector<ui::Range> bubble_text_ranges_;

  // A bubble view that's created by calling either |Show*()| method; owned by
  // the native widget/hierarchy, not this class (though this class must outlive
  // |bubble_|). NULL in many cases.
  base::WeakPtr<AutofillCreditCardBubble> bubble_;

  // Whether the anchor should currently be showing.
  bool should_show_anchor_;

  // A weak pointer factory for |Create()|.
  base::WeakPtrFactory<AutofillCreditCardBubbleController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutofillCreditCardBubbleController);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CREDIT_CARD_BUBBLE_CONTROLLER_H_
