// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_slider_view.h"

#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/top_shortcut_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

views::Slider* CreateSlider(UnifiedSliderListener* listener, bool readonly) {
  if (readonly)
    return new ReadOnlySlider();

  return new views::Slider(listener);
}

}  // namespace

ReadOnlySlider::ReadOnlySlider() : Slider(nullptr) {}

bool ReadOnlySlider::OnMousePressed(const ui::MouseEvent& event) {
  return false;
}

bool ReadOnlySlider::OnMouseDragged(const ui::MouseEvent& event) {
  return false;
}

void ReadOnlySlider::OnMouseReleased(const ui::MouseEvent& event) {}

bool ReadOnlySlider::OnKeyPressed(const ui::KeyEvent& event) {
  return false;
}

void ReadOnlySlider::OnGestureEvent(ui::GestureEvent* event) {}

UnifiedSliderButton::UnifiedSliderButton(views::ButtonListener* listener,
                                         const gfx::VectorIcon& icon,
                                         int accessible_name_id)
    : TopShortcutButton(listener, accessible_name_id) {
  SetVectorIcon(icon);
}

UnifiedSliderButton::~UnifiedSliderButton() = default;

void UnifiedSliderButton::SetVectorIcon(const gfx::VectorIcon& icon) {
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(icon, kUnifiedMenuIconColor));
  SetImage(views::Button::STATE_DISABLED,
           gfx::CreateVectorIcon(icon, kUnifiedMenuIconColor));
}

void UnifiedSliderButton::SetToggled(bool toggled) {
  toggled_ = toggled;
  SchedulePaint();
}

void UnifiedSliderButton::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::Rect rect(GetContentsBounds());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(toggled_ ? kUnifiedMenuButtonColorActive
                          : kUnifiedMenuButtonColor);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawCircle(gfx::PointF(rect.CenterPoint()), kTrayItemSize / 2, flags);

  views::ImageButton::PaintButtonContents(canvas);
}

void UnifiedSliderButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  TopShortcutButton::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kToggleButton;
  node_data->SetCheckedState(toggled_ ? ax::mojom::CheckedState::kTrue
                                      : ax::mojom::CheckedState::kFalse);
}

UnifiedSliderView::UnifiedSliderView(UnifiedSliderListener* listener,
                                     const gfx::VectorIcon& icon,
                                     int accessible_name_id,
                                     bool readonly)
    : button_(new UnifiedSliderButton(listener, icon, accessible_name_id)),
      slider_(CreateSlider(listener, readonly)) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, kUnifiedMenuItemPadding,
      kUnifiedTopShortcutSpacing));

  AddChildView(button_);
  AddChildView(slider_);

  // Prevent an accessibility event while initiallizing this view. Typically
  // the first update of the slider value is conducted by the caller function
  // to reflect the current value.
  slider_->set_enable_accessibility_events(false);

  slider_->GetViewAccessibility().OverrideName(
      l10n_util::GetStringUTF16(accessible_name_id));
  slider_->SetBorder(views::CreateEmptyBorder(kUnifiedSliderPadding));
  slider_->SetPreferredSize(gfx::Size(0, kTrayItemSize));
  layout->SetFlexForView(slider_, 1);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
}

void UnifiedSliderView::SetSliderValue(float value, bool by_user) {
  // SetValue() calls |listener|, so we should ignore the call when the widget
  // is closed, because controllers are already deleted.
  // It should allow the case GetWidget() returning null, so that initial
  // position can be properly set by controllers before the view is attached to
  // a widget.
  if (GetWidget() && GetWidget()->IsClosed())
    return;

  slider_->SetValue(value);
  if (by_user)
    slider_->set_enable_accessibility_events(true);
}

UnifiedSliderView::~UnifiedSliderView() = default;

}  // namespace ash
