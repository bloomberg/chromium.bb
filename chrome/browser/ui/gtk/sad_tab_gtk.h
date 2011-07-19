// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_SAD_TAB_GTK_H_
#define CHROME_BROWSER_UI_GTK_SAD_TAB_GTK_H_
#pragma once

typedef struct _GtkWidget GtkWidget;

#include "chrome/browser/ui/gtk/owned_widget_gtk.h"
#include "ui/base/gtk/gtk_signal.h"

class TabContents;

class SadTabGtk {
 public:
  enum Kind {
    CRASHED,  // The tab crashed.  Display the "Aw, Snap!" page.
    KILLED    // The tab was killed.  Display the killed tab page.
  };

  explicit SadTabGtk(TabContents* tab_contents, Kind kind);
  virtual ~SadTabGtk();

  GtkWidget* widget() const { return event_box_.get(); }

 private:
  CHROMEGTK_CALLBACK_0(SadTabGtk, void, OnLinkButtonClick);

  void OnLinkButtonClick();

  TabContents* tab_contents_;
  OwnedWidgetGtk event_box_;
  Kind kind_;

  DISALLOW_COPY_AND_ASSIGN(SadTabGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_SAD_TAB_GTK_H_
