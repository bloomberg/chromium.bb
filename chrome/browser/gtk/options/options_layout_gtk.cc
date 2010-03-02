// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/options_layout_gtk.h"

#include "chrome/browser/gtk/gtk_util.h"

OptionsLayoutBuilderGtk::OptionsLayoutBuilderGtk() {
  page_ = gtk_vbox_new(FALSE, gtk_util::kContentAreaSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(page_),
                                 gtk_util::kContentAreaBorder);
}

void OptionsLayoutBuilderGtk::AddOptionGroup(const std::string& title,
                                             GtkWidget* content,
                                             bool expandable) {
  GtkWidget* title_label = gtk_util::CreateBoldLabel(title);

  GtkWidget* group = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(group), title_label, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(group), gtk_util::IndentWidget(content));

  gtk_box_pack_start(GTK_BOX(page_), group, expandable, expandable, 0);
}
