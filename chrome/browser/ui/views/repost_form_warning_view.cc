// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/repost_form_warning_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/repost_form_warning_controller.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/message_box_flags.h"
#include "ui/views/controls/message_box_view.h"

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
  message_box_view_ = new views::MessageBoxView(
      ui::MessageBoxFlags::kIsConfirmMessageBox,
      l10n_util::GetStringUTF16(IDS_HTTP_POST_WARNING),
      string16());
  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(tab_contents);
  controller_->set_window(new ConstrainedWindowViews(wrapper, this));
}

RepostFormWarningView::~RepostFormWarningView() {
}

//////////////////////////////////////////////////////////////////////////////
// RepostFormWarningView, views::DialogDelegate implementation:

string16 RepostFormWarningView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_HTTP_POST_WARNING_TITLE);
}

string16 RepostFormWarningView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_HTTP_POST_WARNING_RESEND);
  if (button == ui::DIALOG_BUTTON_CANCEL)
    return l10n_util::GetStringUTF16(IDS_CANCEL);
  return string16();
}

views::View* RepostFormWarningView::GetContentsView() {
  return message_box_view_;
}

views::Widget* RepostFormWarningView::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* RepostFormWarningView::GetWidget() const {
  return message_box_view_->GetWidget();
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
