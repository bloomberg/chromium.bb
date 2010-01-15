// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/create_application_shortcuts_dialog_gtk.h"

#include "app/l10n_util.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/gtk_util.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

// static
void CreateApplicationShortcutsDialogGtk::Show(GtkWindow* parent,
                                               const GURL& url,
                                               const string16& title,
                                               const SkBitmap& favicon) {
  new CreateApplicationShortcutsDialogGtk(parent, url, title, favicon);
}

CreateApplicationShortcutsDialogGtk::CreateApplicationShortcutsDialogGtk(
    GtkWindow* parent,
    const GURL& url,
    const string16& title,
    const SkBitmap& favicon)
    : url_(url),
      title_(title),
      favicon_(favicon) {
  // Build the dialog.
  GtkWidget* dialog = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_CREATE_SHORTCUTS_TITLE).c_str(),
      parent,
      (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_REJECT,
      NULL);
  gtk_widget_realize(dialog);
  gtk_util::SetWindowSizeFromResources(GTK_WINDOW(dialog),
                                       IDS_CREATE_SHORTCUTS_DIALOG_WIDTH_CHARS,
                                       -1,  // height
                                       false);  // resizable
  gtk_util::AddButtonToDialog(dialog,
      l10n_util::GetStringUTF8(IDS_CREATE_SHORTCUTS_COMMIT).c_str(),
      GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT);

  GtkWidget* content_area = GTK_DIALOG(dialog)->vbox;
  gtk_box_set_spacing(GTK_BOX(content_area), gtk_util::kContentAreaSpacing);

  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_container_add(GTK_CONTAINER(content_area), vbox);

  // Label on top of the checkboxes.
  GtkWidget* description = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_CREATE_SHORTCUTS_LABEL).c_str());
  gtk_misc_set_alignment(GTK_MISC(description), 0, 0);
  gtk_box_pack_start(GTK_BOX(vbox), description, FALSE, FALSE, 0);

  // Desktop checkbox.
  desktop_checkbox_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_CREATE_SHORTCUTS_DESKTOP_CHKBOX).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), desktop_checkbox_, FALSE, FALSE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(desktop_checkbox_), true);

  // Menu checkbox.
  menu_checkbox_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_CREATE_SHORTCUTS_MENU_CHKBOX).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), menu_checkbox_, FALSE, FALSE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(menu_checkbox_), false);

  g_signal_connect(dialog, "response",
                   G_CALLBACK(HandleOnResponseDialog), this);
  gtk_widget_show_all(dialog);
}

void CreateApplicationShortcutsDialogGtk::OnDialogResponse(GtkWidget* widget,
                                                           int response) {
  if (response == GTK_RESPONSE_ACCEPT) {
    ShellIntegration::ShortcutInfo shortcut_info;
    shortcut_info.url = url_;
    shortcut_info.title = title_;
    shortcut_info.favicon = favicon_;
    shortcut_info.create_on_desktop =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(desktop_checkbox_));
    shortcut_info.create_in_applications_menu =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(menu_checkbox_));
    ShellIntegration::CreateDesktopShortcut(shortcut_info);
  }

  delete this;
  gtk_widget_destroy(GTK_WIDGET(widget));
}

