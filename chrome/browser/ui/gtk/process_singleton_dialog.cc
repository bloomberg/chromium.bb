// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/process_singleton_dialog.h"

#include "base/message_loop.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"

// static
void ProcessSingletonDialog::ShowAndRun(const std::string& message) {
  ProcessSingletonDialog dialog(message);
}

ProcessSingletonDialog::ProcessSingletonDialog(const std::string& message) {
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

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponse), this);

  gtk_widget_show_all(dialog_);
  MessageLoop::current()->Run();
}

// static
void ProcessSingletonDialog::OnResponse(GtkWidget* widget, int response,
                                        ProcessSingletonDialog* dialog) {
  gtk_widget_destroy(dialog->dialog_);
  MessageLoop::current()->Quit();
}
