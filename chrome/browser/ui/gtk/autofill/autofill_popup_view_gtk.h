// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_AUTOFILL_AUTOFILL_POPUP_VIEW_GTK_H_
#define CHROME_BROWSER_UI_GTK_AUTOFILL_AUTOFILL_POPUP_VIEW_GTK_H_
#pragma once

#include <pango/pango.h>

#include "chrome/browser/autofill/autofill_popup_view.h"
#include "ui/base/glib/glib_integers.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/gfx/font.h"

namespace gfx {
class Rect;
}

typedef struct _GdkEventExpose GdkEventExpose;
typedef struct _GdkColor GdkColor;
typedef struct _GtkWidget GtkWidget;

class AutofillPopupViewGtk : public AutofillPopupView {
 public:
  AutofillPopupViewGtk(content::WebContents* web_contents, GtkWidget* parent);
  virtual ~AutofillPopupViewGtk();

  // AutofillPopupView implementations.
  virtual void Hide() OVERRIDE;
  virtual void ShowInternal() OVERRIDE;

 private:
  CHROMEGTK_CALLBACK_1(AutofillPopupViewGtk, gboolean, HandleExpose,
                       GdkEventExpose*);

  // Setup the pango layout to display the autofill results.
  void SetupLayout(const gfx::Rect& window_rect, const GdkColor& text_color);

  GtkWidget* parent_;  // Weak reference.
  GtkWidget* window_;  // Strong reference.
  PangoLayout* layout_;  // Strong reference
  gfx::Font font_;

  // The height of each individual Autofill popup row.
  int row_height_;
};

#endif  // CHROME_BROWSER_UI_GTK_AUTOFILL_AUTOFILL_POPUP_VIEW_GTK_H_
