// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/chooser_dialog_view.h"

#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "chrome/browser/extensions/api/chrome_device_permissions_prompt.h"
#include "chrome/browser/extensions/chrome_extension_chooser_dialog.h"
#include "chrome/browser/extensions/device_permissions_dialog_controller.h"
#include "chrome/browser/ui/views/chooser_content_view.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/window/dialog_client_view.h"

ChooserDialogView::ChooserDialogView(
    std::unique_ptr<ChooserController> chooser_controller) {
  // ------------------------------------
  // | Chooser dialog title             |
  // | -------------------------------- |
  // | | option 0                     | |
  // | | option 1                     | |
  // | | option 2                     | |
  // | |                              | |
  // | |                              | |
  // | |                              | |
  // | -------------------------------- |
  // |           [ Connect ] [ Cancel ] |
  // |----------------------------------|
  // | Get help                         |
  // ------------------------------------

  DCHECK(chooser_controller);
  chooser_content_view_ =
      new ChooserContentView(this, std::move(chooser_controller));
}

ChooserDialogView::~ChooserDialogView() {}

base::string16 ChooserDialogView::GetWindowTitle() const {
  return chooser_content_view_->GetWindowTitle();
}

bool ChooserDialogView::ShouldShowCloseButton() const {
  return false;
}

ui::ModalType ChooserDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

base::string16 ChooserDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return chooser_content_view_->GetDialogButtonLabel(button);
}

bool ChooserDialogView::IsDialogButtonEnabled(ui::DialogButton button) const {
  return chooser_content_view_->IsDialogButtonEnabled(button);
}

views::View* ChooserDialogView::CreateFootnoteView() {
  return chooser_content_view_->footnote_link();
}

views::ClientView* ChooserDialogView::CreateClientView(views::Widget* widget) {
  views::DialogClientView* client =
      new views::DialogClientView(widget, GetContentsView());
  client->set_button_row_insets(gfx::Insets());
  return client;
}

views::NonClientFrameView* ChooserDialogView::CreateNonClientFrameView(
    views::Widget* widget) {
  // ChooserDialogView always has a parent, so ShouldUseCustomFrame() should
  // always be true.
  DCHECK(ShouldUseCustomFrame());
  return views::DialogDelegate::CreateDialogFrameView(
      widget, gfx::Insets(views::kPanelVertMargin, views::kPanelHorizMargin,
                          views::kPanelVertMargin, views::kPanelHorizMargin));
}

bool ChooserDialogView::Accept() {
  chooser_content_view_->Accept();
  return true;
}

bool ChooserDialogView::Cancel() {
  chooser_content_view_->Cancel();
  return true;
}

bool ChooserDialogView::Close() {
  chooser_content_view_->Close();
  return true;
}

views::View* ChooserDialogView::GetContentsView() {
  return chooser_content_view_;
}

views::Widget* ChooserDialogView::GetWidget() {
  return chooser_content_view_->GetWidget();
}

const views::Widget* ChooserDialogView::GetWidget() const {
  return chooser_content_view_->GetWidget();
}

void ChooserDialogView::OnSelectionChanged() {
  GetDialogClientView()->UpdateDialogButtons();
}

ChooserContentView* ChooserDialogView::chooser_content_view_for_test() const {
  return chooser_content_view_;
}

void ChromeExtensionChooserDialog::ShowDialogImpl(
    std::unique_ptr<ChooserController> chooser_controller) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(chooser_controller);

  web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents_);
  if (manager) {
    constrained_window::ShowWebModalDialogViews(
        new ChooserDialogView(std::move(chooser_controller)), web_contents_);
  }
}

void ChromeDevicePermissionsPrompt::ShowDialogViews() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<ChooserController> chooser_controller(
      new DevicePermissionsDialogController(web_contents()->GetMainFrame(),
                                            prompt()));

  constrained_window::ShowWebModalDialogViews(
      new ChooserDialogView(std::move(chooser_controller)), web_contents());
}
