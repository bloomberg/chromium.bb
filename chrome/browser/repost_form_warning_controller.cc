// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/repost_form_warning_controller.h"

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#endif

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::NavigationController;
using content::WebContents;

RepostFormWarningController::RepostFormWarningController(
    WebContents* web_contents)
    : TabModalConfirmDialogDelegate(web_contents),
      navigation_controller_(&web_contents->GetController()) {
  registrar_.Add(this, content::NOTIFICATION_REPOST_WARNING_SHOWN,
                 content::Source<NavigationController>(
                    navigation_controller_));
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
  navigation_controller_->ContinuePendingReload();
}

void RepostFormWarningController::OnCanceled() {
  navigation_controller_->CancelPendingReload();
}

void RepostFormWarningController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  // Close the dialog if we show an additional dialog, to avoid them
  // stacking up.
  if (type == content::NOTIFICATION_REPOST_WARNING_SHOWN)
    Cancel();
  else
    TabModalConfirmDialogDelegate::Observe(type, source, details);
}
