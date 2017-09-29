// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/settings_reset_prompt_dialog.h"

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace chrome {

void ShowSettingsResetPrompt(
    Browser* browser,
    safe_browsing::SettingsResetPromptController* controller) {
  SettingsResetPromptDialog* dialog = new SettingsResetPromptDialog(controller);
  // The dialog will delete itself, as implemented in
  // |DialogDelegateView::DeleteDelegate()|, when its widget is closed.
  dialog->Show(browser);
}

}  // namespace chrome

namespace {
constexpr int kDialogWidth = 448;
}  // namespace

SettingsResetPromptDialog::SettingsResetPromptDialog(
    safe_browsing::SettingsResetPromptController* controller)
    : browser_(nullptr), controller_(controller) {
  DCHECK(controller_);

  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));
  SetLayoutManager(new views::FillLayout());

  views::StyledLabel* dialog_label =
      new views::StyledLabel(controller_->GetMainText(), /*listener=*/nullptr);
  views::StyledLabel::RangeStyleInfo url_style;
  url_style.text_style = STYLE_EMPHASIZED;
  dialog_label->AddStyleRange(controller_->GetMainTextUrlRange(), url_style);
  AddChildView(dialog_label);
}

SettingsResetPromptDialog::~SettingsResetPromptDialog() {
  // Make sure the controller is correctly notified in case the dialog widget is
  // closed by some other means than the dialog buttons.
  if (controller_)
    controller_->Close();
}

void SettingsResetPromptDialog::Show(Browser* browser) {
  DCHECK(browser);
  DCHECK(controller_);

  browser_ = browser;
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  constrained_window::CreateBrowserModalDialogViews(
      this, browser_view->GetNativeWindow())
      ->Show();
  controller_->DialogShown();
}

// WidgetDelegate overrides.

ui::ModalType SettingsResetPromptDialog::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

bool SettingsResetPromptDialog::ShouldShowWindowIcon() const {
  return false;
}

base::string16 SettingsResetPromptDialog::GetWindowTitle() const {
  DCHECK(controller_);
  return controller_->GetWindowTitle();
}

// DialogDelegate overrides.

base::string16 SettingsResetPromptDialog::GetDialogButtonLabel(
    ui::DialogButton button) const {
  DCHECK(button == ui::DIALOG_BUTTON_OK || button == ui::DIALOG_BUTTON_CANCEL);
  DCHECK(controller_);

  if (button == ui::DIALOG_BUTTON_OK)
    return controller_->GetButtonLabel();
  return DialogDelegate::GetDialogButtonLabel(button);
}

bool SettingsResetPromptDialog::Accept() {
  if (controller_) {
    controller_->Accept();
    controller_ = nullptr;
  }
  return true;
}

bool SettingsResetPromptDialog::Cancel() {
  if (controller_) {
    controller_->Cancel();
    controller_ = nullptr;
  }
  return true;
}

bool SettingsResetPromptDialog::Close() {
  if (controller_) {
    controller_->Close();
    controller_ = nullptr;
  }
  return true;
}

// View overrides.

gfx::Size SettingsResetPromptDialog::CalculatePreferredSize() const {
  return gfx::Size(kDialogWidth, GetHeightForWidth(kDialogWidth));
}
