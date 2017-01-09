// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_base_view.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/popup_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

namespace autofill {

AutofillPopupBaseView::AutofillPopupBaseView(
    AutofillPopupViewDelegate* delegate,
    views::Widget* parent_widget)
    : delegate_(delegate),
      parent_widget_(parent_widget),
      weak_ptr_factory_(this) {}

AutofillPopupBaseView::~AutofillPopupBaseView() {
  if (delegate_) {
    delegate_->ViewDestroyed();

    RemoveObserver();
  }
}

void AutofillPopupBaseView::DoShow() {
  const bool initialize_widget = !GetWidget();
  if (initialize_widget) {
    parent_widget_->AddObserver(this);
    views::FocusManager* focus_manager = parent_widget_->GetFocusManager();
    focus_manager->RegisterAccelerator(
        ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE),
        ui::AcceleratorManager::kNormalPriority,
        this);
    focus_manager->RegisterAccelerator(
        ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE),
        ui::AcceleratorManager::kNormalPriority,
        this);

    // The widget is destroyed by the corresponding NativeWidget, so we use
    // a weak pointer to hold the reference and don't have to worry about
    // deletion.
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
    params.delegate = this;
    params.parent = parent_widget_->GetNativeView();
    widget->Init(params);

    // No animation for popup appearance (too distracting).
    widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_HIDE);

    show_time_ = base::Time::Now();
  }

  // TODO(crbug.com/676164): Show different border color when focused/unfocused
  SetBorder(views::CreateSolidBorder(
      kPopupBorderThickness,
      GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_UnfocusedBorderColor)));

  DoUpdateBoundsAndRedrawPopup();
  GetWidget()->Show();

  // Showing the widget can change native focus (which would result in an
  // immediate hiding of the popup). Only start observing after shown.
  if (initialize_widget)
    views::WidgetFocusManager::GetInstance()->AddFocusChangeListener(this);
}

void AutofillPopupBaseView::DoHide() {
  // The controller is no longer valid after it hides us.
  delegate_ = NULL;

  RemoveObserver();

  if (GetWidget()) {
    // Don't call CloseNow() because some of the functions higher up the stack
    // assume the the widget is still valid after this point.
    // http://crbug.com/229224
    // NOTE: This deletes |this|.
    GetWidget()->Close();
  } else {
    delete this;
  }
}

void AutofillPopupBaseView::OnWidgetBoundsChanged(views::Widget* widget,
                                                  const gfx::Rect& new_bounds) {
  DCHECK_EQ(widget, parent_widget_);
  HideController();
}

void AutofillPopupBaseView::RemoveObserver() {
  parent_widget_->GetFocusManager()->UnregisterAccelerators(this);
  parent_widget_->RemoveObserver(this);
  views::WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(this);
}

void AutofillPopupBaseView::DoUpdateBoundsAndRedrawPopup() {
  GetWidget()->SetBounds(delegate_->popup_bounds());
  SchedulePaint();
}

void AutofillPopupBaseView::OnNativeFocusChanged(gfx::NativeView focused_now) {
  if (GetWidget() && GetWidget()->GetNativeView() != focused_now)
    HideController();
}

void AutofillPopupBaseView::OnMouseCaptureLost() {
  ClearSelection();
}

bool AutofillPopupBaseView::OnMouseDragged(const ui::MouseEvent& event) {
  if (HitTestPoint(event.location())) {
    SetSelection(event.location());

    // We must return true in order to get future OnMouseDragged and
    // OnMouseReleased events.
    return true;
  }

  // If we move off of the popup, we lose the selection.
  ClearSelection();
  return false;
}

void AutofillPopupBaseView::OnMouseExited(const ui::MouseEvent& event) {
  // Pressing return causes the cursor to hide, which will generate an
  // OnMouseExited event. Pressing return should activate the current selection
  // via AcceleratorPressed, so we need to let that run first.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&AutofillPopupBaseView::ClearSelection,
                            weak_ptr_factory_.GetWeakPtr()));
}

void AutofillPopupBaseView::OnMouseMoved(const ui::MouseEvent& event) {
  // A synthesized mouse move will be sent when the popup is first shown.
  // Don't preview a suggestion if the mouse happens to be hovering there.
#if defined(OS_WIN)
  // TODO(rouslan): Use event.time_stamp() and ui::EventTimeForNow() when they
  // become comparable. http://crbug.com/453559
  if (base::Time::Now() - show_time_ <= base::TimeDelta::FromMilliseconds(50))
    return;
#else
  if (event.flags() & ui::EF_IS_SYNTHESIZED)
    return;
#endif

  if (HitTestPoint(event.location()))
    SetSelection(event.location());
  else
    ClearSelection();
}

bool AutofillPopupBaseView::OnMousePressed(const ui::MouseEvent& event) {
  return event.GetClickCount() == 1;
}

void AutofillPopupBaseView::OnMouseReleased(const ui::MouseEvent& event) {
  // We only care about the left click.
  if (event.IsOnlyLeftMouseButton() && HitTestPoint(event.location()))
    AcceptSelection(event.location());
}

void AutofillPopupBaseView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
    case ui::ET_GESTURE_SCROLL_BEGIN:
    case ui::ET_GESTURE_SCROLL_UPDATE:
      if (HitTestPoint(event->location()))
        SetSelection(event->location());
      else
        ClearSelection();
      break;
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_SCROLL_END:
      if (HitTestPoint(event->location()))
        AcceptSelection(event->location());
      else
        ClearSelection();
      break;
    case ui::ET_GESTURE_TAP_CANCEL:
    case ui::ET_SCROLL_FLING_START:
      ClearSelection();
      break;
    default:
      return;
  }
  event->SetHandled();
}

bool AutofillPopupBaseView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  DCHECK_EQ(accelerator.modifiers(), ui::EF_NONE);

  if (accelerator.key_code() == ui::VKEY_ESCAPE) {
    HideController();
    return true;
  }

  if (accelerator.key_code() == ui::VKEY_RETURN)
    return delegate_->AcceptSelectedLine();

  NOTREACHED();
  return false;
}

void AutofillPopupBaseView::SetSelection(const gfx::Point& point) {
  if (delegate_)
    delegate_->SetSelectionAtPoint(point);
}

void AutofillPopupBaseView::AcceptSelection(const gfx::Point& point) {
  if (!delegate_)
    return;

  delegate_->SetSelectionAtPoint(point);
  delegate_->AcceptSelectedLine();
}

void AutofillPopupBaseView::ClearSelection() {
  if (delegate_)
    delegate_->SelectionCleared();
}

void AutofillPopupBaseView::HideController() {
  if (delegate_)
    delegate_->Hide();
}

gfx::NativeView AutofillPopupBaseView::container_view() {
  return delegate_->container_view();
}

}  // namespace autofill
