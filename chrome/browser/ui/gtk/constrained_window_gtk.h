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

class ConstrainedWindowGtkDelegate {
 public:
  // Returns the widget that will be put in the constrained window's container.
  virtual GtkWidget* GetWidgetRoot() = 0;

  // Returns the widget that should get focus when ConstrainedWindowGtk is
  // focused.
  virtual GtkWidget* GetFocusWidget() = 0;

  // Tells the delegate to either delete itself or set up a task to delete
  // itself later.
  virtual void DeleteDelegate() = 0;

  virtual bool GetBackgroundColor(GdkColor* color);

 protected:
  virtual ~ConstrainedWindowGtkDelegate();
};

// Web contents modal dialog implementation for the GTK port. Unlike the Win32
// system, ConstrainedWindowGtk doesn't draw draggable fake windows and instead
// just centers the dialog. It is thus an order of magnitude simpler.
class ConstrainedWindowGtk {
 public:
  typedef ChromeWebContentsViewDelegateGtk TabContentsViewType;

  ConstrainedWindowGtk(content::WebContents* web_contents,
                       ConstrainedWindowGtkDelegate* delegate);
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

  // Delegate that provides the contents of this constrained window.
  ConstrainedWindowGtkDelegate* delegate_;

  // Stores if |ShowWebContentsModalDialog()| has been called.
  bool visible_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowGtk);
};

GtkWidget* CreateWebContentsModalDialogGtk(
    content::WebContents* web_contents,
    ConstrainedWindowGtkDelegate* delegate);

#endif  // CHROME_BROWSER_UI_GTK_CONSTRAINED_WINDOW_GTK_H_
