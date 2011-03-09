// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/options/advanced_page_gtk.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/options/options_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

AdvancedPageGtk::AdvancedPageGtk(Profile* profile)
    : OptionsPageBase(profile),
      advanced_contents_(profile),
      managed_prefs_banner_(profile->GetPrefs(), OPTIONS_PAGE_ADVANCED) {
  Init();
}

AdvancedPageGtk::~AdvancedPageGtk() {
}

void AdvancedPageGtk::Init() {
  page_ = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(page_),
                                 gtk_util::kContentAreaBorder);

  gtk_box_pack_start(GTK_BOX(page_), managed_prefs_banner_.banner_widget(),
                     false, false, 0);

  GtkWidget* scroll_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(page_), scroll_window);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                 GTK_POLICY_NEVER,
                                 GTK_POLICY_AUTOMATIC);
  // Note that typically we call gtk_scrolled_window_set_shadow_type right
  // here, but the add_with_viewport method of GtkScrolledWindow already adds
  // its own shadow.
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll_window),
                                        advanced_contents_.get_page_widget());

  GtkWidget* button_box = gtk_hbutton_box_new();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_END);
  GtkWidget* reset_button = gtk_button_new_with_label(
        l10n_util::GetStringUTF8(IDS_OPTIONS_RESET).c_str());
  g_signal_connect(reset_button, "clicked",
                   G_CALLBACK(OnResetToDefaultsClickedThunk), this);
  gtk_container_add(GTK_CONTAINER(button_box), reset_button);
  gtk_box_pack_start(GTK_BOX(page_), button_box, FALSE, FALSE, 0);
}

void AdvancedPageGtk::OnResetToDefaultsClicked(GtkWidget* button) {
  UserMetricsRecordAction(UserMetricsAction("Options_ResetToDefaults"), NULL);
  GtkWidget* dialog_ = gtk_message_dialog_new(
      GTK_WINDOW(gtk_widget_get_toplevel(page_)),
      static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL),
      GTK_MESSAGE_QUESTION,
      GTK_BUTTONS_NONE,
      "%s",
      l10n_util::GetStringUTF8(IDS_OPTIONS_RESET_MESSAGE).c_str());
  gtk_util::ApplyMessageDialogQuirks(dialog_);
  gtk_dialog_add_buttons(
      GTK_DIALOG(dialog_),
      l10n_util::GetStringUTF8(IDS_OPTIONS_RESET_CANCELLABEL).c_str(),
      GTK_RESPONSE_CANCEL,
      l10n_util::GetStringUTF8(IDS_OPTIONS_RESET_OKLABEL).c_str(),
      GTK_RESPONSE_OK,
      NULL);
  gtk_window_set_title(GTK_WINDOW(dialog_),
      l10n_util::GetStringUTF8(IDS_PRODUCT_NAME).c_str());
  g_signal_connect(dialog_, "response",
                   G_CALLBACK(OnResetToDefaultsResponseThunk), this);

  gtk_util::ShowDialog(dialog_);
}

void AdvancedPageGtk::OnResetToDefaultsResponse(GtkWidget* dialog,
                                                int response_id) {
  if (response_id == GTK_RESPONSE_OK) {
    OptionsUtil::ResetToDefaults(profile());
  }
  gtk_widget_destroy(dialog);
}
