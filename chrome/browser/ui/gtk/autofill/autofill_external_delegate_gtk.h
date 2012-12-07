// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_AUTOFILL_AUTOFILL_EXTERNAL_DELEGATE_GTK_H_
#define CHROME_BROWSER_UI_GTK_AUTOFILL_AUTOFILL_EXTERNAL_DELEGATE_GTK_H_

#include <gtk/gtk.h>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autofill/autofill_external_delegate.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/gfx/native_widget_types.h"

class AutofillPopupViewGtk;

class AutofillExternalDelegateGtk : public AutofillExternalDelegate {
 public:
  AutofillExternalDelegateGtk(content::WebContents* web_contents,
                              AutofillManager* autofill_manager);

  virtual ~AutofillExternalDelegateGtk();

 protected:
  // AutofillExternalDelegate implementations.
  virtual void HideAutofillPopupInternal() OVERRIDE;
  virtual void ApplyAutofillSuggestions(
      const std::vector<string16>& autofill_values,
      const std::vector<string16>& autofill_labels,
      const std::vector<string16>& autofill_icons,
      const std::vector<int>& autofill_unique_ids) OVERRIDE;
  virtual void CreatePopupForElement(const gfx::Rect& element_bounds) OVERRIDE;

 private:
  // Create a valid view to display the autofill results if one doesn't
  // currently exist.
  void CreateViewIfNeeded();

  CHROMEGTK_CALLBACK_1(AutofillExternalDelegateGtk, gboolean,
                       HandleViewFocusOut, GdkEventFocus*);

  // Unlike in the Views implementation, the popup is always destroyed when
  // this class is.
  scoped_ptr<AutofillPopupViewGtk> view_;

  gfx::NativeView tab_native_view_;  // Weak reference.

  // The handler value for the focus out event, allows us to block and unblock
  // it as needed.
  gulong event_handler_id_;

  DISALLOW_COPY_AND_ASSIGN(AutofillExternalDelegateGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_AUTOFILL_AUTOFILL_EXTERNAL_DELEGATE_GTK_H_
