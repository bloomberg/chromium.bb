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

namespace content {
class WebContents;
}

// Web contents modal dialog implementation for the GTK port. Unlike the Win32
// system, ConstrainedWindowGtk doesn't draw draggable fake windows and instead
// just centers the dialog. It is thus an order of magnitude simpler.
class ConstrainedWindowGtk {
 public:
  typedef ChromeWebContentsViewDelegateGtk TabContentsViewType;

  ConstrainedWindowGtk(content::WebContents* web_contents,
                       GtkWidget* contents,
                       GtkWidget* focus_widget);
  virtual ~ConstrainedWindowGtk();

  void ShowWebContentsModalDialog();
  void FocusWebContentsModalDialog();
  void PulseWebContentsModalDialog();
  NativeWebContentsModalDialog GetNativeDialog();

  // Returns the toplevel widget that displays this "window".
  GtkWidget* widget() { return border_; }

 private:
  // Returns the View that we collaborate with to position ourselves.
  TabContentsViewType* ContainingView();

  // Signal callbacks.
  CHROMEGTK_CALLBACK_1(ConstrainedWindowGtk, gboolean, OnKeyPress,
                       GdkEventKey*);
  CHROMEGTK_CALLBACK_1(ConstrainedWindowGtk, void, OnHierarchyChanged,
                       GtkWidget*);
  CHROMEGTK_CALLBACK_0(ConstrainedWindowGtk, void, OnDestroy);

  // The WebContents that owns and constrains this ConstrainedWindowGtk.
  content::WebContents* web_contents_;

  // The top level widget container that exports to our WebContentsView.
  GtkWidget* border_;

  // The widget that should get focus when ConstrainedWindowGtk is focused.
  GtkWidget* focus_widget_;

  // Stores if |ShowWebContentsModalDialog()| has been called.
  bool visible_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowGtk);
};

GtkWidget* CreateWebContentsModalDialogGtk(
    content::WebContents* web_contents,
    GtkWidget* contents,
    GtkWidget* focus_widget);

#endif  // CHROME_BROWSER_UI_GTK_CONSTRAINED_WINDOW_GTK_H_
