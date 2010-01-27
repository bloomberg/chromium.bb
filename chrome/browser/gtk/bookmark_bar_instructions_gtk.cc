// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BOOKMARK_BAR_INSTRUCTIONS_GTK_CC_
#define CHROME_BROWSER_GTK_BOOKMARK_BAR_INSTRUCTIONS_GTK_CC_

#include "chrome/browser/gtk/bookmark_bar_instructions_gtk.h"

#include "app/l10n_util.h"
#include "base/observer_list.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"

BookmarkBarInstructionsGtk::BookmarkBarInstructionsGtk(Delegate* delegate,
                                                       Profile* profile)
    : delegate_(delegate),
      profile_(profile) {
  instructions_hbox_ = gtk_hbox_new(FALSE, 0);

  instructions_label_ =
      gtk_label_new(l10n_util::GetStringUTF8(IDS_BOOKMARKS_NO_ITEMS).c_str());
  instructions_label_ = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_BOOKMARKS_NO_ITEMS).c_str());
  gtk_util::CenterWidgetInHBox(instructions_hbox_, instructions_label_,
                               false, 1);

  instructions_link_ = gtk_chrome_link_button_new(
      l10n_util::GetStringUTF8(IDS_BOOKMARK_BAR_IMPORT_LINK).c_str());
  g_signal_connect(instructions_link_, "clicked",
                   G_CALLBACK(OnButtonClick), this);
  gtk_util::SetButtonTriggersNavigation(instructions_link_);
  // Until we switch to vector graphics, force the font size.
  // 13.4px == 10pt @ 96dpi
  gtk_util::ForceFontSizePixels(
      GTK_CHROME_LINK_BUTTON(instructions_link_)->label, 13.4);
  gtk_util::CenterWidgetInHBox(instructions_hbox_, instructions_link_,
                               false, 1);

  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
}

void BookmarkBarInstructionsGtk::Observe(NotificationType type,
                                         const NotificationSource& source,
                                         const NotificationDetails& details) {
  if (type == NotificationType::BROWSER_THEME_CHANGED)
    UpdateColors();
}

// static
void BookmarkBarInstructionsGtk::OnButtonClick(
    GtkWidget* button, BookmarkBarInstructionsGtk* instructions) {
  instructions->delegate_->ShowImportDialog();
}

void BookmarkBarInstructionsGtk::UpdateColors() {
  const GtkThemeProvider* theme_provider = GtkThemeProvider::GetFrom(profile_);
  if (!theme_provider)
    return;

  gtk_chrome_link_button_set_use_gtk_theme(
      GTK_CHROME_LINK_BUTTON(instructions_link_),
      theme_provider->UseGtkTheme());

  // When using a non-standard, non-gtk theme, we make the link color match
  // the bookmark text color. Otherwise, standard link blue can look very
  // bad for some dark themes.
  if (theme_provider->GetColor(BrowserThemeProvider::COLOR_BOOKMARK_TEXT) ==
      BrowserThemeProvider::GetDefaultColor(
          BrowserThemeProvider::COLOR_BOOKMARK_TEXT)) {
    gtk_util::SetLabelColor(instructions_label_, NULL);
    gtk_chrome_link_button_set_normal_color(
        GTK_CHROME_LINK_BUTTON(instructions_link_), NULL);
  } else {
    GdkColor bookmark_color = theme_provider->GetGdkColor(
        BrowserThemeProvider::COLOR_BOOKMARK_TEXT);
    gtk_util::SetLabelColor(instructions_label_, &bookmark_color);
    gtk_chrome_link_button_set_normal_color(
        GTK_CHROME_LINK_BUTTON(instructions_link_), &bookmark_color);
  }
}

#endif  // CHROME_BROWSER_GTK_BOOKMARK_BAR_INSTRUCTIONS_GTK_CC_
