// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/settings_reset_prompt_dialog.h"

#include <utility>

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
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

SettingsResetPromptDialog::SettingsResetPromptDialog(
    safe_browsing::SettingsResetPromptController* controller)
    : browser_(nullptr), controller_(controller) {
  DCHECK(controller_);

  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_SETTINGS_RESET_PROMPT_ACCEPT_BUTTON_LABEL));

  // There is at most one of {Accept(), Cancel(), Close()} will be run for
  // |controller_|. Each of them causes |controller_| deletion.
  SetAcceptCallback(base::BindOnce(
      [](SettingsResetPromptDialog* dialog) {
        std::exchange(dialog->controller_, nullptr)->Accept();
      },
      base::Unretained(this)));
  SetCancelCallback(base::BindOnce(
      [](SettingsResetPromptDialog* dialog) {
        std::exchange(dialog->controller_, nullptr)->Cancel();
      },
      base::Unretained(this)));
  SetCloseCallback(base::BindOnce(
      [](SettingsResetPromptDialog* dialog) {
        std::exchange(dialog->controller_, nullptr)->Close();
      },
      base::Unretained(this)));

  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  views::StyledLabel* dialog_label =
      new views::StyledLabel(controller_->GetMainText(), /*listener=*/nullptr);
  dialog_label->SetTextContext(CONTEXT_BODY_TEXT_LARGE);
  dialog_label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  views::StyledLabel::RangeStyleInfo url_style;
  url_style.text_style = STYLE_EMPHASIZED_SECONDARY;
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

bool SettingsResetPromptDialog::ShouldShowCloseButton() const {
  return false;
}

base::string16 SettingsResetPromptDialog::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : base::string16();
}

// View overrides.

gfx::Size SettingsResetPromptDialog::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}
