// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/options_layout_gtk.h"

#include "chrome/common/gtk_util.h"

namespace {

// Style for option group titles
const char kOptionGroupTitleMarkup[] = "<span weight='bold'>%s</span>";

}  // namespace

OptionsLayoutBuilderGtk::OptionsLayoutBuilderGtk() {
  page_ = gtk_vbox_new(FALSE, gtk_util::kContentAreaSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(page_),
                                 gtk_util::kContentAreaBorder);
}

void OptionsLayoutBuilderGtk::AddOptionGroup(const std::string& title,
                                             GtkWidget* content,
                                             bool expandable) {
  GtkWidget* title_label = gtk_label_new(NULL);
  char* markup = g_markup_printf_escaped(kOptionGroupTitleMarkup,
                                         title.c_str());
  gtk_label_set_markup(GTK_LABEL(title_label), markup);
  g_free(markup);

  GtkWidget* title_alignment = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
  gtk_container_add(GTK_CONTAINER(title_alignment), title_label);

  GtkWidget* group = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(group), title_alignment, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(group), gtk_util::IndentWidget(content));

  gtk_box_pack_start(GTK_BOX(page_), group, expandable, expandable, 0);
}
