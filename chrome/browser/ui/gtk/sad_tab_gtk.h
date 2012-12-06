// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_SAD_TAB_GTK_H_
#define CHROME_BROWSER_UI_GTK_SAD_TAB_GTK_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/sad_tab.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"

typedef struct _GtkWidget GtkWidget;

class SadTabGtk : public chrome::SadTab {
 public:
  SadTabGtk(content::WebContents* web_contents, chrome::SadTabKind kind);
  virtual ~SadTabGtk();

 private:
  // Overridden from SadTab:
  virtual void Show() OVERRIDE;
  virtual void Close() OVERRIDE;

  CHROMEGTK_CALLBACK_0(SadTabGtk, void, OnLinkButtonClick);

  void OnLinkButtonClick();

  content::WebContents* web_contents_;
  ui::OwnedWidgetGtk event_box_;
  chrome::SadTabKind kind_;

  DISALLOW_COPY_AND_ASSIGN(SadTabGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_SAD_TAB_GTK_H_
