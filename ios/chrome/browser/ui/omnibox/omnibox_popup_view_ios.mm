// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/omnibox/omnibox_popup_view_ios.h"

#import <QuartzCore/QuartzCore.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/threading/sequenced_worker_pool.h"
#import "components/image_fetcher/ios/ios_image_data_fetcher_wrapper.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/experimental_flags.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_popup_mediator.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_popup_presenter.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_popup_view_controller.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_popup_view_suggestions_delegate.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ios/web/public/web_thread.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/gfx/geometry/rect.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

OmniboxPopupViewIOS::OmniboxPopupViewIOS(
    ios::ChromeBrowserState* browser_state,
    OmniboxEditModel* edit_model,
    OmniboxPopupViewSuggestionsDelegate* delegate,
    id<OmniboxPopupPositioner> positioner)
    : model_(new OmniboxPopupModel(this, edit_model)),
      delegate_(delegate),
      is_open_(false) {
  DCHECK(delegate);
  DCHECK(browser_state);
  DCHECK(edit_model);

  std::unique_ptr<image_fetcher::IOSImageDataFetcherWrapper> imageFetcher =
      base::MakeUnique<image_fetcher::IOSImageDataFetcherWrapper>(
          browser_state->GetRequestContext());

  mediator_.reset([[OmniboxPopupMediator alloc]
      initWithFetcher:std::move(imageFetcher)
             delegate:this]);
  popup_controller_.reset([[OmniboxPopupViewController alloc] init]);
  [popup_controller_ setIncognito:browser_state->IsOffTheRecord()];

  [mediator_ setIncognito:browser_state->IsOffTheRecord()];
  [mediator_ setConsumer:popup_controller_];
  [popup_controller_ setImageRetriever:mediator_];
  [popup_controller_ setDelegate:mediator_];

  presenter_.reset([[OmniboxPopupPresenter alloc]
      initWithPopupPositioner:positioner
          popupViewController:popup_controller_]);
}

OmniboxPopupViewIOS::~OmniboxPopupViewIOS() {
  // Destroy the model, in case it tries to call back into us when destroyed.
  model_.reset();
}

// Set left image to globe or magnifying glass depending on which autocomplete
// option comes first.
void OmniboxPopupViewIOS::UpdateEditViewIcon() {
  const AutocompleteResult& result = model_->result();
  const AutocompleteMatch& match = result.match_at(0);  // 0 for first result.
  int image_id = GetIconForAutocompleteMatchType(
      match.type, /* is_starred */ false, /* is_incognito */ false);
  delegate_->OnTopmostSuggestionImageChanged(image_id);
}

void OmniboxPopupViewIOS::UpdatePopupAppearance() {
  const AutocompleteResult& result = model_->result();

  // TODO(crbug.com/762597): this logic should move to PopupCoordinator.
  if (!is_open_ && !result.empty()) {
    // The popup is not currently open and there are results to display. Update
    // and animate the cells
    [mediator_ updateMatches:result withAnimation:YES];
  } else {
    // The popup is already displayed or there are no results to display. Update
    // the cells without animating.
    [mediator_ updateMatches:result withAnimation:NO];
  }
  is_open_ = !result.empty();

  if (is_open_) {
    [presenter_ updateHeightAndAnimateAppearanceIfNecessary];
    UpdateEditViewIcon();
  } else {
    [presenter_ animateCollapse];
  }

  delegate_->OnResultsChanged(result);
}

gfx::Rect OmniboxPopupViewIOS::GetTargetBounds() {
  return gfx::Rect();
}

bool OmniboxPopupViewIOS::IsOpen() const {
  return is_open_;
}

OmniboxPopupModel* OmniboxPopupViewIOS::model() const {
  return model_.get();
}

#pragma mark - OmniboxPopupProvider

bool OmniboxPopupViewIOS::IsPopupOpen() {
  return is_open_;
}

void OmniboxPopupViewIOS::SetTextAlignment(NSTextAlignment alignment) {
  [popup_controller_ setTextAlignment:alignment];
}

#pragma mark - OmniboxPopupViewControllerDelegate

bool OmniboxPopupViewIOS::IsStarredMatch(const AutocompleteMatch& match) const {
  return model_->IsStarredMatch(match);
}

void OmniboxPopupViewIOS::OnMatchSelected(
    const AutocompleteMatch& selectedMatch,
    size_t row) {
  WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB;
  base::RecordAction(UserMetricsAction("MobileOmniboxUse"));

  // OpenMatch() may close the popup, which will clear the result set and, by
  // extension, |match| and its contents.  So copy the relevant match out to
  // make sure it stays alive until the call completes.
  AutocompleteMatch match = selectedMatch;

  if (match.type == AutocompleteMatchType::CLIPBOARD) {
    base::RecordAction(UserMetricsAction("MobileOmniboxClipboardToURL"));
    UMA_HISTOGRAM_LONG_TIMES_100(
        "MobileOmnibox.PressedClipboardSuggestionAge",
        ClipboardRecentContent::GetInstance()->GetClipboardContentAge());
  }
  delegate_->OnSelectedMatchForOpening(match, disposition, GURL(),
                                       base::string16(), row);
}

void OmniboxPopupViewIOS::OnMatchSelectedForAppending(
    const AutocompleteMatch& match) {
  // Make a defensive copy of |match.contents|, as CopyToOmnibox() will trigger
  // a new round of autocomplete and modify |match|.
  base::string16 contents(match.contents);
  delegate_->OnSelectedMatchForAppending(contents);
}

void OmniboxPopupViewIOS::OnMatchSelectedForDeletion(
    const AutocompleteMatch& match) {
  model_->autocomplete_controller()->DeleteMatch(match);
}

void OmniboxPopupViewIOS::OnScroll() {
  delegate_->OnPopupDidScroll();
}
