// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_modal_dialog.h"

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

AppModalDialog::~AppModalDialog() {
}

void AppModalDialog::CreateAndShowDialog() {
}

// static
void AppModalDialog::OnDialogResponse(GtkDialog* dialog, gint response_id,
                                      AppModalDialog* app_modal_dialog) {
  app_modal_dialog->HandleDialogResponse(dialog, response_id);
}

void AppModalDialog::ActivateModalDialog() {
  DCHECK(dialog_);
  gtk_window_present(GTK_WINDOW(dialog_));
}

void AppModalDialog::CloseModalDialog() {
  DCHECK(dialog_);
  HandleDialogResponse(GTK_DIALOG(dialog_), GTK_RESPONSE_DELETE_EVENT);
}

void AppModalDialog::AcceptWindow() {
  DCHECK(dialog_);
  HandleDialogResponse(GTK_DIALOG(dialog_), GTK_RESPONSE_OK);
}

void AppModalDialog::CancelWindow() {
  DCHECK(dialog_);
  HandleDialogResponse(GTK_DIALOG(dialog_), GTK_RESPONSE_CANCEL);
}
