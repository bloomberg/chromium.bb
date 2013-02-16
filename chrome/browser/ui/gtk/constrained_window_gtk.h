// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_CONSTRAINED_WINDOW_GTK_H_
#define CHROME_BROWSER_UI_GTK_CONSTRAINED_WINDOW_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/web_contents_modal_dialog.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"

typedef struct _GdkColor GdkColor;
class ChromeWebContentsViewDelegateGtk;

namespace content {
class WebContents;
}

class ConstrainedWindowGtkDelegate {
 public:
  // Returns the widget that will be put in the constrained window's container.
  virtual GtkWidget* GetWidgetRoot() = 0;

  // Returns the widget that should get focus when WebContentsModalDialog is
  // focused.
  virtual GtkWidget* GetFocusWidget() = 0;

  // Tells the delegate to either delete itself or set up a task to delete
  // itself later.
  virtual void DeleteDelegate() = 0;

  virtual bool GetBackgroundColor(GdkColor* color);

  // Returns true if hosting ConstrainedWindowGtk should apply default padding.
  virtual bool ShouldHaveBorderPadding() const;

 protected:
  virtual ~ConstrainedWindowGtkDelegate();
};

// WebContentsModalDialog implementation for the GTK port. Unlike the Win32
// system, ConstrainedWindowGtk doesn't draw draggable fake windows and instead
// just centers the dialog. It is thus an order of magnitude simpler.
class ConstrainedWindowGtk : public WebContentsModalDialog {
 public:
  typedef ChromeWebContentsViewDelegateGtk TabContentsViewType;

  ConstrainedWindowGtk(content::WebContents* web_contents,
                       ConstrainedWindowGtkDelegate* delegate);
  virtual ~ConstrainedWindowGtk();

  // Overridden from WebContentsModalDialog:
  virtual void ShowWebContentsModalDialog() OVERRIDE;
  virtual void CloseWebContentsModalDialog() OVERRIDE;
  virtual void FocusWebContentsModalDialog() OVERRIDE;
  virtual void PulseWebContentsModalDialog() OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE;

  // Called when the result of GetBackgroundColor may have changed.
  void BackgroundColorChanged();

  // Returns the WebContents that constrains this Constrained Window.
  content::WebContents* owner() const { return web_contents_; }

  // Returns the toplevel widget that displays this "window".
  GtkWidget* widget() { return border_.get(); }

  // Returns the View that we collaborate with to position ourselves.
  TabContentsViewType* ContainingView();

 private:
  friend class WebContentsModalDialog;

  // Signal callbacks.
  CHROMEGTK_CALLBACK_1(ConstrainedWindowGtk, gboolean, OnKeyPress,
                       GdkEventKey*);
  CHROMEGTK_CALLBACK_1(ConstrainedWindowGtk, void, OnHierarchyChanged,
                       GtkWidget*);

  // The WebContents that owns and constrains this WebContentsModalDialog.
  content::WebContents* web_contents_;

  // The top level widget container that exports to our WebContentsView.
  ui::OwnedWidgetGtk border_;

  // Delegate that provides the contents of this constrained window.
  ConstrainedWindowGtkDelegate* delegate_;

  // Stores if |ShowWebContentsModalDialog()| has been called.
  bool visible_;

  base::WeakPtrFactory<ConstrainedWindowGtk> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_CONSTRAINED_WINDOW_GTK_H_
