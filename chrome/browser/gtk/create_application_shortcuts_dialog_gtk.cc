// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/create_application_shortcuts_dialog_gtk.h"

#include <string>

#include "app/gtk_util.h"
#include "app/l10n_util.h"
#include "base/env_var.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/web_applications/web_app.h"
#include "gfx/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

namespace {

// Size (in pixels) of the icon preview.
const int kIconPreviewSizePixels = 32;

// Height (in lines) of the shortcut description label.
const int kDescriptionLabelHeightLines = 3;

}  // namespace

// static
void CreateApplicationShortcutsDialogGtk::Show(GtkWindow* parent,
                                               TabContents* tab_contents) {
  new CreateApplicationShortcutsDialogGtk(parent, tab_contents);
}

CreateApplicationShortcutsDialogGtk::CreateApplicationShortcutsDialogGtk(
    GtkWindow* parent,
    TabContents* tab_contents)
    : tab_contents_(tab_contents),
      error_dialog_(NULL) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  // Will be balanced by Release later.
  AddRef();

  // Get shortcut information now, it's needed for our UI.
  web_app::GetShortcutInfoForTab(tab_contents_, &shortcut_info_);

  // Prepare the icon. Try to scale it if it's too small, otherwise it would
  // look weird.
  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&shortcut_info_.favicon);
  int pixbuf_width = gdk_pixbuf_get_width(pixbuf);
  int pixbuf_height = gdk_pixbuf_get_height(pixbuf);
  if (pixbuf_width == pixbuf_height && pixbuf_width < kIconPreviewSizePixels) {
    // Only scale the pixbuf if it's a square (for simplicity).
    // Generally it should be square, if it's a favicon.
    // Use the highest quality interpolation. The scaling is
    // going to have low quality anyway, because the initial image
    // is likely small.
    favicon_pixbuf_ = gdk_pixbuf_scale_simple(pixbuf,
                                              kIconPreviewSizePixels,
                                              kIconPreviewSizePixels,
                                              GDK_INTERP_HYPER);
    g_object_unref(pixbuf);
  } else {
    favicon_pixbuf_ = pixbuf;
  }

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

  // Create a box containing basic information about the new shortcut: an image
  // on the left, and a description on the right.
  GtkWidget* hbox = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(hbox),
                                 gtk_util::kControlSpacing);

  // Put the icon preview in place.
  GtkWidget* favicon_image = gtk_image_new_from_pixbuf(favicon_pixbuf_);
  gtk_box_pack_start(GTK_BOX(hbox), favicon_image, FALSE, FALSE, 0);

  // Create the label with application shortcut description.
  GtkWidget* description_label = gtk_label_new(NULL);
  gtk_box_pack_start(GTK_BOX(hbox), description_label, FALSE, FALSE, 0);
  gtk_label_set_line_wrap(GTK_LABEL(description_label), TRUE);
  gtk_widget_realize(description_label);
  int label_height;
  gtk_util::GetWidgetSizeFromCharacters(description_label, -1,
                                        kDescriptionLabelHeightLines, NULL,
                                        &label_height);
  gtk_widget_set_size_request(description_label, -1, label_height);
  gtk_misc_set_alignment(GTK_MISC(description_label), 0, 0.5);
  std::string description(UTF16ToUTF8(shortcut_info_.description));
  std::string title(UTF16ToUTF8(shortcut_info_.title));
  gtk_label_set_text(GTK_LABEL(description_label),
                     (description.empty() ? title : description).c_str());

  // Label on top of the checkboxes.
  GtkWidget* checkboxes_label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_CREATE_SHORTCUTS_LABEL).c_str());
  gtk_misc_set_alignment(GTK_MISC(checkboxes_label), 0, 0);
  gtk_box_pack_start(GTK_BOX(vbox), checkboxes_label, FALSE, FALSE, 0);

  // Desktop checkbox.
  desktop_checkbox_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_CREATE_SHORTCUTS_DESKTOP_CHKBOX).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), desktop_checkbox_, FALSE, FALSE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(desktop_checkbox_), true);
  g_signal_connect(desktop_checkbox_, "toggled",
                   G_CALLBACK(OnToggleCheckboxThunk), this);

  // Menu checkbox.
  menu_checkbox_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_CREATE_SHORTCUTS_MENU_CHKBOX).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), menu_checkbox_, FALSE, FALSE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(menu_checkbox_), false);
  g_signal_connect(menu_checkbox_, "toggled",
                   G_CALLBACK(OnToggleCheckboxThunk), this);

  g_signal_connect(create_dialog_, "response",
                   G_CALLBACK(OnCreateDialogResponseThunk), this);
  gtk_widget_show_all(create_dialog_);
}

CreateApplicationShortcutsDialogGtk::~CreateApplicationShortcutsDialogGtk() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  gtk_widget_destroy(create_dialog_);

  if (error_dialog_)
    gtk_widget_destroy(error_dialog_);

  g_object_unref(favicon_pixbuf_);
}

void CreateApplicationShortcutsDialogGtk::OnCreateDialogResponse(
    GtkWidget* widget, int response) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  if (response == GTK_RESPONSE_ACCEPT) {
    shortcut_info_.create_on_desktop =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(desktop_checkbox_));
    shortcut_info_.create_in_applications_menu =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(menu_checkbox_));
    ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
         NewRunnableMethod(this,
             &CreateApplicationShortcutsDialogGtk::CreateDesktopShortcut,
             shortcut_info_));

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

  scoped_ptr<base::EnvVarGetter> env_getter(base::EnvVarGetter::Create());

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
                   G_CALLBACK(OnErrorDialogResponseThunk), this);
  gtk_widget_show_all(error_dialog_);
}

void CreateApplicationShortcutsDialogGtk::OnToggleCheckbox(GtkWidget* sender) {
  gboolean can_accept = FALSE;

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(desktop_checkbox_)) ||
      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(menu_checkbox_))) {
    can_accept = TRUE;
  }

  gtk_dialog_set_response_sensitive(GTK_DIALOG(create_dialog_),
                                    GTK_RESPONSE_ACCEPT,
                                    can_accept);
}
