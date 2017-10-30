// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTKUI_NAV_BUTTON_LAYOUT_MANAGER_GTK3_H_
#define CHROME_BROWSER_UI_LIBGTKUI_NAV_BUTTON_LAYOUT_MANAGER_GTK3_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/libgtkui/nav_button_layout_manager.h"
#include "ui/base/glib/glib_signal.h"

typedef struct _GParamSpec GParamSpec;
typedef struct _GtkSettings GtkSettings;

namespace libgtkui {

class GtkUi;

class NavButtonLayoutManagerGtk3 : public NavButtonLayoutManager {
 public:
  explicit NavButtonLayoutManagerGtk3(GtkUi* delegate);
  ~NavButtonLayoutManagerGtk3() override;

 private:
  void SetWindowButtonOrderingFromGtkLayout(const std::string& gtk_layout);

  CHROMEG_CALLBACK_1(NavButtonLayoutManagerGtk3,
                     void,
                     OnDecorationButtonLayoutChanged,
                     GtkSettings*,
                     GParamSpec*);

  CHROMEG_CALLBACK_1(NavButtonLayoutManagerGtk3,
                     void,
                     OnMiddleClickActionChanged,
                     GtkSettings*,
                     GParamSpec*);

  CHROMEG_CALLBACK_1(NavButtonLayoutManagerGtk3,
                     void,
                     OnThemeChanged,
                     GtkSettings*,
                     GParamSpec*);

  GtkUi* delegate_;

  unsigned long signal_id_;

  unsigned long signal_id_middle_click_;

  DISALLOW_COPY_AND_ASSIGN(NavButtonLayoutManagerGtk3);
};

}  // namespace libgtkui

#endif  // CHROME_BROWSER_UI_LIBGTKUI_NAV_BUTTON_LAYOUT_MANAGER_GTK3_H_
