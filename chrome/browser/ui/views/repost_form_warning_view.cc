// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/repost_form_warning_view.h"

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/repost_form_warning_controller.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/message_box_flags.h"
#include "views/controls/message_box_view.h"
#include "views/window/window.h"

namespace browser {

// Declared in browser_dialogs.h so others don't have to depend on our header.
void ShowRepostFormWarningDialog(gfx::NativeWindow parent_window,
                                 TabContents* tab_contents) {
  new RepostFormWarningView(parent_window, tab_contents);
}

}  // namespace browser

//////////////////////////////////////////////////////////////////////////////
// RepostFormWarningView, constructor & destructor:

RepostFormWarningView::RepostFormWarningView(
    gfx::NativeWindow parent_window,
    TabContents* tab_contents)
      : controller_(new RepostFormWarningController(tab_contents)),
        message_box_view_(NULL) {
  message_box_view_ = new MessageBoxView(
      ui::MessageBoxFlags::kIsConfirmMessageBox,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_HTTP_POST_WARNING)),
      std::wstring());
  controller_->Show(this);
}

RepostFormWarningView::~RepostFormWarningView() {
}

//////////////////////////////////////////////////////////////////////////////
// RepostFormWarningView, views::DialogDelegate implementation:

std::wstring RepostFormWarningView::GetWindowTitle() const {
  return UTF16ToWide(l10n_util::GetStringUTF16(IDS_HTTP_POST_WARNING_TITLE));
}

std::wstring RepostFormWarningView::GetDialogButtonLabel(
    ui::MessageBoxFlags::DialogButton button) const {
  if (button == ui::MessageBoxFlags::DIALOGBUTTON_OK)
    return UTF16ToWide(l10n_util::GetStringUTF16(IDS_HTTP_POST_WARNING_RESEND));
  if (button == ui::MessageBoxFlags::DIALOGBUTTON_CANCEL)
    return UTF16ToWide(l10n_util::GetStringUTF16(IDS_CANCEL));
  return std::wstring();
}

views::View* RepostFormWarningView::GetContentsView() {
  return message_box_view_;
}

bool RepostFormWarningView::Cancel() {
  controller_->Cancel();
  return true;
}

bool RepostFormWarningView::Accept() {
  controller_->Continue();
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// RepostFormWarningView, RepostFormWarning implementation:

void RepostFormWarningView::DeleteDelegate() {
  delete this;
}
