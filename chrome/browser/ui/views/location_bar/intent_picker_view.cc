// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/intent_picker_view.h"

#include "chrome/browser/chromeos/arc/intent_helper/intent_picker_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "components/toolbar/vector_icons.h"

IntentPickerView::IntentPickerView(Browser* browser)
    : BubbleIconView(nullptr, 0), browser_(browser) {
  if (browser_) {
    intent_picker_controller_ =
        std::make_unique<arc::IntentPickerController>(browser_);
  }
}

IntentPickerView::~IntentPickerView() = default;

void IntentPickerView::OnExecuting(
    BubbleIconView::ExecuteSource execute_source) {
  if (browser_ && !browser_->profile()->IsGuestSession() &&
      !IsIncognitoMode()) {
    SetVisible(true);
    const GURL url = chrome::GetURLToBookmark(
        browser_->tab_strip_model()->GetActiveWebContents());

    arc::ArcNavigationThrottle::AsyncShowIntentPickerBubble(browser_, url);
  } else {
    SetVisible(false);
  }
}

views::BubbleDialogDelegateView* IntentPickerView::GetBubble() const {
  return IntentPickerBubbleView::intent_picker_bubble();
}

bool IntentPickerView::IsIncognitoMode() {
  DCHECK(browser_);

  return browser_->profile()->IsOffTheRecord();
}

const gfx::VectorIcon& IntentPickerView::GetVectorIcon() const {
  return toolbar::kOpenInNewIcon;
}
