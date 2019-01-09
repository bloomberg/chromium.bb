// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/incognito_window_count_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

// static
void IncognitoWindowCountView::ShowBubble(views::Button* anchor_button,
                                          Browser* browser,
                                          int incognito_window_count) {
  // The IncognitoWindowCountView is self-owned, it deletes itself when the
  // widget is closed or the parent browser is destroyed.
  new IncognitoWindowCountView(anchor_button, browser, incognito_window_count);
}

IncognitoWindowCountView::IncognitoWindowCountView(views::Button* anchor_button,
                                                   Browser* browser,
                                                   int incognito_window_count)
    : BubbleDialogDelegateView(anchor_button, views::BubbleBorder::TOP_RIGHT),
      incognito_window_count_(incognito_window_count),
      browser_(browser),
      browser_list_observer_(this),
      weak_ptr_factory_(this) {
  browser_list_observer_.Add(BrowserList::GetInstance());

  // The lifetime of this bubble is tied to the lifetime of the browser.
  set_parent_window(
      platform_util::GetViewForWindow(browser_->window()->GetNativeWindow()));

  set_margins(gfx::Insets());
  views::BubbleDialogDelegateView::CreateBubble(this)->Show();
  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::INCOGNITO_WINDOW_COUNTER);
}

IncognitoWindowCountView::~IncognitoWindowCountView() {}

void IncognitoWindowCountView::OnBrowserRemoved(Browser* browser) {
  if (browser_ == browser)
    delete this;
}

int IncognitoWindowCountView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void IncognitoWindowCountView::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  auto incognito_icon = std::make_unique<views::ImageView>();
  // TODO(https://crbug.com/896235): Try replacing |kIncognitoCircleIcon| with
  // drawing a circle under |kIncognitoIcon| and removing the circled icon.
  incognito_icon->SetImage(
      gfx::CreateVectorIcon(kIncognitoCircleIcon, 40, gfx::kGoogleGrey100));

  // TODO(https://crbug.com/915120): This Button is never clickable. Replace
  // by an alternative list item.
  HoverButton* title_line = new HoverButton(
      nullptr, std::move(incognito_icon),
      l10n_util::GetStringUTF16(IDS_INCOGNITO_WINDOW_COUNTER_TITLE),
      l10n_util::GetPluralStringFUTF16(IDS_INCOGNITO_WINDOW_COUNTER_MESSAGE,
                                       incognito_window_count_));
  title_line->SetState(views::Button::ButtonState::STATE_DISABLED);

  AddChildView(title_line);
  AddChildView(new views::Separator());

  AddChildView(new HoverButton(
      this, gfx::CreateVectorIcon(kCloseAllIcon, 16, gfx::kChromeIconGrey),
      l10n_util::GetStringUTF16(IDS_INCOGNITO_WINDOW_COUNTER_CLOSE_BUTTON)));
}

void IncognitoWindowCountView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  BrowserList::CloseAllBrowsersWithIncognitoProfile(
      browser_->profile(), base::DoNothing(), base::DoNothing(),
      false /* skip_beforeunload */);
}
