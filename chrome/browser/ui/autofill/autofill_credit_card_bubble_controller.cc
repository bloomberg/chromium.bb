// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_credit_card_bubble_controller.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_credit_card_bubble.h"
#include "chrome/browser/ui/autofill/tab_autofill_manager_delegate.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/webkit_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/range/range.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(autofill::AutofillCreditCardBubbleController);

namespace autofill {

namespace {

static const int kMaxGeneratedCardTimesToShow = 5;
static const char kWalletGeneratedCardLearnMoreLink[] =
    "http://support.google.com/wallet/bin/answer.py?hl=en&answer=2740044";

AutofillCreditCardBubbleController* GetOrCreate(content::WebContents* wc) {
  AutofillCreditCardBubbleController::CreateForWebContents(wc);
  return AutofillCreditCardBubbleController::FromWebContents(wc);
}

}  // namespace

AutofillCreditCardBubbleController::AutofillCreditCardBubbleController(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      web_contents_(web_contents),
      should_show_anchor_(true),
      weak_ptr_factory_(this) {}

AutofillCreditCardBubbleController::~AutofillCreditCardBubbleController() {
  Hide();
}

// static
void AutofillCreditCardBubbleController::RegisterUserPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      ::prefs::kAutofillGeneratedCardBubbleTimesShown,
      0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

// static
void AutofillCreditCardBubbleController::ShowGeneratedCardUI(
    content::WebContents* contents,
    const base::string16& fronting_card_name,
    const base::string16& backing_card_name) {
  GetOrCreate(contents)->ShowAsGeneratedCardBubble(fronting_card_name,
                                                   backing_card_name);
}

// static
void AutofillCreditCardBubbleController::ShowNewCardSavedBubble(
    content::WebContents* contents,
    const base::string16& new_card_name) {
  GetOrCreate(contents)->ShowAsNewCardSavedBubble(new_card_name);
}

void AutofillCreditCardBubbleController::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (details.entry &&
      !content::PageTransitionIsRedirect(details.entry->GetTransitionType())) {
    should_show_anchor_ = false;
    UpdateAnchor();
    web_contents()->RemoveUserData(UserDataKey());
    // |this| is now deleted.
  }
}

bool AutofillCreditCardBubbleController::IsHiding() const {
  return bubble_ && bubble_->IsHiding();
}

gfx::Image AutofillCreditCardBubbleController::AnchorIcon() const {
  if (!should_show_anchor_)
    return gfx::Image();

  return ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IsGeneratedCardBubble() ? IDR_WALLET_ICON : IDR_AUTOFILL_CC_GENERIC);
}

base::string16 AutofillCreditCardBubbleController::BubbleTitle() const {
  return !IsGeneratedCardBubble() ? ASCIIToUTF16("Lorem ipsum, savum cardum") :
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CREDIT_CARD_BUBBLE_GENERATED_TITLE);
}

base::string16 AutofillCreditCardBubbleController::BubbleText() const {
  DCHECK(IsSetup());
  return bubble_text_;
}

const std::vector<ui::Range>& AutofillCreditCardBubbleController::
    BubbleTextRanges() const {
  DCHECK(IsSetup());
  return bubble_text_ranges_;
}

base::string16 AutofillCreditCardBubbleController::LinkText() const {
  return l10n_util::GetStringUTF16(IsGeneratedCardBubble() ?
      IDS_LEARN_MORE : IDS_AUTOFILL_CREDIT_CARD_BUBBLE_MANAGE_CARDS);
}

void AutofillCreditCardBubbleController::OnAnchorClicked() {
  Show(true);
}

void AutofillCreditCardBubbleController::OnLinkClicked() {
  if (IsGeneratedCardBubble()) {
#if !defined(OS_ANDROID)
    chrome::NavigateParams params(
        chrome::FindBrowserWithWebContents(web_contents()),
        GURL(kWalletGeneratedCardLearnMoreLink),
        content::PAGE_TRANSITION_AUTO_BOOKMARK);
    params.disposition = NEW_FOREGROUND_TAB;
    chrome::Navigate(&params);
#else
  // TODO(dbeam): implement.
#endif
  } else {
    TabAutofillManagerDelegate::FromWebContents(web_contents())->
        ShowAutofillSettings();
  }
  Hide();
}

base::WeakPtr<AutofillCreditCardBubbleController>
    AutofillCreditCardBubbleController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<AutofillCreditCardBubble> AutofillCreditCardBubbleController::
    CreateBubble() {
  return AutofillCreditCardBubble::Create(GetWeakPtr());
}

base::WeakPtr<AutofillCreditCardBubble> AutofillCreditCardBubbleController::
    bubble() {
  return bubble_;
}

bool AutofillCreditCardBubbleController::CanShow() const {
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  return web_contents() == browser->tab_strip_model()->GetActiveWebContents();
#else
  return true;
#endif
}

bool AutofillCreditCardBubbleController::ShouldDisplayBubbleInitially() const {
  Profile* profile = Profile::FromBrowserContext(
      web_contents_->GetBrowserContext());
  int times_shown = profile->GetPrefs()->GetInteger(
      ::prefs::kAutofillGeneratedCardBubbleTimesShown);
  return times_shown < kMaxGeneratedCardTimesToShow;
}

void AutofillCreditCardBubbleController::ShowAsGeneratedCardBubble(
    const base::string16& fronting_card_name,
    const base::string16& backing_card_name) {
  Reset();

  DCHECK(!fronting_card_name.empty());
  DCHECK(!backing_card_name.empty());
  fronting_card_name_ = fronting_card_name;
  backing_card_name_ = backing_card_name;

  SetUp();

  if (ShouldDisplayBubbleInitially())
    Show(false);
}

void AutofillCreditCardBubbleController::ShowAsNewCardSavedBubble(
    const base::string16& new_card_name) {
  Reset();

  DCHECK(!new_card_name.empty());
  new_card_name_ = new_card_name;

  SetUp();
  Show(false);
}

void AutofillCreditCardBubbleController::Reset() {
  Hide();

  fronting_card_name_.clear();
  backing_card_name_.clear();
  new_card_name_.clear();
  bubble_text_.clear();
  bubble_text_ranges_.clear();

  DCHECK(!IsSetup());
}

void AutofillCreditCardBubbleController::SetUp() {
  base::string16 full_text;
  if (IsGeneratedCardBubble()) {
    full_text = l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_CREDIT_CARD_BUBBLE_GENERATED_TEXT,
        fronting_card_name_,
        backing_card_name_);
  } else {
    full_text = ReplaceStringPlaceholders(
        ASCIIToUTF16("Lorem ipsum, savum cardem |$1|. Replacem before launch."),
        new_card_name_,
        NULL);
  }

  base::char16 separator('|');
  std::vector<base::string16> pieces;
  base::SplitStringDontTrim(full_text, separator, &pieces);

  while (!pieces.empty()) {
    base::string16 piece = pieces.front();
    if (!piece.empty() && pieces.size() % 2 == 0) {
      const size_t start = bubble_text_.size();
      bubble_text_ranges_.push_back(ui::Range(start, start + piece.size()));
    }
    bubble_text_.append(piece);
    pieces.erase(pieces.begin(), pieces.begin() + 1);
  }

  UpdateAnchor();
  DCHECK(IsSetup());
}

bool AutofillCreditCardBubbleController::IsSetup() const {
  DCHECK_EQ(bubble_text_.empty(), bubble_text_ranges_.empty());
  return !bubble_text_.empty();
}

bool AutofillCreditCardBubbleController::IsGeneratedCardBubble() const {
  DCHECK_EQ(fronting_card_name_.empty(), backing_card_name_.empty());
  DCHECK_NE(backing_card_name_.empty(), new_card_name_.empty());
  return !fronting_card_name_.empty();
}

void AutofillCreditCardBubbleController::Show(bool was_anchor_click) {
  if (!CanShow())
    return;

  bubble_ = CreateBubble();
  if (!bubble_) {
    // TODO(dbeam): Make a bubble on all applicable platforms.
    return;
  }

  bubble_->Show();

  if (IsGeneratedCardBubble() && !was_anchor_click) {
    // If the bubble was an automatically created "you generated a card" bubble,
    // count it as a show. If the user clicked the omnibox icon, don't count it.
    PrefService* prefs = Profile::FromBrowserContext(
        web_contents()->GetBrowserContext())->GetPrefs();
    prefs->SetInteger(::prefs::kAutofillGeneratedCardBubbleTimesShown,
        prefs->GetInteger(::prefs::kAutofillGeneratedCardBubbleTimesShown) + 1);
  }
}

void AutofillCreditCardBubbleController::UpdateAnchor() {
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (browser && browser->window() && browser->window()->GetLocationBar())
    browser->window()->GetLocationBar()->UpdateAutofillCreditCardView();
#else
  // TODO(dbeam): implement.
#endif
}

void AutofillCreditCardBubbleController::Hide() {
  // Sever |bubble_|'s reference to the controller and hide (if it exists).
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (bubble_ && !bubble_->IsHiding())
    bubble_->Hide();

  DCHECK(!bubble_ || bubble_->IsHiding());
}

}  // namespace autofill
