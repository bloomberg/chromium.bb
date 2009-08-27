// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings_page_view.h"

#include "views/controls/label.h"
#include "views/fill_layout.h"
#include "views/widget/widget_gtk.h"

SettingsPageView::SettingsPageView(Profile* profile)
    : OptionsPageView(profile) {
  SetLayoutManager(new views::FillLayout());
}

GtkWidget* SettingsPageView::WrapInGtkWidget() {
  views::WidgetGtk* widget =
      new views::WidgetGtk(views::WidgetGtk::TYPE_CHILD);
  widget->Init(NULL, gfx::Rect());
  widget->SetContentsView(this);
  widget->Show();
  // Removing the widget from the container results in unref'ing the widget. We
  // need to ref here otherwise the removal deletes the widget. The caller ends
  // up taking ownership.
  g_object_ref(widget->GetNativeView());
  GtkWidget* parent = gtk_widget_get_parent(widget->GetNativeView());
  gtk_container_remove(GTK_CONTAINER(parent), widget->GetNativeView());
  return widget->GetNativeView();
}

void SettingsPageView::InitControlLayout() {
  // Remove this and add the real views we want. We'll likely need to make this
  // scrollable as well.
  AddChildView(new views::Label(L"Implement me"));
}
