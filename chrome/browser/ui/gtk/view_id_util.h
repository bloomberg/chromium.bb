// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_VIEW_ID_UTIL_H_
#define CHROME_BROWSER_UI_GTK_VIEW_ID_UTIL_H_
#pragma once

#include "chrome/browser/ui/view_ids.h"

typedef struct _GtkWidget GtkWidget;

class ViewIDUtil {
 public:
  // Use this delegate to override default view id searches.
  class Delegate {
   public:
    virtual GtkWidget* GetWidgetForViewID(ViewID id) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // If you set the ID via this function, it will also set the name of your
  // widget to a human-readable value for debugging.
  static void SetID(GtkWidget* widget, ViewID id);

  static GtkWidget* GetWidget(GtkWidget* root, ViewID id);

  static void SetDelegateForWidget(GtkWidget* widget, Delegate* delegate);
};

#endif  // CHROME_BROWSER_UI_GTK_VIEW_ID_UTIL_H_
