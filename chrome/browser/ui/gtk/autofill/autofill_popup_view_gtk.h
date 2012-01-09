// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_AUTOFILL_AUTOFILL_POPUP_VIEW_GTK_H_
#define CHROME_BROWSER_UI_GTK_AUTOFILL_AUTOFILL_POPUP_VIEW_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "base/string16.h"
#include "chrome/browser/autofill/autofill_popup_view.h"
#include "ui/base/gtk/gtk_signal.h"

#include <vector>

class AutofillPopupViewGtk : public AutofillPopupView {
 public:
  AutofillPopupViewGtk(content::WebContents* web_contents, GtkWidget* parent);
  virtual ~AutofillPopupViewGtk();

  // AutofillPopupView implementations.
  virtual void Hide() OVERRIDE;
  virtual void Show(const std::vector<string16>& autofill_values,
                    const std::vector<string16>& autofill_labels,
                    const std::vector<string16>& autofill_icons,
                    const std::vector<int>& autofill_unique_ids,
                    int separator_index) OVERRIDE;

 private:
  CHROMEGTK_CALLBACK_1(AutofillPopupViewGtk, gboolean, HandleExpose,
                       GdkEventExpose*);

  GtkWidget* parent_;  // Weak reference.
  GtkWidget* window_;  // Strong refence.
};

#endif  // CHROME_BROWSER_UI_GTK_AUTOFILL_AUTOFILL_POPUP_VIEW_GTK_H_
