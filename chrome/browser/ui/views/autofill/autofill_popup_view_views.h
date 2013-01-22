// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_VIEWS_H_

#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

class AutofillPopupController;

namespace content {
class WebContents;
}

// Views toolkit implementation for AutofillPopupView.
class AutofillPopupViewViews : public AutofillPopupView,
                               public views::WidgetDelegateView,
                               public views::WidgetObserver {
 public:
  explicit AutofillPopupViewViews(AutofillPopupController* controller);

 private:
  virtual ~AutofillPopupViewViews();

  // AutofillPopupView implementation.
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void InvalidateRow(size_t row) OVERRIDE;
  virtual void UpdateBoundsAndRedrawPopup() OVERRIDE;

  // views:Views implementation.
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // views::WidgetObserver implementation.
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseMoved(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnWidgetBoundsChanged(views::Widget* widget,
                                     const gfx::Rect& new_bounds) OVERRIDE;

  // Draw the given autofill entry in |entry_rect|.
  void DrawAutofillEntry(gfx::Canvas* canvas,
                         int index,
                         const gfx::Rect& entry_rect);

  AutofillPopupController* controller_;  // Weak reference.

  // The widget that |this| observes. Weak reference.
  views::Widget* observing_widget_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupViewViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_VIEWS_H_
