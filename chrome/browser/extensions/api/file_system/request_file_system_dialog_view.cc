// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/request_file_system_dialog_view.h"

#include <cstdlib>

#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace {

// Maximum width of the dialog in pixels.
const int kDialogMaxWidth = 320;

}  // namespace

// static
void RequestFileSystemDialogView::ShowDialog(
    content::WebContents* web_contents,
    const extensions::Extension& extension,
    base::WeakPtr<file_manager::Volume> volume,
    bool writable,
    const base::Callback<void(ui::DialogButton)>& callback) {
  constrained_window::ShowWebModalDialogViews(
      new RequestFileSystemDialogView(extension, volume, writable, callback),
      web_contents);
}

RequestFileSystemDialogView::~RequestFileSystemDialogView() {
}

base::string16 RequestFileSystemDialogView::GetDialogTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_TITLE);
}

int RequestFileSystemDialogView::GetDefaultDialogButton() const {
  return ui::DIALOG_BUTTON_CANCEL;
}

base::string16 RequestFileSystemDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  switch (button) {
    case ui::DIALOG_BUTTON_OK:
      return l10n_util::GetStringUTF16(
          IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_YES_BUTTON);
    case ui::DIALOG_BUTTON_CANCEL:
      return l10n_util::GetStringUTF16(
          IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_NO_BUTTON);
    default:
      NOTREACHED();
  }
  return base::string16();
}

ui::ModalType RequestFileSystemDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

views::View* RequestFileSystemDialogView::GetContentsView() {
  return contents_view_;
}

views::Widget* RequestFileSystemDialogView::GetWidget() {
  return contents_view_->GetWidget();
}

const views::Widget* RequestFileSystemDialogView::GetWidget() const {
  return contents_view_->GetWidget();
}

bool RequestFileSystemDialogView::Cancel() {
  callback_.Run(ui::DIALOG_BUTTON_CANCEL);
  return true;
}

bool RequestFileSystemDialogView::Accept() {
  callback_.Run(ui::DIALOG_BUTTON_OK);
  return true;
}

RequestFileSystemDialogView::RequestFileSystemDialogView(
    const extensions::Extension& extension,
    base::WeakPtr<file_manager::Volume> volume,
    bool writable,
    const base::Callback<void(ui::DialogButton)>& callback)
    : callback_(callback), contents_view_(new views::View) {
  DCHECK(!callback_.is_null());

  // If the volume is gone, then cancel the dialog.
  if (!volume.get()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, ui::DIALOG_BUTTON_CANCEL));
    return;
  }

  // TODO(mtomasz): Improve the dialog contents, so it's easier for the user
  // to understand what device is being requested.
  const base::string16 display_name =
      base::UTF8ToUTF16(!volume->volume_label().empty() ? volume->volume_label()
                                                        : volume->volume_id());
  const base::string16 message = l10n_util::GetStringFUTF16(
      writable ? IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_WRITABLE_MESSAGE
               : IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_MESSAGE,
      display_name);

  // Find location of the placeholder by comparing message and message_host
  // strings in order to apply the bold style on the display name.
  const base::string16 message_host = l10n_util::GetStringFUTF16(
      writable ? IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_WRITABLE_MESSAGE
               : IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_MESSAGE,
      base::string16());

  size_t placeholder_start = 0;
  while (placeholder_start < message.size() &&
         message[placeholder_start] == message_host[placeholder_start]) {
    ++placeholder_start;
  }

  views::StyledLabel* const label = new views::StyledLabel(message, nullptr);
  views::StyledLabel::RangeStyleInfo bold_style;
  bold_style.font_style = gfx::Font::BOLD;

  label->AddStyleRange(
      gfx::Range(placeholder_start, placeholder_start + display_name.size()),
      bold_style);

  views::BoxLayout* const layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, views::kButtonHEdgeMarginNew,
      views::kPanelVertMargin, 0);
  contents_view_->SetLayoutManager(layout);

  label->SizeToFit(kDialogMaxWidth - 2 * views::kButtonHEdgeMarginNew);
  contents_view_->AddChildView(label);
}
