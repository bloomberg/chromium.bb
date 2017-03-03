// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/settings_reset_prompt_dialog.h"

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace safe_browsing {

// static
void SettingsResetPromptController::ShowSettingsResetPrompt(
    Browser* browser,
    SettingsResetPromptController* controller) {
  SettingsResetPromptDialog* dialog = new SettingsResetPromptDialog(controller);
  // The dialog will delete itself, as implemented in
  // |DialogDelegateView::DeleteDelegate()|, when its widget is closed.
  dialog->Show(browser);
}

}  // namespace safe_browsing

namespace {
constexpr int kDialogWidth = 448;
}  // namespace

SettingsResetPromptDialog::SettingsResetPromptDialog(
    safe_browsing::SettingsResetPromptController* controller)
    : browser_(nullptr), controller_(controller), interaction_done_(false) {
  DCHECK(controller_);

  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
                                        views::kButtonHEdgeMarginNew,
                                        views::kPanelVertMargin, 0));

  views::StyledLabel* dialog_label =
      new views::StyledLabel(controller_->GetMainText(), /*listener=*/nullptr);
  views::StyledLabel::RangeStyleInfo url_style;
  url_style.weight = gfx::Font::Weight::BOLD;
  dialog_label->AddStyleRange(controller_->GetMainTextUrlRange(), url_style);
  AddChildView(dialog_label);
}

SettingsResetPromptDialog::~SettingsResetPromptDialog() {
  // Make sure the controller is correctly notified in case the dialog widget is
  // closed by some other means than the dialog buttons.
  Close();
}

void SettingsResetPromptDialog::Show(Browser* browser) {
  DCHECK(browser);
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
  return controller_->GetWindowTitle();
}

// DialogModel overrides.

bool SettingsResetPromptDialog::ShouldDefaultButtonBeBlue() const {
  return true;
}

// DialogDelegate overrides.

base::string16 SettingsResetPromptDialog::GetDialogButtonLabel(
    ui::DialogButton button) const {
  DCHECK(button == ui::DIALOG_BUTTON_OK || button == ui::DIALOG_BUTTON_CANCEL);

  if (button == ui::DIALOG_BUTTON_OK)
    return controller_->GetButtonLabel();
  return DialogDelegate::GetDialogButtonLabel(button);
}

bool SettingsResetPromptDialog::Accept() {
  if (!interaction_done_) {
    interaction_done_ = true;
    controller_->Accept();
  }
  return true;
}

bool SettingsResetPromptDialog::Cancel() {
  if (!interaction_done_) {
    interaction_done_ = true;
    controller_->Cancel();
  }
  return true;
}

// View overrides.

gfx::Size SettingsResetPromptDialog::GetPreferredSize() const {
  return gfx::Size(kDialogWidth, GetHeightForWidth(kDialogWidth));
}
