// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_CONSTRAINED_WINDOW_GTK_H_
#define CHROME_BROWSER_UI_GTK_CONSTRAINED_WINDOW_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/native_web_contents_modal_dialog.h"
#include "ui/base/gtk/gtk_signal.h"

typedef struct _GdkColor GdkColor;
class ChromeWebContentsViewDelegateGtk;

// Web contents modal dialog implementation for the GTK port. Unlike the Win32
// system, ConstrainedWindowGtk doesn't draw draggable fake windows and instead
// just centers the dialog. It is thus an order of magnitude simpler.
class ConstrainedWindowGtk {
 public:
  typedef ChromeWebContentsViewDelegateGtk TabContentsViewType;

  ConstrainedWindowGtk(GtkWidget* contents,
                       GtkWidget* focus_widget);
  virtual ~ConstrainedWindowGtk();

  // Returns the toplevel widget that displays this "window".
  GtkWidget* widget() { return border_; }

 private:
  // The top level widget container that exports to our WebContentsView.
  GtkWidget* border_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowGtk);
};

// Returns a floating reference.
GtkWidget* CreateWebContentsModalDialogGtk(
    GtkWidget* contents,
    GtkWidget* focus_widget);

#endif  // CHROME_BROWSER_UI_GTK_CONSTRAINED_WINDOW_GTK_H_
