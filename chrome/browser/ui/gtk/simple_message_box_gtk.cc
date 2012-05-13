// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/simple_message_box.h"

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/gtk/gtk_util.h"

namespace {

void SetDialogTitle(GtkWidget* dialog, const string16& title) {
  gtk_window_set_title(GTK_WINDOW(dialog), UTF16ToUTF8(title).c_str());

  // The following code requires the dialog to be realized.
  gtk_widget_realize(dialog);

  // Make sure it's big enough to show the title.
  GtkRequisition req;
  gtk_widget_size_request(dialog, &req);
  int width;
  gtk_util::GetWidgetSizeFromCharacters(dialog, title.length(), 0,
                                        &width, NULL);
  // The fudge factor accounts for extra space needed by the frame
  // decorations as well as width differences between average text and the
  // actual title text.
  width = width * 1.2 + 50;

  if (width > req.width)
    gtk_widget_set_size_request(dialog, width, -1);
}

int g_dialog_response;

void HandleOnResponseDialog(GtkWidget* widget, int response, void* user_data) {
  g_dialog_response = response;
  gtk_widget_destroy(widget);
  MessageLoop::current()->QuitNow();
}

}  // namespace

namespace browser {

void ShowWarningMessageBox(gfx::NativeWindow parent,
                           const string16& title,
                           const string16& message) {
  GtkWidget* dialog = gtk_message_dialog_new(parent,
                                             GTK_DIALOG_MODAL,
                                             GTK_MESSAGE_WARNING,
                                             GTK_BUTTONS_OK,
                                             "%s",
                                             UTF16ToUTF8(message).c_str());
  gtk_util::ApplyMessageDialogQuirks(dialog);
  SetDialogTitle(dialog, title);

  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
  g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
  gtk_util::ShowDialog(dialog);
}

bool ShowQuestionMessageBox(gfx::NativeWindow parent,
                            const string16& title,
                            const string16& message) {
  GtkWidget* dialog = gtk_message_dialog_new(parent,
                                             GTK_DIALOG_MODAL,
                                             GTK_MESSAGE_QUESTION,
                                             GTK_BUTTONS_YES_NO,
                                             "%s",
                                             UTF16ToUTF8(message).c_str());
  gtk_util::ApplyMessageDialogQuirks(dialog);
  SetDialogTitle(dialog, title);

  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);
  g_signal_connect(dialog,
                   "response",
                   G_CALLBACK(HandleOnResponseDialog),
                   NULL);
  gtk_util::ShowDialog(dialog);
  // Not gtk_dialog_run as it prevents timers from running in the unit tests.
  MessageLoop::current()->Run();
  return g_dialog_response == GTK_RESPONSE_YES;
}

}  // namespace browser
