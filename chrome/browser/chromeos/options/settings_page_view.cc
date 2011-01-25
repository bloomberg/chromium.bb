// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/settings_page_view.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "gfx/skia_utils_gtk.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/label.h"
#include "views/layout/fill_layout.h"
#include "views/widget/widget_gtk.h"

namespace chromeos {

SettingsPageView::SettingsPageView(Profile* profile)
    : OptionsPageView(profile) {
  SetLayoutManager(new views::FillLayout());
}

GtkWidget* SettingsPageView::WrapInGtkWidget() {
  views::WidgetGtk* widget =
      new views::WidgetGtk(views::WidgetGtk::TYPE_CHILD);
  widget->Init(NULL, gfx::Rect());
  widget->SetContentsView(this);
  // Set to a solid background with the same color as the widget's bg color.
  GtkStyle* window_style = gtk_widget_get_style(widget->GetNativeView());
  set_background(views::Background::CreateSolidBackground(
      gfx::GdkColorToSkColor(window_style->bg[GTK_STATE_NORMAL])));
  widget->Show();
  // Removing the widget from the container results in unref'ing the widget. We
  // need to ref here otherwise the removal deletes the widget. The caller ends
  // up taking ownership.
  g_object_ref(widget->GetNativeView());
  GtkWidget* parent = gtk_widget_get_parent(widget->GetNativeView());
  gtk_container_remove(GTK_CONTAINER(parent), widget->GetNativeView());
  return widget->GetNativeView();
}


////////////////////////////////////////////////////////////////////////////////
// SettingsPageSection

SettingsPageSection::SettingsPageSection(Profile* profile, int title_msg_id)
    : OptionsPageView(profile),
      title_msg_id_(title_msg_id),
      // Using 1000 so that it does not clash with ids defined in subclasses.
      single_column_view_set_id_(1000),
      double_column_view_set_id_(1001) {
}

void SettingsPageSection::InitControlLayout() {
  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  int single_column_layout_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_layout_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);
  int inset_column_layout_id = 1;
  column_set = layout->AddColumnSet(inset_column_layout_id);
  column_set->AddPaddingColumn(0, kUnrelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::LEADING, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_layout_id);
  views::Label* title_label = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(title_msg_id_)));
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font title_font =
      rb.GetFont(ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD);
  title_label->SetFont(title_font);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, inset_column_layout_id);

  views::View* contents = new views::View;
  GridLayout* child_layout = new GridLayout(contents);
  contents->SetLayoutManager(child_layout);

  column_set = child_layout->AddColumnSet(single_column_view_set_id_);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  column_set = child_layout->AddColumnSet(double_column_view_set_id_);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  InitContents(child_layout);
  layout->AddView(contents);
}

}  // namespace chromeos
