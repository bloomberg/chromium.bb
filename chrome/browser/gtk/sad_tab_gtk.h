// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_SAD_TAB_GTK_H_
#define CHROME_BROWSER_GTK_SAD_TAB_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "chrome/common/owned_widget_gtk.h"

class SadTabGtk {
 public:
  SadTabGtk();
  ~SadTabGtk();

  GtkWidget* widget() { return event_box_.get(); }

 private:
  // expose-event handler that draws the gradient background of the SadTabGtk.
  static gboolean OnBackgroundExposeThunk(GtkWidget* widget,
                                          GdkEventExpose* event,
                                          SadTabGtk* sad_tab) {
    return sad_tab->OnBackgroundExpose(widget, event);
  }

  gboolean OnBackgroundExpose(GtkWidget* widget, GdkEventExpose* event);

  // size-allocate handler to adjust dimensions for children widgets.
  static void OnSizeAllocateThunk(GtkWidget* widget,
                                      GtkAllocation* allocation,
                                      SadTabGtk* sad_tab) {
    sad_tab->OnSizeAllocate(widget, allocation);
  }

  void OnSizeAllocate(GtkWidget* widget, GtkAllocation* allocation);

  // click-event handler that opens the url of the clicked link.
  static void OnLinkButtonClick(GtkWidget* button, const char* url);

  // Track the view's width and height from size-allocate signals.
  int width_;
  int height_;

  OwnedWidgetGtk event_box_;
  GtkWidget* top_padding_;
  GtkWidget* message_;

  DISALLOW_COPY_AND_ASSIGN(SadTabGtk);
};

#endif  // CHROME_BROWSER_GTK_SAD_TAB_GTK_H__
