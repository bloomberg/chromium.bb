// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include <gtk/gtk.h>

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "content/common/process_watcher.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/native_widget_types.h"

namespace {

void SetDialogTitle(GtkWidget* dialog, const string16& title) {
  gtk_window_set_title(GTK_WINDOW(dialog), UTF16ToUTF8(title).c_str());

#if !defined(OS_CHROMEOS)
  // The following code requires the dialog to be realized. However, we host
  // dialog's content in a Chrome window without really realize the dialog
  // on ChromeOS. Thus, skip the following code for ChromeOS.
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
#endif  // !defined(OS_CHROMEOS)
}

int g_dialog_response;

void HandleOnResponseDialog(GtkWidget* widget,
                            int response,
                            void* user_data) {
  g_dialog_response = response;
  gtk_widget_destroy(widget);
  MessageLoop::current()->QuitNow();
}

}  // namespace

namespace platform_util {

gfx::NativeWindow GetTopLevel(gfx::NativeView view) {
  // A detached widget won't have a toplevel window as an ancestor, so we can't
  // assume that the query for toplevel will return a window.
  GtkWidget* toplevel = gtk_widget_get_ancestor(view, GTK_TYPE_WINDOW);
  return GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : NULL;
}

gfx::NativeView GetParent(gfx::NativeView view) {
  return gtk_widget_get_parent(view);
}

bool IsWindowActive(gfx::NativeWindow window) {
  return gtk_window_is_active(window);
}

void ActivateWindow(gfx::NativeWindow window) {
  gtk_window_present(window);
}

bool IsVisible(gfx::NativeView view) {
  return GTK_WIDGET_VISIBLE(view);
}

void SimpleErrorBox(gfx::NativeWindow parent,
                    const string16& title,
                    const string16& message) {
  GtkWidget* dialog = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL,
      GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", UTF16ToUTF8(message).c_str());
  gtk_util::ApplyMessageDialogQuirks(dialog);
  SetDialogTitle(dialog, title);

  g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
  gtk_util::ShowDialog(dialog);
}

bool SimpleYesNoBox(gfx::NativeWindow parent,
                    const string16& title,
                    const string16& message) {
  GtkWidget* dialog = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL,
      GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "%s",
      UTF16ToUTF8(message).c_str());
  gtk_util::ApplyMessageDialogQuirks(dialog);
  SetDialogTitle(dialog, title);

  g_signal_connect(dialog,
                   "response",
                   G_CALLBACK(HandleOnResponseDialog),
                   NULL);
  gtk_util::ShowDialog(dialog);
  // Not gtk_dialog_run as it prevents timers from running in the unit tests.
  MessageLoop::current()->Run();
  return g_dialog_response == GTK_RESPONSE_YES;
}

// Warning: this may be either Linux or ChromeOS.
std::string GetVersionStringModifier() {
  char* env = getenv("CHROME_VERSION_EXTRA");
  if (!env)
    return std::string();
  std::string modifier(env);

#if defined(GOOGLE_CHROME_BUILD)
  // Only ever return "", "unknown", "dev" or "beta" in a branded build.
  if (modifier == "unstable")  // linux version of "dev"
    modifier = "dev";
  if (modifier == "stable") {
    modifier = "";
  } else if ((modifier == "dev") || (modifier == "beta")) {
    // do nothing.
  } else {
    modifier = "unknown";
  }
#endif

  return modifier;
}

bool CanSetAsDefaultBrowser() {
  return true;
}

}  // namespace platform_util
