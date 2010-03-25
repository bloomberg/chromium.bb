// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/create_application_shortcuts_dialog_gtk.h"

#include "app/l10n_util.h"
#include "base/linux_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

// static
void CreateApplicationShortcutsDialogGtk::Show(GtkWindow* parent,
                                               TabContents* tab_contents) {
  new CreateApplicationShortcutsDialogGtk(parent, tab_contents);
}

CreateApplicationShortcutsDialogGtk::CreateApplicationShortcutsDialogGtk(
    GtkWindow* parent,
    TabContents* tab_contents)
    : tab_contents_(tab_contents),
      url_(tab_contents->GetURL()),
      title_(tab_contents->GetTitle()),
      favicon_(tab_contents->FavIconIsValid() ? tab_contents->GetFavIcon() :
                                                SkBitmap()),
      error_dialog_(NULL) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  // Will be balanced by Release later.
  AddRef();

  // Build the dialog.
  create_dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_CREATE_SHORTCUTS_TITLE).c_str(),
      parent,
      (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_REJECT,
      NULL);
  gtk_widget_realize(create_dialog_);
  gtk_util::SetWindowSizeFromResources(GTK_WINDOW(create_dialog_),
                                       IDS_CREATE_SHORTCUTS_DIALOG_WIDTH_CHARS,
                                       -1,  // height
                                       false);  // resizable
  gtk_util::AddButtonToDialog(create_dialog_,
      l10n_util::GetStringUTF8(IDS_CREATE_SHORTCUTS_COMMIT).c_str(),
      GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT);

  GtkWidget* content_area = GTK_DIALOG(create_dialog_)->vbox;
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

  g_signal_connect(create_dialog_, "response",
                   G_CALLBACK(HandleOnResponseCreateDialog), this);
  gtk_widget_show_all(create_dialog_);
}

CreateApplicationShortcutsDialogGtk::~CreateApplicationShortcutsDialogGtk() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  gtk_widget_destroy(create_dialog_);

  if (error_dialog_)
    gtk_widget_destroy(error_dialog_);
}

void CreateApplicationShortcutsDialogGtk::OnCreateDialogResponse(
    GtkWidget* widget, int response) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  if (response == GTK_RESPONSE_ACCEPT) {
    ShellIntegration::ShortcutInfo shortcut_info;
    shortcut_info.url = url_;
    shortcut_info.title = title_;
    shortcut_info.favicon = favicon_;
    shortcut_info.create_on_desktop =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(desktop_checkbox_));
    shortcut_info.create_in_applications_menu =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(menu_checkbox_));
    ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
         NewRunnableMethod(this,
             &CreateApplicationShortcutsDialogGtk::CreateDesktopShortcut,
             shortcut_info));

    if (tab_contents_->delegate())
      tab_contents_->delegate()->ConvertContentsToApplication(tab_contents_);
  } else {
    Release();
  }
}

void CreateApplicationShortcutsDialogGtk::OnErrorDialogResponse(
    GtkWidget* widget, int response) {
  Release();
}

void CreateApplicationShortcutsDialogGtk::CreateDesktopShortcut(
    const ShellIntegration::ShortcutInfo& shortcut_info) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  scoped_ptr<base::EnvironmentVariableGetter> env_getter(
      base::EnvironmentVariableGetter::Create());

  std::string shortcut_template;
  if (ShellIntegration::GetDesktopShortcutTemplate(env_getter.get(),
                                                   &shortcut_template)) {
    ShellIntegration::CreateDesktopShortcut(shortcut_info,
                                            shortcut_template);
    Release();
  } else {
    ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this,
            &CreateApplicationShortcutsDialogGtk::ShowErrorDialog));
  }
}

void CreateApplicationShortcutsDialogGtk::ShowErrorDialog() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  // Hide the create dialog so that the user can no longer interact with it.
  gtk_widget_hide(create_dialog_);

  error_dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_CREATE_SHORTCUTS_ERROR_TITLE).c_str(),
      NULL,
      (GtkDialogFlags) (GTK_DIALOG_NO_SEPARATOR),
      GTK_STOCK_OK,
      GTK_RESPONSE_ACCEPT,
      NULL);
  gtk_widget_realize(error_dialog_);
  gtk_util::SetWindowSizeFromResources(
      GTK_WINDOW(error_dialog_),
      IDS_CREATE_SHORTCUTS_ERROR_DIALOG_WIDTH_CHARS,
      IDS_CREATE_SHORTCUTS_ERROR_DIALOG_HEIGHT_LINES,
      false);  // resizable
  GtkWidget* content_area = GTK_DIALOG(error_dialog_)->vbox;
  gtk_box_set_spacing(GTK_BOX(content_area), gtk_util::kContentAreaSpacing);

  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_container_add(GTK_CONTAINER(content_area), vbox);

  // Label on top of the checkboxes.
  GtkWidget* description = gtk_label_new(
      l10n_util::GetStringFUTF8(
          IDS_CREATE_SHORTCUTS_ERROR_LABEL,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(description), TRUE);
  gtk_misc_set_alignment(GTK_MISC(description), 0, 0);
  gtk_box_pack_start(GTK_BOX(vbox), description, FALSE, FALSE, 0);

  g_signal_connect(error_dialog_, "response",
                   G_CALLBACK(HandleOnResponseErrorDialog), this);
  gtk_widget_show_all(error_dialog_);
}
