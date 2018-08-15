// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_base_view.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/autofill/popup_view_common.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_features.h"
#include "ui/gfx/color_palette.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/fill_layout.h"

namespace autofill {

// TODO(crbug.com/831603): Determine how colors should be shared with menus
// and/or omnibox, and how these should interact (if at all) with native
// theme colors.
const SkColor AutofillPopupBaseView::kBackgroundColor = SK_ColorWHITE;
const SkColor AutofillPopupBaseView::kSelectedBackgroundColor =
    gfx::kGoogleGrey200;
const SkColor AutofillPopupBaseView::kFooterBackgroundColor =
    gfx::kGoogleGrey050;
const SkColor AutofillPopupBaseView::kSeparatorColor = gfx::kGoogleGrey200;
const SkColor AutofillPopupBaseView::kWarningColor = gfx::kGoogleRed600;

int AutofillPopupBaseView::GetCornerRadius() {
  return ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::EMPHASIS_MEDIUM);
}

AutofillPopupBaseView::AutofillPopupBaseView(
    AutofillPopupViewDelegate* delegate,
    views::Widget* parent_widget)
    : delegate_(delegate),
      parent_widget_(parent_widget),
      weak_ptr_factory_(this) {}

AutofillPopupBaseView::~AutofillPopupBaseView() {
  if (delegate_) {
    delegate_->ViewDestroyed();

    RemoveWidgetObservers();
  }
}

void AutofillPopupBaseView::DoShow() {
  const bool initialize_widget = !GetWidget();
  if (initialize_widget) {
    // On Mac Cocoa browser, |parent_widget_| is null (the parent is not a
    // views::Widget).
    // TODO(crbug.com/826862): Remove |parent_widget_|.
    if (parent_widget_)
      parent_widget_->AddObserver(this);

    // The widget is destroyed by the corresponding NativeWidget, so we use
    // a weak pointer to hold the reference and don't have to worry about
    // deletion.
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
    params.delegate = this;
    params.parent = parent_widget_ ? parent_widget_->GetNativeView()
                                   : delegate_->container_view();
    AddExtraInitParams(&params);
    widget->Init(params);

    widget->SetContentsView(CreateWrapperView().release());
    widget->AddObserver(this);

    // No animation for popup appearance (too distracting).
    widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_HIDE);

    show_time_ = base::Time::Now();
  }

  GetWidget()->GetRootView()->SetBorder(CreateBorder());
  DoUpdateBoundsAndRedrawPopup();
  GetWidget()->Show();

#if defined(OS_MACOSX)
  mac_bubble_closer_ = std::make_unique<ui::BubbleCloser>(
      GetWidget()->GetNativeWindow(),
      base::BindRepeating(&AutofillPopupBaseView::HideController,
                          base::Unretained(this)));
#endif

  // Showing the widget can change native focus (which would result in an
  // immediate hiding of the popup). Only start observing after shown.
  if (initialize_widget)
    views::WidgetFocusManager::GetInstance()->AddFocusChangeListener(this);
}

void AutofillPopupBaseView::DoHide() {
  // The controller is no longer valid after it hides us.
  delegate_ = NULL;

  RemoveWidgetObservers();

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
  DCHECK(widget == parent_widget_ || widget == GetWidget());
  if (widget != parent_widget_)
    return;

  HideController();
}

void AutofillPopupBaseView::OnWidgetDestroying(views::Widget* widget) {
  // On Windows, widgets can be destroyed in any order. Regardless of which
  // widget is destroyed first, remove all observers and hide the popup.
  DCHECK(widget == parent_widget_ || widget == GetWidget());

  // Normally this happens at destruct-time or hide-time, but because it depends
  // on |parent_widget_| (which is about to go away), it needs to happen sooner
  // in this case.
  RemoveWidgetObservers();

  // Because the parent widget is about to be destroyed, we null out the weak
  // reference to it and protect against possibly accessing it during
  // destruction (e.g., by attempting to remove observers).
  parent_widget_ = nullptr;

  HideController();
}

void AutofillPopupBaseView::RemoveWidgetObservers() {
  if (parent_widget_)
    parent_widget_->RemoveObserver(this);
  GetWidget()->RemoveObserver(this);

  views::WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(this);
}

// TODO(crbug.com/831603): Inline this function once AutofillPopupViewViews is
// deleted.
void AutofillPopupBaseView::AddExtraInitParams(
    views::Widget::InitParams* params) {
  // Ensure the bubble border is not painted on an opaque background.
  params->opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params->shadow_type = views::Widget::InitParams::SHADOW_TYPE_NONE;
}

std::unique_ptr<views::View> AutofillPopupBaseView::CreateWrapperView() {
  // Create a wrapper view that contains the current view and will receive the
  // bubble border. This is needed so that a clipping path can be later applied
  // on the contents only and not affect the border.
  auto wrapper_view = std::make_unique<views::View>();
  wrapper_view->SetLayoutManager(std::make_unique<views::FillLayout>());
  wrapper_view->AddChildView(this);
  return wrapper_view;
}

std::unique_ptr<views::Border> AutofillPopupBaseView::CreateBorder() {
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::SMALL_SHADOW,
      SK_ColorWHITE);
  border->SetCornerRadius(GetCornerRadius());
  border->set_md_shadow_elevation(
      ChromeLayoutProvider::Get()->GetShadowElevationMetric(
          views::EMPHASIS_MEDIUM));
  return border;
}

void AutofillPopupBaseView::SetClipPath() {
  SkRect local_bounds = gfx::RectToSkRect(GetLocalBounds());
  SkScalar radius = SkIntToScalar(GetCornerRadius());
  gfx::Path clip_path;
  clip_path.addRoundRect(local_bounds, radius, radius);
  set_clip_path(clip_path);
}

void AutofillPopupBaseView::DoUpdateBoundsAndRedrawPopup() {
  gfx::Rect bounds = delegate()->popup_bounds();

  SetSize(bounds.size());

  gfx::Rect clipping_bounds = CalculateClippingBounds();

  int available_vertical_space =
      clipping_bounds.height() - (bounds.y() - clipping_bounds.y());

  if (available_vertical_space < bounds.height())
    bounds.set_height(available_vertical_space);

  // Account for the scroll view's border so that the content has enough space.
  bounds.Inset(-GetWidget()->GetRootView()->border()->GetInsets());
  GetWidget()->SetBounds(bounds);
  SetClipPath();

  SchedulePaint();
}

gfx::Rect AutofillPopupBaseView::CalculateClippingBounds() const {
  if (parent_widget_)
    return parent_widget_->GetClientAreaBoundsInScreen();

  return PopupViewCommon().GetWindowBounds(delegate_->container_view());
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
  // There is no need to post a ClearSelection task if no row is selected.
  if (!delegate_ || !delegate_->HasSelection())
    return;

  // Pressing return causes the cursor to hide, which will generate an
  // OnMouseExited event. Pressing return should activate the current selection
  // via AcceleratorPressed, so we need to let that run first.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&AutofillPopupBaseView::ClearSelection,
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

void AutofillPopupBaseView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kMenu;
  node_data->SetName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_POPUP_ACCESSIBLE_NODE_DATA));
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
  // This will eventually result in the deletion of |this|, as the delegate
  // will hide |this|. See |DoHide| above for an explanation on why the precise
  // timing of that deletion is tricky.
}

gfx::NativeView AutofillPopupBaseView::container_view() {
  return delegate_->container_view();
}

}  // namespace autofill
