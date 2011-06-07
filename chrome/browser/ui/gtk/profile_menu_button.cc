// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/profile_menu_button.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/gtk_chrome_button.h"
#include "chrome/browser/ui/profile_menu_model.h"
#include "chrome/common/pref_names.h"
#include "ui/base/text/text_elider.h"

// Maximum width for name string in pixels.
const int kMaxTextWidth = 200;

ProfileMenuButton::ProfileMenuButton() {
  profile_menu_model_.reset(new ProfileMenuModel);
  menu_.reset(new MenuGtk(NULL, profile_menu_model_.get()));

  widget_.Own(gtk_button_new());

  g_signal_connect(widget_.get(), "button-press-event",
                   G_CALLBACK(OnButtonPressedThunk), this);
  gtk_widget_set_no_show_all(GTK_WIDGET(widget_.get()), TRUE);
}

ProfileMenuButton::~ProfileMenuButton() {}

void ProfileMenuButton::UpdateText(Profile* profile) {
  string16 text = UTF8ToUTF16(
      profile->GetPrefs()->GetString(prefs::kGoogleServicesUsername));
  string16 elided_text = ui::ElideText(text, gfx::Font(), kMaxTextWidth, false);
  gtk_button_set_label(
      GTK_BUTTON(widget_.get()), UTF16ToUTF8(elided_text).c_str());

  if (!text.empty())
    gtk_widget_show(widget_.get());
  else
    gtk_widget_hide(widget_.get());
}

gboolean ProfileMenuButton::OnButtonPressed(GtkWidget* widget,
                                            GdkEventButton* event) {
  if (event->button != 1)
    return FALSE;

  menu_->PopupForWidget(widget_.get(), event->button, event->time);
  return TRUE;
}
