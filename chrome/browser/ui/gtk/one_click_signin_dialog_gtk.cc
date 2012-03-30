// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/one_click_signin_dialog_gtk.h"

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"

OneClickSigninDialogGtk::OneClickSigninDialogGtk(
    GtkWindow* parent_window,
    const OneClickAcceptCallback& accept_callback)
    : dialog_(NULL),
      use_default_settings_checkbox_(NULL),
      accept_callback_(accept_callback) {
  // Lay out the dialog.

  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_ONE_CLICK_SIGNIN_DIALOG_TITLE).c_str(),
      parent_window,
      GTK_DIALOG_MODAL,
      NULL);

  ignore_result(gtk_dialog_add_button(
      GTK_DIALOG(dialog_),
      l10n_util::GetStringUTF8(IDS_CANCEL).c_str(),
      GTK_RESPONSE_CLOSE));
  GtkWidget* ok_button = gtk_dialog_add_button(
      GTK_DIALOG(dialog_),
      l10n_util::GetStringUTF8(IDS_ONE_CLICK_SIGNIN_DIALOG_OK_BUTTON).c_str(),
      GTK_RESPONSE_ACCEPT);
#if !GTK_CHECK_VERSION(2, 22, 0)
  gtk_dialog_set_has_separator(GTK_DIALOG(dialog_), FALSE);
#endif

  GtkWidget* const content_area =
      gtk_dialog_get_content_area(GTK_DIALOG(dialog_));
  gtk_box_set_spacing(GTK_BOX(content_area), ui::kContentAreaSpacing);

  // Heading.
  GtkWidget* heading_label = gtk_util::CreateBoldLabel(
      l10n_util::GetStringUTF8(IDS_ONE_CLICK_SIGNIN_DIALOG_HEADING).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(heading_label), TRUE);
  gtk_misc_set_alignment(GTK_MISC(heading_label), 0.0, 0.5);
  gtk_box_pack_start(GTK_BOX(content_area), heading_label, FALSE, FALSE, 0);

  // Message.
  GtkWidget* message_label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_ONE_CLICK_SIGNIN_DIALOG_MESSAGE).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(message_label), TRUE);
  gtk_misc_set_alignment(GTK_MISC(message_label), 0.0, 0.5);
  gtk_box_pack_start(GTK_BOX(content_area), message_label, FALSE, FALSE, 0);

  // Checkbox.
  use_default_settings_checkbox_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(
          IDS_ONE_CLICK_SIGNIN_DIALOG_CHECKBOX).c_str());
  gtk_toggle_button_set_active(
      GTK_TOGGLE_BUTTON(use_default_settings_checkbox_), TRUE);
  gtk_box_pack_start(GTK_BOX(content_area),
                     use_default_settings_checkbox_, FALSE, FALSE, 0);

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);
  gtk_window_set_resizable(GTK_WINDOW(dialog_), FALSE);

  gtk_dialog_set_default_response(GTK_DIALOG(dialog_), GTK_RESPONSE_ACCEPT);
  gtk_widget_show_all(dialog_);
  gtk_widget_grab_focus(ok_button);
}

void OneClickSigninDialogGtk::SetUseDefaultSettingsForTest(
    bool use_default_settings) {
  gtk_toggle_button_set_active(
      GTK_TOGGLE_BUTTON(use_default_settings_checkbox_), FALSE);
}

void OneClickSigninDialogGtk::SendResponseForTest(int response_id) {
  OnResponse(dialog_, response_id);
}

OneClickSigninDialogGtk::~OneClickSigninDialogGtk() {}

void OneClickSigninDialogGtk::OnResponse(GtkWidget* dialog, int response_id) {
  if (response_id == GTK_RESPONSE_ACCEPT) {
    const bool use_default_settings =
        gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(use_default_settings_checkbox_));
    accept_callback_.Run(use_default_settings);
  }

  gtk_widget_destroy(dialog_);
  delete this;
}

void ShowOneClickSigninDialog(
    gfx::NativeWindow parent_window,
    const OneClickAcceptCallback& accept_callback) {
  ignore_result(
      new OneClickSigninDialogGtk(parent_window, accept_callback));
}
