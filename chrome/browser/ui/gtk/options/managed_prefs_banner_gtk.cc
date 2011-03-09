// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/options/managed_prefs_banner_gtk.h"

#include "chrome/browser/ui/gtk/gtk_theme_provider.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Padding within the banner box.
const int kBannerPadding = 3;

}

ManagedPrefsBannerGtk::ManagedPrefsBannerGtk(PrefService* prefs,
                                             OptionsPage page)
    : policy::ManagedPrefsBannerBase(prefs, page),
      banner_widget_(NULL) {
  InitWidget();
  OnUpdateVisibility();
}

void ManagedPrefsBannerGtk::InitWidget() {
  banner_widget_ = gtk_frame_new(NULL);
  GtkWidget* contents = gtk_hbox_new(FALSE, kBannerPadding);
  gtk_container_set_border_width(GTK_CONTAINER(contents), kBannerPadding);
  gtk_container_add(GTK_CONTAINER(banner_widget_), contents);
  GtkWidget* warning_image =
      gtk_image_new_from_stock(GTK_STOCK_DIALOG_WARNING,
                               GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_box_pack_start(GTK_BOX(contents), warning_image, FALSE, FALSE, 0);
  std::string info_text(l10n_util::GetStringUTF8(IDS_OPTIONS_MANAGED_PREFS));
  GtkWidget* info_label = gtk_label_new(info_text.c_str());
  gtk_box_pack_start(GTK_BOX(contents), info_label, FALSE, FALSE, 0);
  gtk_widget_show_all(banner_widget_);
  gtk_widget_set_no_show_all(GTK_WIDGET(banner_widget_), TRUE);
}

void ManagedPrefsBannerGtk::OnUpdateVisibility() {
  DCHECK(banner_widget_);
  if (DetermineVisibility())
    gtk_widget_show(banner_widget_);
  else
    gtk_widget_hide(banner_widget_);
}
