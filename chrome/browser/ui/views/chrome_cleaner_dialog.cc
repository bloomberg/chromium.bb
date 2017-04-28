// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_cleaner_dialog.h"

#include "base/strings/string16.h"
#include "chrome/browser/safe_browsing/chrome_cleaner_dialog_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace chrome {

void ShowChromeCleanerPrompt(
    Browser* browser,
    safe_browsing::ChromeCleanerDialogController* controller) {
  ChromeCleanerDialog* dialog = new ChromeCleanerDialog(controller);
  dialog->Show(browser);
}

}  // namespace chrome

namespace {
constexpr int kDialogWidth = 448;
}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ChromeCleanerDialog

ChromeCleanerDialog::ChromeCleanerDialog(
    safe_browsing::ChromeCleanerDialogController* controller)
    : browser_(nullptr),
      controller_(controller),
      advanced_button_(
          new views::LabelButton(this, controller_->GetAdvancedButtonLabel())) {
  DCHECK(controller_);

  SetLayoutManager(new views::BoxLayout(
      /*orientation=*/views::BoxLayout::kVertical,
      /*inside_border_horizontal_spacing=*/views::kButtonHEdgeMarginNew,
      /*inside_border_vertical_spacing=*/views::kPanelVertMargin,
      /*between_child_spacing=*/0));
  views::Label* label = new views::Label(controller_->GetMainText());
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(label);

  advanced_button_->SetStyle(views::Button::STYLE_BUTTON);
}

ChromeCleanerDialog::~ChromeCleanerDialog() {
  // Make sure the controller is correctly notified in case the dialog widget is
  // closed by some other means than the dialog buttons.
  if (controller_)
    controller_->Cancel();
}

void ChromeCleanerDialog::Show(Browser* browser) {
  DCHECK(browser);
  DCHECK(!browser_);
  DCHECK(controller_);

  browser_ = browser;
  constrained_window::CreateBrowserModalDialogViews(
      this, browser_->window()->GetNativeWindow())
      ->Show();
  controller_->DialogShown();
}

// DialogModel overrides.

bool ChromeCleanerDialog::ShouldDefaultButtonBeBlue() const {
  return true;
}

// WidgetDelegate overrides.

ui::ModalType ChromeCleanerDialog::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 ChromeCleanerDialog::GetWindowTitle() const {
  DCHECK(controller_);
  return controller_->GetWindowTitle();
}

// DialogDelegate overrides.

base::string16 ChromeCleanerDialog::GetDialogButtonLabel(
    ui::DialogButton button) const {
  DCHECK(button == ui::DIALOG_BUTTON_OK || button == ui::DIALOG_BUTTON_CANCEL);
  DCHECK(controller_);

  return button == ui::DIALOG_BUTTON_OK
             ? controller_->GetAcceptButtonLabel()
             : DialogDelegate::GetDialogButtonLabel(button);
}

views::View* ChromeCleanerDialog::CreateExtraView() {
  return advanced_button_;
}

bool ChromeCleanerDialog::Accept() {
  if (controller_) {
    controller_->Accept();
    controller_ = nullptr;
  }
  return true;
}

bool ChromeCleanerDialog::Cancel() {
  if (controller_) {
    controller_->Cancel();
    controller_ = nullptr;
  }
  return true;
}

bool ChromeCleanerDialog::Close() {
  if (controller_) {
    controller_->Close();
    controller_ = nullptr;
  }
  return true;
}

// View overrides.

gfx::Size ChromeCleanerDialog::GetPreferredSize() const {
  return gfx::Size(kDialogWidth, GetHeightForWidth(kDialogWidth));
}

// views::ButtonListener overrides.

void ChromeCleanerDialog::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  DCHECK_EQ(sender, advanced_button_);
  DCHECK(browser_);

  // TODO(alito): Navigate to the webui version of the Chrome Cleaner UI when
  // that is implemented.
  if (controller_) {
    controller_->AdvancedButtonClicked();
    controller_ = nullptr;
  }
  GetWidget()->Close();
}
