// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/overflow_button.h"

#include <gtk/gtk.h>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_source.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

OverflowButton::OverflowButton(Profile* profile) : profile_(profile) {
  widget_.Own(GtkThemeService::GetFrom(profile)->BuildChromeButton());
  gtk_widget_set_no_show_all(widget_.get(), TRUE);

  GtkThemeService* theme_service = GtkThemeService::GetFrom(profile);
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(theme_service));
  theme_service->InitThemesFor(this);
}

OverflowButton::~OverflowButton() {
  widget_.Destroy();
}

void OverflowButton::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  GtkWidget* former_child = gtk_bin_get_child(GTK_BIN(widget()));
  if (former_child)
    gtk_widget_destroy(former_child);

  GtkWidget* new_child;
  if (GtkThemeService::GetFrom(profile_)->UsingNativeTheme()) {
    new_child = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE);
  } else {
    const gfx::Image& image = ui::ResourceBundle::GetSharedInstance().
        GetNativeImageNamed(IDR_BOOKMARK_BAR_CHEVRONS,
                            ui::ResourceBundle::RTL_ENABLED);
    new_child = gtk_image_new_from_pixbuf(image.ToGdkPixbuf());
  }

  gtk_container_add(GTK_CONTAINER(widget()), new_child);
  gtk_widget_show(new_child);
}
