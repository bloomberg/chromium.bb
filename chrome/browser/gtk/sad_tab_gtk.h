// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_SAD_TAB_GTK_H_
#define CHROME_BROWSER_GTK_SAD_TAB_GTK_H_

typedef struct _GtkWidget GtkWidget;

#include "app/gtk_signal.h"
#include "chrome/common/owned_widget_gtk.h"

class TabContents;

class SadTabGtk {
 public:
  explicit SadTabGtk(TabContents* tab_contents);
  ~SadTabGtk();

  GtkWidget* widget() const { return event_box_.get(); }

 private:
  CHROMEGTK_CALLBACK_0(SadTabGtk, void, OnLinkButtonClick);

  void OnLinkButtonClick();

  TabContents* tab_contents_;
  OwnedWidgetGtk event_box_;

  DISALLOW_COPY_AND_ASSIGN(SadTabGtk);
};

#endif  // CHROME_BROWSER_GTK_SAD_TAB_GTK_H_
