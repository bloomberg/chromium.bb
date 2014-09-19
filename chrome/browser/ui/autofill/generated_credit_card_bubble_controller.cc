// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/generated_credit_card_bubble_controller.h"

#include <climits>

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/generated_credit_card_bubble_view.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    autofill::GeneratedCreditCardBubbleController);

namespace autofill {

namespace {

static const int kMaxGeneratedCardTimesToShow = INT_MAX;
static const base::char16 kRangeSeparator = '|';
static const char kWalletGeneratedCardLearnMoreLink[] =
    "http://support.google.com/wallet/bin/answer.py?hl=en&answer=2740044";

GeneratedCreditCardBubbleController* GetOrCreate(content::WebContents* wc) {
  GeneratedCreditCardBubbleController::CreateForWebContents(wc);
  return GeneratedCreditCardBubbleController::FromWebContents(wc);
}

}  // namespace

bool TextRange::operator==(const TextRange& other) const {
  return other.range == range && other.is_link == is_link;
}

GeneratedCreditCardBubbleController::GeneratedCreditCardBubbleController(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      web_contents_(web_contents),
      title_text_(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_GENERATED_CREDIT_CARD_BUBBLE_TITLE)),
      should_show_anchor_(true),
      weak_ptr_factory_(this) {}

GeneratedCreditCardBubbleController::~GeneratedCreditCardBubbleController() {
  // In the case that the tab is closed, the controller can be deleted while
  // bubble is showing. Always calling |Hide()| ensures that the bubble closes.
  Hide();
}

// static
void GeneratedCreditCardBubbleController::RegisterUserPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      ::prefs::kAutofillGeneratedCardBubbleTimesShown,
      0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

// static
void GeneratedCreditCardBubbleController::Show(
    content::WebContents* contents,
    const base::string16& fronting_card_name,
    const base::string16& backing_card_name) {
  GetOrCreate(contents)->SetupAndShow(fronting_card_name, backing_card_name);
}

void GeneratedCreditCardBubbleController::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (!details.entry)
    return;

  // Don't destory the bubble due to reloads, form submits, or redirects right
  // after the dialog succeeds. Merchants often navigate to a confirmation page.
  ui::PageTransition transition = details.entry->GetTransitionType();
  if (transition == ui::PAGE_TRANSITION_FORM_SUBMIT ||
      transition == ui::PAGE_TRANSITION_RELOAD ||
      ui::PageTransitionIsRedirect(transition)) {
    return;
  }

  should_show_anchor_ = false;
  UpdateAnchor();
  web_contents()->RemoveUserData(UserDataKey());
  // |this| is now deleted.
}

bool GeneratedCreditCardBubbleController::IsHiding() const {
  return bubble_ && bubble_->IsHiding();
}

gfx::Image GeneratedCreditCardBubbleController::AnchorIcon() const {
  if (!should_show_anchor_)
    return gfx::Image();
  return ui::ResourceBundle::GetSharedInstance().GetImageNamed(IDR_WALLET_ICON);
}

const base::string16& GeneratedCreditCardBubbleController::TitleText() const {
  return title_text_;
}

const base::string16& GeneratedCreditCardBubbleController::ContentsText()
    const {
  return contents_text_;
}

const std::vector<TextRange>& GeneratedCreditCardBubbleController::
    ContentsTextRanges() const {
  return contents_text_ranges_;
}

void GeneratedCreditCardBubbleController::OnAnchorClicked() {
  Show(true);
}

void GeneratedCreditCardBubbleController::OnLinkClicked() {
  // Open a new tab to the Online Wallet help link.
  chrome::NavigateParams params(
      chrome::FindBrowserWithWebContents(web_contents()),
      GURL(kWalletGeneratedCardLearnMoreLink),
      ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);

  Hide();
}

base::WeakPtr<GeneratedCreditCardBubbleController>
    GeneratedCreditCardBubbleController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<GeneratedCreditCardBubbleView>
    GeneratedCreditCardBubbleController::CreateBubble() {
  return GeneratedCreditCardBubbleView::Create(GetWeakPtr());
}

base::WeakPtr<GeneratedCreditCardBubbleView>
    GeneratedCreditCardBubbleController::bubble() {
  return bubble_;
}

bool GeneratedCreditCardBubbleController::CanShow() const {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  return web_contents() == browser->tab_strip_model()->GetActiveWebContents();
}

bool GeneratedCreditCardBubbleController::ShouldDisplayBubbleInitially() const {
  Profile* profile = Profile::FromBrowserContext(
      web_contents_->GetBrowserContext());
  int times_shown = profile->GetPrefs()->GetInteger(
      ::prefs::kAutofillGeneratedCardBubbleTimesShown);
  return times_shown < kMaxGeneratedCardTimesToShow;
}

void GeneratedCreditCardBubbleController::SetupAndShow(
    const base::string16& fronting_card_name,
    const base::string16& backing_card_name) {
  DCHECK(!fronting_card_name.empty());
  DCHECK(!backing_card_name.empty());

  fronting_card_name_ = fronting_card_name;
  backing_card_name_ = backing_card_name;

  // Clear any generated state or from the last |SetupAndShow()| call.
  contents_text_.clear();
  contents_text_ranges_.clear();

  base::string16 to_split = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_GENERATED_CREDIT_CARD_BUBBLE_CONTENTS,
      fronting_card_name_,
      backing_card_name_);

  // Split the full text on '|' to highlight certain parts. For example, "sly"
  // and "jumped" would be bolded in "The |sly| fox |jumped| over the lazy dog".
  std::vector<base::string16> pieces;
  base::SplitStringDontTrim(to_split, kRangeSeparator, &pieces);

  while (!pieces.empty()) {
    base::string16 piece = pieces.front();

    // Every second piece should be bolded. Because |base::SplitString*()|
    // leaves an empty "" even if '|' is the first character, this is guaranteed
    // to work for "|highlighting| starts here". Ignore empty pieces because
    // there's nothing to highlight.
    if (!piece.empty() && pieces.size() % 2 == 0) {
      const size_t start = contents_text_.size();
      TextRange bold_text;
      bold_text.range = gfx::Range(start, start + piece.size());
      bold_text.is_link = false;
      contents_text_ranges_.push_back(bold_text);
    }

    // Append the piece whether it's bolded or not and move on to the next one.
    contents_text_.append(piece);
    pieces.erase(pieces.begin(), pieces.begin() + 1);
  }

  // Add a "Learn more" link at the end of the header text if it's a generated
  // card bubble.
  base::string16 learn_more = l10n_util::GetStringUTF16(IDS_LEARN_MORE);
  contents_text_.append(base::ASCIIToUTF16(" ") + learn_more);
  const size_t header_size = contents_text_.size();
  TextRange end_link;
  end_link.range = gfx::Range(header_size - learn_more.size(), header_size);
  end_link.is_link = true;
  contents_text_ranges_.push_back(end_link);

  UpdateAnchor();

  if (ShouldDisplayBubbleInitially())
    Show(false);
}

void GeneratedCreditCardBubbleController::Show(bool was_anchor_click) {
  Hide();

  if (!CanShow())
    return;

  bubble_ = CreateBubble();
  if (!bubble_) {
    // TODO(dbeam): Make a bubble on all applicable platforms.
    return;
  }

  bubble_->Show();

  if (!was_anchor_click) {
    // If the bubble was an automatically created "you generated a card" bubble,
    // count it as a show. If the user clicked the omnibox icon, don't count it.
    PrefService* prefs = Profile::FromBrowserContext(
        web_contents()->GetBrowserContext())->GetPrefs();
    prefs->SetInteger(::prefs::kAutofillGeneratedCardBubbleTimesShown,
        prefs->GetInteger(::prefs::kAutofillGeneratedCardBubbleTimesShown) + 1);
  }
}

void GeneratedCreditCardBubbleController::UpdateAnchor() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (browser && browser->window() && browser->window()->GetLocationBar())
    browser->window()->GetLocationBar()->UpdateGeneratedCreditCardView();
}

void GeneratedCreditCardBubbleController::Hide() {
  if (bubble_ && !bubble_->IsHiding())
    bubble_->Hide();
}

}  // namespace autofill
