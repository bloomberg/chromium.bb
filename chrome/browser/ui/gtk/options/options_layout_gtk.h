// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_OPTIONS_OPTIONS_LAYOUT_GTK_H_
#define CHROME_BROWSER_UI_GTK_OPTIONS_OPTIONS_LAYOUT_GTK_H_
#pragma once

#include <gtk/gtk.h>
#include <string>

#include "base/basictypes.h"

class OptionsLayoutBuilderGtk {
 public:
  virtual ~OptionsLayoutBuilderGtk() {}

  GtkWidget* get_page_widget() {
    return page_;
  }

  // Adds an option group to the table.  Handles layout and the placing of
  // separators between groups.  If expandable is true, the content widget will
  // be allowed to expand and fill any extra space when the dialog is resized.
  virtual void AddOptionGroup(const std::string& title, GtkWidget* content,
                              bool expandable) = 0;

  // Adds a widget without title or special layout.  If expandable is true, the
  // content widget will be allowed to expand and fill any extra space when the
  // dialog is resized.
  virtual void AddWidget(GtkWidget* content, bool expandable) = 0;

  // Creates a default option layout builder. The default layout builder
  // follows the GNOME HIG.
  static OptionsLayoutBuilderGtk* Create();

  // Creates a compact option layout builder, if the screen is compact.
  // Otherwise, creates a default one.
  static OptionsLayoutBuilderGtk* CreateOptionallyCompactLayout();

 protected:
  // The parent widget
  GtkWidget* page_;
};

#endif  // CHROME_BROWSER_UI_GTK_OPTIONS_OPTIONS_LAYOUT_GTK_H_
