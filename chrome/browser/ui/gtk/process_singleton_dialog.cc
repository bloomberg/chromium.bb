// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/process_singleton_dialog.h"

#include <gtk/gtk.h>

#include "base/message_loop/message_loop.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"

// static
bool ProcessSingletonDialog::ShowAndRun(const std::string& message,
                                        const std::string& relaunch_text) {
  ProcessSingletonDialog dialog(message, relaunch_text);
  return dialog.GetResponseId() == GTK_RESPONSE_ACCEPT;
}

ProcessSingletonDialog::ProcessSingletonDialog(
    const std::string& message,
    const std::string& relaunch_text) {
  dialog_ = gtk_message_dialog_new(
      NULL,
      static_cast<GtkDialogFlags>(0),
      GTK_MESSAGE_ERROR,
      GTK_BUTTONS_NONE,
      "%s",
      message.c_str());
  gtk_util::ApplyMessageDialogQuirks(dialog_);
  gtk_window_set_title(GTK_WINDOW(dialog_),
                       l10n_util::GetStringUTF8(IDS_PRODUCT_NAME).c_str());
  gtk_dialog_add_button(GTK_DIALOG(dialog_), GTK_STOCK_QUIT,
                        GTK_RESPONSE_REJECT);
  gtk_dialog_add_button(GTK_DIALOG(dialog_), relaunch_text.c_str(),
                        GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog_), GTK_RESPONSE_ACCEPT);

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);

  gtk_widget_show_all(dialog_);
  base::MessageLoop::current()->Run();
}

void ProcessSingletonDialog::OnResponse(GtkWidget* dialog, int response_id) {
  response_id_ = response_id;
  gtk_widget_destroy(dialog_);
  base::MessageLoop::current()->Quit();
}
