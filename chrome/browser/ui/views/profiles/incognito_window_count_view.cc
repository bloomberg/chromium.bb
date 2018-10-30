// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/incognito_window_count_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

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

  views::BubbleDialogDelegateView::CreateBubble(this)->Show();
  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::INCOGNITO_WINDOW_COUNTER);
}

IncognitoWindowCountView::~IncognitoWindowCountView() {}

void IncognitoWindowCountView::OnBrowserRemoved(Browser* browser) {
  if (browser_ == browser)
    delete this;
}

base::string16 IncognitoWindowCountView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_INCOGNITO_WINDOW_COUNTER_TITLE);
}

int IncognitoWindowCountView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

base::string16 IncognitoWindowCountView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  DCHECK_EQ(ui::DIALOG_BUTTON_OK, button);
  if (ui::DIALOG_BUTTON_OK) {
    return l10n_util::GetPluralStringFUTF16(
        IDS_INCOGNITO_WINDOW_COUNTER_CLOSE_BUTTON, incognito_window_count_);
  }
  return NULL;
}

bool IncognitoWindowCountView::ShouldShowCloseButton() const {
  return true;
}

bool IncognitoWindowCountView::ShouldShowWindowIcon() const {
  return true;
}

gfx::ImageSkia IncognitoWindowCountView::GetWindowIcon() {
  // TODO(http://crbug.com/896235): Update color and size based on theme and UX
  // feedback.
  return gfx::CreateVectorIcon(kIncognitoIcon, 24, gfx::kGoogleGrey050);
}

bool IncognitoWindowCountView::Close() {
  // By default, closing the dialog would call Accept(), which would result in
  // data loss, so we intentionally just do nothing and close the dialog.
  return true;
}

bool IncognitoWindowCountView::Accept() {
  BrowserList::CloseAllBrowsersWithIncognitoProfile(
      browser_->profile(), base::DoNothing(), base::DoNothing(),
      false /* skip_beforeunload */);
  return true;
}

void IncognitoWindowCountView::Init() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(new views::Label(l10n_util::GetPluralStringFUTF16(
      IDS_INCOGNITO_WINDOW_COUNTER_MESSAGE, incognito_window_count_)));
}
