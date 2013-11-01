// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_NEW_CREDIT_CARD_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_NEW_CREDIT_CARD_BUBBLE_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "ui/gfx/image/image.h"

class Profile;

namespace content {
class WebContents;
}

namespace autofill {

class NewCreditCardBubbleView;
class AutofillProfile;
class CreditCard;

// A simple wrapper that contains descriptive information about a credit card
// that should be shown in the content of the bubble.
struct CreditCardDescription {
 CreditCardDescription();
 ~CreditCardDescription();
 // The icon of the credit card issuer (i.e. Visa, Mastercard).
 gfx::Image icon;
 // The display name of the card. Shown next to the icon.
 base::string16 name;
 // A longer description of the card being shown in the bubble.
 base::string16 description;
};

////////////////////////////////////////////////////////////////////////////////
//
// NewCreditCardBubbleController
//
//  A class to control showing/hiding a bubble after saved a new card in Chrome.
//  Here's a visual reference to what this bubble looks like:
//
//  @----------------------------------------@
//  |  Bubble title text                     |
//  |                                        |
//  |  [ Card icon ] Card name               |
//  |  Card description that will probably   |
//  |  also span multiple lines.             |
//  |                                        |
//  |  Learn more link                       |
//  @----------------------------------------@
//
////////////////////////////////////////////////////////////////////////////////
class NewCreditCardBubbleController {
 public:
  virtual ~NewCreditCardBubbleController();

  // Show a bubble informing the user that new credit card data has been saved.
  // This bubble points to the settings menu. Ownership of |new_card|
  // and |billing_profile| are transferred by this call.
  static void Show(content::WebContents* web_contents,
                   scoped_ptr<CreditCard> new_card,
                   scoped_ptr<AutofillProfile> billing_profile);

  // The bubble's title text.
  const base::string16& TitleText() const;

  // A card description to show in the bubble.
  const CreditCardDescription& CardDescription() const;

  // The text of the link shown at the bubble of the bubble.
  const base::string16& LinkText() const;

  // Called when |bubble_| is destroyed.
  void OnBubbleDestroyed();

  // Called when the link at the bottom of the bubble is clicked.
  void OnLinkClicked();

  // Returns the profile this bubble is associated with.
  Profile* profile() { return profile_; }

  // Returns the WebContents this bubble is associated with.
  content::WebContents* web_contents() { return web_contents_; }

 protected:
  // Create a bubble attached to |profile|.
  explicit NewCreditCardBubbleController(content::WebContents* web_contents);

  // Creates and returns an Autofill credit card bubble. Exposed for testing.
  virtual base::WeakPtr<NewCreditCardBubbleView> CreateBubble();

  // Returns a weak reference to |bubble_|. May be invalid/NULL.
  virtual base::WeakPtr<NewCreditCardBubbleView> bubble();

  // Show a bubble notifying the user that new credit card data has been saved.
  // Exposed for testing.
  virtual void SetupAndShow(scoped_ptr<CreditCard> new_card,
                            scoped_ptr<AutofillProfile> billing_profile);

 private:
  friend class NewCreditCardBubbleCocoaUnitTest;

  // Hides |bubble_| if it exists.
  void Hide();

  // The profile this bubble is associated with.
  // TODO(dbeam): Break Views dependency on Profile and remove |profile_|.
  Profile* const profile_;

  // The web contents associated with this bubble.
  content::WebContents* const web_contents_;

  // The newly saved credit card and assocated billing information.
  scoped_ptr<CreditCard> new_card_;
  scoped_ptr<AutofillProfile> billing_profile_;

  // The title text of the bubble.
  const base::string16 title_text_;

  // The bubble's link text.
  const base::string16 link_text_;

  // Strings and descriptions that are generated based on |new_card_| and
  // |billing_profile_|.
  struct CreditCardDescription card_desc_;

  // A bubble view that's created by calling either |Show*()| method; owned by
  // the native widget/hierarchy, not this class (though this class must outlive
  // |bubble_|). NULL in many cases.
  base::WeakPtr<NewCreditCardBubbleView> bubble_;

  // A weak pointer factory for |Create()|.
  base::WeakPtrFactory<NewCreditCardBubbleController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NewCreditCardBubbleController);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_NEW_CREDIT_CARD_BUBBLE_CONTROLLER_H_
