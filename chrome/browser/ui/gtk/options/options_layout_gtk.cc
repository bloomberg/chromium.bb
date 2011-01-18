// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/options/options_layout_gtk.h"

#include "chrome/browser/ui/gtk/gtk_util.h"

// If the height of screen is equal or shorter than this, we will use
// a more compact option layout.
const int kCompactScreenHeight = 600;

// Default option layout builder follows GNOME HIG, which uses header and
// spacing to group options.
class DefaultOptionsLayoutBuilderGtk : public OptionsLayoutBuilderGtk {
 public:
  explicit DefaultOptionsLayoutBuilderGtk();

  void AddOptionGroup(const std::string& title, GtkWidget* content,
                      bool expandable);
  void AddWidget(GtkWidget* content, bool expandable);
 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultOptionsLayoutBuilderGtk);
};

DefaultOptionsLayoutBuilderGtk::DefaultOptionsLayoutBuilderGtk() {
  page_ = gtk_vbox_new(FALSE, gtk_util::kContentAreaSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(page_),
                                 gtk_util::kContentAreaBorder);
}

void DefaultOptionsLayoutBuilderGtk::AddOptionGroup(const std::string& title,
                                                    GtkWidget* content,
                                                    bool expandable) {
  GtkWidget* title_label = gtk_util::CreateBoldLabel(title);

  GtkWidget* group = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(group), title_label, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(group), gtk_util::IndentWidget(content));

  AddWidget(group, expandable);
}

void DefaultOptionsLayoutBuilderGtk::AddWidget(GtkWidget* content,
                                               bool expandable) {
  gtk_box_pack_start(GTK_BOX(page_), content, expandable, expandable, 0);
}

// Compact layout builder uses table to layout label and content horizontally.
class CompactOptionsLayoutBuilderGtk : public OptionsLayoutBuilderGtk {
 public:
  explicit CompactOptionsLayoutBuilderGtk();

  void AddOptionGroup(const std::string& title, GtkWidget* content,
                      bool expandable);
  void AddWidget(GtkWidget* content, bool expandable);
 private:
  GtkWidget *table_;
  guint row_;

  DISALLOW_COPY_AND_ASSIGN(CompactOptionsLayoutBuilderGtk);
};

CompactOptionsLayoutBuilderGtk::CompactOptionsLayoutBuilderGtk() {
  row_ = 0;
  table_ = NULL;

  page_ = gtk_vbox_new(FALSE, gtk_util::kContentAreaSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(page_),
                                 gtk_util::kContentAreaBorder);
}

void CompactOptionsLayoutBuilderGtk::AddOptionGroup(const std::string& title,
                                             GtkWidget* content,
                                             bool expandable) {
  if (!table_) {
    // Create a new table to contain option groups
    table_ = gtk_table_new(0, 2, FALSE);
    gtk_table_set_col_spacing(GTK_TABLE(table_), 0, gtk_util::kLabelSpacing);
    gtk_table_set_row_spacings(GTK_TABLE(table_),
                               gtk_util::kContentAreaSpacing);

    gtk_container_set_border_width(GTK_CONTAINER(table_),
                                   gtk_util::kContentAreaBorder);
    gtk_box_pack_start(GTK_BOX(page_), table_, TRUE, TRUE, 0);
  }

  GtkWidget* title_label = gtk_util::CreateBoldLabel(title);

  gtk_table_resize(GTK_TABLE(table_), row_ + 1, 2);
  gtk_misc_set_alignment(GTK_MISC(title_label), 1, 0);

  gtk_table_attach(GTK_TABLE(table_), title_label,
                   0, 1, row_, row_ + 1,
                   GTK_FILL, GTK_FILL,
                   0, 0);
  gtk_table_attach(GTK_TABLE(table_), content,
                   1, 2, row_, row_ + 1,
                   expandable ?
                   GTK_FILL : GtkAttachOptions(GTK_FILL | GTK_EXPAND),
                   GTK_FILL,
                   0, 0);
  row_++;
}

void CompactOptionsLayoutBuilderGtk::AddWidget(GtkWidget* content,
                                               bool expandable) {
  gtk_box_pack_start(GTK_BOX(page_), content, expandable, expandable, 0);

  // Let AddOptionGroup create a new table and append after this widget
  table_ = NULL;
}

OptionsLayoutBuilderGtk* OptionsLayoutBuilderGtk::Create() {
    return new DefaultOptionsLayoutBuilderGtk();
}

OptionsLayoutBuilderGtk*
OptionsLayoutBuilderGtk::CreateOptionallyCompactLayout() {
  gint screen_height = gdk_screen_get_height(gdk_screen_get_default());
  if (screen_height <= kCompactScreenHeight)
    return new CompactOptionsLayoutBuilderGtk();
  else
    return new DefaultOptionsLayoutBuilderGtk();
}
