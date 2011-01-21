// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/options/passwords_exceptions_window_gtk.h"

#include <gtk/gtk.h>

#include <string>

#include "base/message_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/accessible_widget_helper_gtk.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/options/passwords_exceptions_page_gtk.h"
#include "chrome/browser/ui/gtk/options/passwords_page_gtk.h"
#include "chrome/browser/ui/options/options_window.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

///////////////////////////////////////////////////////////////////////////////
// PasswordsExceptionsWindowGtk
//
// The contents of the Passwords and Exceptions dialog window.

class PasswordsExceptionsWindowGtk {
 public:
  explicit PasswordsExceptionsWindowGtk(Profile* profile);
  ~PasswordsExceptionsWindowGtk();

  void Show();

 private:
  static void OnWindowDestroy(GtkWidget* widget,
                              PasswordsExceptionsWindowGtk* window);

  // The passwords and exceptions dialog.
  GtkWidget *dialog_;

  // The container of the password and exception pages.
  GtkWidget *notebook_;

  // The Profile associated with these passwords and exceptions.
  Profile* profile_;

  // The passwords page.
  PasswordsPageGtk passwords_page_;

  // The exceptions page.
  PasswordsExceptionsPageGtk exceptions_page_;

  // Helper object to manage accessibility metadata.
  scoped_ptr<AccessibleWidgetHelper> accessible_widget_helper_;

  DISALLOW_COPY_AND_ASSIGN(PasswordsExceptionsWindowGtk);
};

// The singleton passwords and exceptions window object.
static PasswordsExceptionsWindowGtk* passwords_exceptions_window = NULL;

///////////////////////////////////////////////////////////////////////////////
// PasswordsExceptionsWindowGtk, public:

PasswordsExceptionsWindowGtk::PasswordsExceptionsWindowGtk(Profile* profile)
    : profile_(profile),
      passwords_page_(profile_),
      exceptions_page_(profile_) {
  std::string dialog_name = l10n_util::GetStringUTF8(
      IDS_PASSWORDS_EXCEPTIONS_WINDOW_TITLE);
  dialog_ = gtk_dialog_new_with_buttons(
      dialog_name.c_str(),
      // Passwords and exceptions window is shared between all browser windows.
      NULL,
      // Non-modal.
      GTK_DIALOG_NO_SEPARATOR,
      GTK_STOCK_CLOSE,
      GTK_RESPONSE_CLOSE,
      NULL);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);

  accessible_widget_helper_.reset(new AccessibleWidgetHelper(
      dialog_, profile));
  accessible_widget_helper_->SendOpenWindowNotification(dialog_name);

  notebook_ = gtk_notebook_new();

  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook_),
      passwords_page_.get_page_widget(),
      gtk_label_new(l10n_util::GetStringUTF8(
              IDS_PASSWORDS_SHOW_PASSWORDS_TAB_TITLE).c_str()));

  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook_),
      exceptions_page_.get_page_widget(),
      gtk_label_new(l10n_util::GetStringUTF8(
              IDS_PASSWORDS_EXCEPTIONS_TAB_TITLE).c_str()));

  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), notebook_);

  // We only have one button and don't do any special handling, so just hook it
  // directly to gtk_widget_destroy.
  g_signal_connect(dialog_, "response", G_CALLBACK(gtk_widget_destroy), NULL);

  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnWindowDestroy), this);

  gtk_util::ShowDialogWithLocalizedSize(dialog_,
      IDS_PASSWORDS_DIALOG_WIDTH_CHARS,
      IDS_PASSWORDS_DIALOG_HEIGHT_LINES,
      true);
}

PasswordsExceptionsWindowGtk::~PasswordsExceptionsWindowGtk() {
}

void PasswordsExceptionsWindowGtk::Show() {
  // Bring options window to front if it already existed and isn't already
  // in front
  gtk_util::PresentWindow(dialog_, 0);
}

///////////////////////////////////////////////////////////////////////////////
// PasswordsExceptionsWindowGtk, private:

// static
void PasswordsExceptionsWindowGtk::OnWindowDestroy(GtkWidget* widget,
    PasswordsExceptionsWindowGtk* window) {
  passwords_exceptions_window = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, window);
}

///////////////////////////////////////////////////////////////////////////////
// Factory/finder method:

void ShowPasswordsExceptionsWindow(Profile* profile) {
  DCHECK(profile);
  // If there's already an existing passwords and exceptions window, use it.
  if (!passwords_exceptions_window) {
    passwords_exceptions_window = new PasswordsExceptionsWindowGtk(profile);
  }
  passwords_exceptions_window->Show();
}
