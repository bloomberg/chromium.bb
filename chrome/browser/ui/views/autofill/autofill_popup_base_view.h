// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_BASE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_BASE_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/widget/widget_delegate.h"

namespace content {
class WebContents;
}

namespace gfx {
class Point;
}

namespace views {
class FocusManager;
}

namespace autofill {

// Class that deals with the event handling for Autofill-style popups. This
// class should only be instantiated by sub-classes.
class AutofillPopupBaseView : public views::WidgetDelegateView,
                              public views::WidgetFocusChangeListener {
 public:
  static const SkColor kBorderColor;
  static const SkColor kHoveredBackgroundColor;
  static const SkColor kItemTextColor;
  static const SkColor kPopupBackground;
  static const SkColor kValueTextColor;
  static const SkColor kWarningTextColor;

 protected:
  explicit AutofillPopupBaseView(AutofillPopupViewDelegate* delegate,
                                 views::FocusManager* focus_manager);
  ~AutofillPopupBaseView() override;

  // Show this popup. Idempotent.
  void DoShow();

  // Hide the widget and delete |this|.
  void DoHide();

  // Update size of popup and paint.
  void DoUpdateBoundsAndRedrawPopup();

 private:
  friend class AutofillPopupBaseViewTest;

  // views::Views implementation.
  void OnMouseCaptureLost() override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // views::WidgetFocusChangeListener implementation.
  void OnNativeFocusChanged(gfx::NativeView focused_now) override;

  // Stop observing accelerators and focus changes.
  void RemoveObserver();

  void SetSelection(const gfx::Point& point);
  void AcceptSelection(const gfx::Point& point);
  void ClearSelection();

  // Hide the controller of this view. This assumes that doing so will
  // eventually hide this view in the process.
  void HideController();

  // Must return the container view for this popup.
  gfx::NativeView container_view();

  // Controller for this popup. Weak reference.
  AutofillPopupViewDelegate* delegate_;

  // The focus manager that |this| observes. Weak reference.
  views::FocusManager* focus_manager_;

  base::WeakPtrFactory<AutofillPopupBaseView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupBaseView);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_BASE_VIEW_H_
