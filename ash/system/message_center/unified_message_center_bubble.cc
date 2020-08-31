// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/unified_message_center_bubble.h"

#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/default_color_constants.h"
#include "ash/system/message_center/unified_message_center_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_event_filter.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/widget/widget.h"

namespace ash {

// We need to draw a custom inner border for the message center in a separate
// layer so we can properly clip ARC notifications. Each ARC notification is
// contained in its own Window with its own layer, and the border needs to be
// drawn on top of them all.
class UnifiedMessageCenterBubble::Border : public ui::LayerDelegate {
 public:
  Border() : layer_(ui::LAYER_TEXTURED) {
    layer_.set_delegate(this);
    layer_.SetFillsBoundsOpaquely(false);
  }

  ~Border() override = default;

  ui::Layer* layer() { return &layer_; }

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    gfx::Rect bounds = layer()->bounds();
    ui::PaintRecorder recorder(context, bounds.size());
    gfx::Canvas* canvas = recorder.canvas();

    // Draw a solid rounded rect as the inner border.
    cc::PaintFlags flags;
    flags.setColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kSeparator,
        AshColorProvider::AshColorMode::kLight));
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(canvas->image_scale());
    flags.setAntiAlias(true);
    canvas->DrawRoundRect(bounds, kUnifiedTrayCornerRadius, flags);
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  ui::Layer layer_;

  DISALLOW_COPY_AND_ASSIGN(Border);
};

UnifiedMessageCenterBubble::UnifiedMessageCenterBubble(UnifiedSystemTray* tray)
    : tray_(tray), border_(std::make_unique<Border>()) {
  TrayBubbleView::InitParams init_params;
  init_params.delegate = this;
  // Anchor within the overlay container.
  init_params.parent_window = tray->GetBubbleWindowContainer();
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.preferred_width = kTrayMenuWidth;
  init_params.has_shadow = false;
  init_params.close_on_deactivate = false;

  bubble_view_ = new TrayBubbleView(init_params);

  message_center_view_ =
      bubble_view_->AddChildView(std::make_unique<UnifiedMessageCenterView>(
          nullptr /* parent */, tray->model(), this));

  message_center_view_->AddObserver(this);
}

void UnifiedMessageCenterBubble::ShowBubble() {
  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  bubble_widget_->AddObserver(this);
  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);

  tray_->tray_event_filter()->AddBubble(this);
  tray_->bubble()->unified_view()->AddObserver(this);

  ui::Layer* widget_layer = bubble_widget_->GetLayer();
  int radius = kUnifiedTrayCornerRadius;
  widget_layer->SetRoundedCornerRadius({radius, radius, radius, radius});
  widget_layer->SetIsFastRoundedCorner(true);
  widget_layer->Add(border_->layer());

  bubble_view_->InitializeAndShowBubble();

  // Check if the message center bubble should be collapsed or expanded
  // when it is initially opened.
  if (CalculateAvailableHeight() < kMessageCenterCollapseThreshold &&
      message_center_view_->GetPreferredSize().height()) {
    if (tray_->IsQuickSettingsExplicitlyExpanded()) {
      message_center_view_->SetCollapsed(false /*animate*/);
    } else {
      message_center_view_->SetExpanded();
      tray_->EnsureQuickSettingsCollapsed(false /*animate*/);
    }
  }

  UpdatePosition();
}

UnifiedMessageCenterBubble::~UnifiedMessageCenterBubble() {
  if (bubble_widget_) {
    tray_->tray_event_filter()->RemoveBubble(this);
    tray_->bubble()->unified_view()->RemoveObserver(this);
    CHECK(message_center_view_);
    message_center_view_->RemoveObserver(this);

    bubble_view_->ResetDelegate();
    bubble_widget_->RemoveObserver(this);
    bubble_widget_->CloseNow();
  }
}

int UnifiedMessageCenterBubble::CalculateAvailableHeight() {
  return tray_->bubble()->CalculateMaxHeight() -
         tray_->bubble()->GetCurrentTrayHeight() -
         kUnifiedMessageCenterBubbleSpacing;
}

void UnifiedMessageCenterBubble::CollapseMessageCenter() {
  if (message_center_view_->collapsed())
    return;

  message_center_view_->SetCollapsed(true /*animate*/);
}

void UnifiedMessageCenterBubble::ExpandMessageCenter() {
  if (!message_center_view_->collapsed())
    return;

  message_center_view_->SetExpanded();
  UpdatePosition();
  tray_->EnsureQuickSettingsCollapsed(true /*animate*/);
}

void UnifiedMessageCenterBubble::UpdatePosition() {
  int available_height = CalculateAvailableHeight();

  message_center_view_->SetMaxHeight(available_height);
  message_center_view_->SetAvailableHeight(available_height);

  // Note: It's tempting to set the insets for TrayBubbleView in order to
  // achieve the padding, but that enlarges the layer bounds and breaks rounded
  // corner clipping for ARC notifications. This approach only modifies the
  // position of the layer.
  gfx::Rect anchor_rect = tray_->shelf()->GetSystemTrayAnchorRect();

  gfx::Insets tray_bubble_insets = GetTrayBubbleInsets();
  int left_offset = (tray_->shelf()->alignment() == ShelfAlignment::kLeft ||
                     base::i18n::IsRTL())
                        ? tray_bubble_insets.left()
                        : -tray_bubble_insets.right();

  anchor_rect.set_x(anchor_rect.x() + left_offset);
  anchor_rect.set_y(anchor_rect.y() - tray_->bubble()->GetCurrentTrayHeight() -
                    tray_bubble_insets.bottom() -
                    kUnifiedMessageCenterBubbleSpacing);
  bubble_view_->ChangeAnchorRect(anchor_rect);

  bubble_widget_->GetLayer()->StackAtTop(border_->layer());
  border_->layer()->SetBounds(message_center_view_->GetContentsBounds());
}

void UnifiedMessageCenterBubble::FocusEntered(bool reverse) {
  message_center_view_->FocusEntered(reverse);
}

bool UnifiedMessageCenterBubble::FocusOut(bool reverse) {
  return tray_->FocusQuickSettings(reverse);
}

void UnifiedMessageCenterBubble::ActivateQuickSettingsBubble() {
  tray_->ActivateBubble();
}

void UnifiedMessageCenterBubble::FocusFirstNotification() {
  // Move focus to first notification from notification bar if it is visible.
  if (message_center_view_->IsNotificationBarVisible())
    message_center_view_->GetFocusManager()->AdvanceFocus(false /*reverse*/);
}

bool UnifiedMessageCenterBubble::IsMessageCenterVisible() {
  return !!bubble_widget_ && message_center_view_->GetVisible();
}

bool UnifiedMessageCenterBubble::IsMessageCenterCollapsed() {
  return message_center_view_->collapsed();
}

TrayBackgroundView* UnifiedMessageCenterBubble::GetTray() const {
  return tray_;
}

TrayBubbleView* UnifiedMessageCenterBubble::GetBubbleView() const {
  return bubble_view_;
}

views::Widget* UnifiedMessageCenterBubble::GetBubbleWidget() const {
  return bubble_widget_;
}

base::string16 UnifiedMessageCenterBubble::GetAccessibleNameForBubble() {
  return l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_ACCESSIBLE_NAME);
}

bool UnifiedMessageCenterBubble::ShouldEnableExtraKeyboardAccessibility() {
  return Shell::Get()->accessibility_controller()->spoken_feedback_enabled();
}

void UnifiedMessageCenterBubble::OnViewPreferredSizeChanged(
    views::View* observed_view) {
  UpdatePosition();
  bubble_view_->Layout();
}

void UnifiedMessageCenterBubble::OnWidgetDestroying(views::Widget* widget) {
  CHECK_EQ(bubble_widget_, widget);
  tray_->tray_event_filter()->RemoveBubble(this);
  tray_->bubble()->unified_view()->RemoveObserver(this);
  message_center_view_->RemoveObserver(this);
  bubble_widget_->RemoveObserver(this);
  bubble_widget_ = nullptr;
  bubble_view_->ResetDelegate();

  // Close the quick settings bubble as well, which may not automatically happen
  // when dismissing the message center bubble by pressing ESC.
  tray_->CloseBubble();
}

void UnifiedMessageCenterBubble::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  if (active)
    tray_->bubble()->OnMessageCenterActivated();
}

}  // namespace ash
