// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/repost_form_warning_controller.h"

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#endif

#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

RepostFormWarningController::RepostFormWarningController(
    content::WebContents* web_contents)
    : TabModalConfirmDialogDelegate(web_contents),
      content::WebContentsObserver(web_contents) {
}

RepostFormWarningController::~RepostFormWarningController() {
}

string16 RepostFormWarningController::GetTitle() {
  return l10n_util::GetStringUTF16(IDS_HTTP_POST_WARNING_TITLE);
}

string16 RepostFormWarningController::GetMessage() {
  return l10n_util::GetStringUTF16(IDS_HTTP_POST_WARNING);
}

string16 RepostFormWarningController::GetAcceptButtonTitle() {
  return l10n_util::GetStringUTF16(IDS_HTTP_POST_WARNING_RESEND);
}

#if defined(TOOLKIT_GTK)
const char* RepostFormWarningController::GetAcceptButtonIcon() {
  return GTK_STOCK_REFRESH;
}

const char* RepostFormWarningController::GetCancelButtonIcon() {
  return GTK_STOCK_CANCEL;
}
#endif  // defined(TOOLKIT_GTK)

void RepostFormWarningController::OnAccepted() {
  web_contents()->GetController().ContinuePendingReload();
}

void RepostFormWarningController::OnCanceled() {
  web_contents()->GetController().CancelPendingReload();
}

void RepostFormWarningController::BeforeFormRepostWarningShow() {
  // Close the dialog if we show an additional dialog, to avoid them
  // stacking up.
  Cancel();
}
