// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_GENERATED_CREDIT_CARD_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_GENERATED_CREDIT_CARD_BUBBLE_CONTROLLER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/range/range.h"

namespace content {
class WebContents;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace autofill {

class GeneratedCreditCardBubbleView;

// A simple struct of text highlighting range information. If |is_link| is true
// this portion of the text should be clickable (and trigger |OnLinkClicked()|).
// If |is_link| is false, the text denoted by |range| should be bolded.
struct TextRange {
  // The range of text this TextRange applies to (start and end).
  gfx::Range range;
  // Whether this text range should be styled like a link (e.g. clickable).
  bool is_link;
  // An equality operator for testing.
  bool operator==(const TextRange& other) const;
};

////////////////////////////////////////////////////////////////////////////////
//
// GeneratedCreditCardBubbleController
//
//  A class to control showing and hiding a bubble after a credit card is
//  generated.
//
////////////////////////////////////////////////////////////////////////////////
class GeneratedCreditCardBubbleController
    : public content::WebContentsObserver,
      public content::WebContentsUserData<GeneratedCreditCardBubbleController> {
 public:
  virtual ~GeneratedCreditCardBubbleController();

  // Registers preferences this class cares about.
  static void RegisterUserPrefs(user_prefs::PrefRegistrySyncable* registry);

  // Show a bubble to educate the user about generated (fronting) cards and how
  // they are used to bill their original (backing) card.
  static void Show(content::WebContents* contents,
                   const base::string16& fronting_card_name,
                   const base::string16& backing_card_name);

  // content::WebContentsObserver:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

  // Returns whether |bubble_| is currently in the process of hiding.
  bool IsHiding() const;

  // Returns the image that should be shown as an icon in the omnibox.
  gfx::Image AnchorIcon() const;

  // The title of the bubble.
  const base::string16& TitleText() const;

  // Text in the contents of the bubble.
  const base::string16& ContentsText() const;

  // Ranges of text styles in the bubble's main content.
  const std::vector<TextRange>& ContentsTextRanges() const;

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
  // Creates a bubble connected to |web_contents|.
  explicit GeneratedCreditCardBubbleController(content::WebContents* contents);

  // Returns a base::WeakPtr that references |this|. Exposed for testing.
  base::WeakPtr<GeneratedCreditCardBubbleController> GetWeakPtr();

  // Creates and returns an Autofill credit card bubble. Exposed for testing.
  virtual base::WeakPtr<GeneratedCreditCardBubbleView> CreateBubble();

  // Returns a weak reference to |bubble_|. May be invalid/NULL.
  virtual base::WeakPtr<GeneratedCreditCardBubbleView> bubble();

  // Returns whether the bubble can currently show itself.
  virtual bool CanShow() const;

  // Whether the generated card bubble should be shown initially when showing
  // the anchor icon. This does not affect whether the generated card's icon
  // will show in the omnibox.
  bool ShouldDisplayBubbleInitially() const;

  // Exposed for testing.
  base::string16 fronting_card_name() const { return fronting_card_name_; }
  base::string16 backing_card_name() const { return backing_card_name_; }

  // Generates the correct bubble text and text highlighting ranges and shows a
  // bubble to educate the user about generated (fronting) cards and how they
  // are used to bill their original (backing) card. Exposed for testing.
  virtual void SetupAndShow(const base::string16& fronting_card_name,
                            const base::string16& backing_card_name);

 private:
  friend class
      content::WebContentsUserData<GeneratedCreditCardBubbleController>;

  // An internal helper to show the bubble.
  void Show(bool was_anchor_click);

  // Updates the omnibox icon that |bubble_| is anchored to.
  void UpdateAnchor();

  // Hides |bubble_| (if it exists and isn't already hiding).
  void Hide();

  // The web contents associated with this bubble.
  content::WebContents* const web_contents_;

  // The generated credit card number and associated backing card.
  base::string16 fronting_card_name_;
  base::string16 backing_card_name_;

  // The title text of the bubble.
  const base::string16 title_text_;

  // Strings and ranges generated based on |backing_card_name_| and
  // |fronting_card_name_|.
  base::string16 contents_text_;
  std::vector<TextRange> contents_text_ranges_;

  // A bubble view that's created by calling either |Show*()| method; owned by
  // the native widget/hierarchy, not this class (though this class must outlive
  // |bubble_|). NULL in many cases.
  base::WeakPtr<GeneratedCreditCardBubbleView> bubble_;

  // Whether the anchor should currently be showing.
  bool should_show_anchor_;

  // A weak pointer factory for |Create()|.
  base::WeakPtrFactory<GeneratedCreditCardBubbleController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GeneratedCreditCardBubbleController);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_GENERATED_CREDIT_CARD_BUBBLE_CONTROLLER_H_
