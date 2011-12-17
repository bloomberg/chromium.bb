// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_REPOST_FORM_WARNING_GTK_H_
#define CHROME_BROWSER_UI_GTK_REPOST_FORM_WARNING_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "ui/base/gtk/gtk_signal.h"

class RepostFormWarningController;
class TabContents;

// Displays a dialog that warns the user that they are about to resubmit
// a form.
// To display the dialog, allocate this object on the heap. It will open the
// dialog from its constructor and then delete itself when the user dismisses
// the dialog.
class RepostFormWarningGtk : public ConstrainedWindowGtkDelegate {
 public:
  RepostFormWarningGtk(GtkWindow* parent, TabContents* tab_contents);

  // ConstrainedWindowGtkDelegate methods
  virtual GtkWidget* GetWidgetRoot() OVERRIDE;
  virtual GtkWidget* GetFocusWidget() OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;

 private:
  virtual ~RepostFormWarningGtk();

  // Callbacks
  CHROMEGTK_CALLBACK_0(RepostFormWarningGtk, void, OnRefresh);
  CHROMEGTK_CALLBACK_0(RepostFormWarningGtk, void, OnCancel);

  scoped_ptr<RepostFormWarningController> controller_;

  GtkWidget* dialog_;
  GtkWidget* ok_;
  GtkWidget* cancel_;

  DISALLOW_COPY_AND_ASSIGN(RepostFormWarningGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_REPOST_FORM_WARNING_GTK_H_
