// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_card_bubble_controller_impl.h"

#include "chrome/browser/ui/autofill/save_card_bubble_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "content/public/browser/navigation_details.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(autofill::SaveCardBubbleControllerImpl);

namespace {

// Number of seconds the bubble and icon will survive navigations, starting
// from when the bubble is shown.
// TODO(bondd): Share with ManagePasswordsUIController.
const int kSurviveNavigationSeconds = 5;

}  // namespace

namespace autofill {

SaveCardBubbleControllerImpl::SaveCardBubbleControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      save_card_bubble_view_(nullptr) {
  DCHECK(web_contents);
}

SaveCardBubbleControllerImpl::~SaveCardBubbleControllerImpl() {
  if (save_card_bubble_view_)
    save_card_bubble_view_->Hide();
}

void SaveCardBubbleControllerImpl::SetCallback(
    const base::Closure& save_card_callback) {
  save_card_callback_ = save_card_callback;
}

void SaveCardBubbleControllerImpl::ShowBubble(bool user_action) {
  DCHECK(!save_card_callback_.is_null());

  // Need to create location bar icon before bubble, otherwise bubble will be
  // unanchored.
  UpdateIcon();

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  save_card_bubble_view_ = browser->window()->ShowSaveCreditCardBubble(
      web_contents(), this, user_action);
  DCHECK(save_card_bubble_view_);

  // Update icon after creating |save_card_bubble_view_| so that icon will show
  // its "toggled on" state.
  UpdateIcon();

  timer_.reset(new base::ElapsedTimer());
}

bool SaveCardBubbleControllerImpl::IsIconVisible() const {
  return !save_card_callback_.is_null();
}

SaveCardBubbleView* SaveCardBubbleControllerImpl::save_card_bubble_view()
    const {
  return save_card_bubble_view_;
}

void SaveCardBubbleControllerImpl::OnSaveButton() {
  save_card_callback_.Run();
  save_card_callback_.Reset();
}

void SaveCardBubbleControllerImpl::OnCancelButton() {
  save_card_callback_.Reset();
}

void SaveCardBubbleControllerImpl::OnLearnMoreClicked() {
  web_contents()->OpenURL(content::OpenURLParams(
      GURL(kHelpURL), content::Referrer(), NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false));
}

void SaveCardBubbleControllerImpl::OnBubbleClosed() {
  save_card_bubble_view_ = nullptr;
  UpdateIcon();
}

void SaveCardBubbleControllerImpl::UpdateIcon() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  LocationBar* location_bar = browser->window()->GetLocationBar();
  location_bar->UpdateSaveCreditCardIcon();
}

void SaveCardBubbleControllerImpl::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // Nothing to do if there's no bubble available.
  if (save_card_callback_.is_null())
    return;

  // Don't react to in-page (fragment) navigations.
  if (details.is_in_page)
    return;

  // Don't do anything if a navigation occurs before a user could reasonably
  // interact with the bubble.
  if (timer_->Elapsed() <
      base::TimeDelta::FromSeconds(kSurviveNavigationSeconds))
    return;

  // Otherwise, get rid of the bubble and icon.
  save_card_callback_.Reset();
  if (save_card_bubble_view_) {
    save_card_bubble_view_->Hide();
    OnBubbleClosed();
  } else {
    UpdateIcon();
  }
}

}  // namespace autofill
