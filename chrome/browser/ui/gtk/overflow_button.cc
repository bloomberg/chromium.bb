// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/overflow_button.h"

#include <gtk/gtk.h>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/gtk_theme_provider.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

OverflowButton::OverflowButton(Profile* profile) : profile_(profile) {
  widget_.Own(GtkThemeProvider::GetFrom(profile)->BuildChromeButton());
  gtk_widget_set_no_show_all(widget_.get(), TRUE);

  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
  GtkThemeProvider::GetFrom(profile)->InitThemesFor(this);
}

OverflowButton::~OverflowButton() {
  widget_.Destroy();
}

void OverflowButton::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  GtkWidget* former_child = gtk_bin_get_child(GTK_BIN(widget()));
  if (former_child)
    gtk_widget_destroy(former_child);

  GtkWidget* new_child =
      GtkThemeProvider::GetFrom(profile_)->UseGtkTheme() ?
      gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE) :
      gtk_image_new_from_pixbuf(ResourceBundle::GetSharedInstance().
          GetRTLEnabledPixbufNamed(IDR_BOOKMARK_BAR_CHEVRONS));

  gtk_container_add(GTK_CONTAINER(widget()), new_child);
  gtk_widget_show(new_child);
}
