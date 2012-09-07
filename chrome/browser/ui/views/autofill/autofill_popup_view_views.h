// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_VIEWS_H_

#include "chrome/browser/autofill/autofill_popup_view.h"
#include "content/public/browser/keyboard_listener.h"
#include "ui/views/widget/widget_delegate.h"

class AutofillExternalDelegateViews;

namespace content {
class WebContents;
}

// The View Autofill popup implementation.
class AutofillPopupViewViews : public views::WidgetDelegateView,
                               public AutofillPopupView,
                               public KeyboardListener {
 public:
  AutofillPopupViewViews(content::WebContents* web_contents,
                         AutofillExternalDelegateViews* external_delegate);

 private:
  class AutofillPopupWidget;

  virtual ~AutofillPopupViewViews();

  // views:Views implementation.
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // AutofillPopupView implementation.
  virtual void ShowInternal() OVERRIDE;
  virtual void HideInternal() OVERRIDE;
  virtual void InvalidateRow(size_t row) OVERRIDE;
  virtual void ResizePopup() OVERRIDE;

  AutofillExternalDelegateViews* external_delegate_;  // Weak reference.
  content::WebContents* web_contents_;  // Weak reference.

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupViewViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_VIEWS_H_
