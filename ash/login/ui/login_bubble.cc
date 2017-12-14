// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_bubble.h"

#include "ash/login/ui/login_button.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/font.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// The size of the alert icon in the error bubble.
constexpr int kAlertIconSizeDp = 20;

// Vertical spacing between the anchor view and error bubble.
constexpr int kAnchorViewErrorBubbleVerticalSpacingDp = 48;

// An alpha value for the sub message in the user menu.
constexpr SkAlpha kSubMessageColorAlpha = 0x89;

// Horizontal spacing with the anchor view.
constexpr int kAnchorViewHorizontalSpacingDp = 105;

// Vertical spacing between the anchor view and user menu.
constexpr int kAnchorViewUserMenuVerticalSpacingDp = 4;

// The amount of time for bubble show/hide animation.
constexpr int kBubbleAnimationDurationMs = 300;

views::Label* CreateLabel(const base::string16& message, SkColor color) {
  views::Label* label = new views::Label(message, views::style::CONTEXT_LABEL,
                                         views::style::STYLE_PRIMARY);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColor(color);
  const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();
  label->SetFontList(base_font_list.Derive(0, gfx::Font::FontStyle::NORMAL,
                                           gfx::Font::Weight::NORMAL));
  return label;
}

class LoginErrorBubbleView : public LoginBaseBubbleView {
 public:
  LoginErrorBubbleView(views::StyledLabel* label, views::View* anchor_view)
      : LoginBaseBubbleView(anchor_view) {
    set_anchor_view_insets(
        gfx::Insets(kAnchorViewErrorBubbleVerticalSpacingDp, 0));

    views::View* alert_view = new views::View();
    alert_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kHorizontal, gfx::Insets()));
    views::ImageView* alert_icon = new views::ImageView();
    alert_icon->SetPreferredSize(gfx::Size(kAlertIconSizeDp, kAlertIconSizeDp));
    alert_icon->SetImage(
        gfx::CreateVectorIcon(kLockScreenAlertIcon, SK_ColorWHITE));
    alert_view->AddChildView(alert_icon);
    AddChildView(alert_view);

    label->set_auto_color_readability_enabled(false);
    AddChildView(label);
  }

  ~LoginErrorBubbleView() override = default;

  // views::View:
  const char* GetClassName() const override { return "LoginErrorBubbleView"; }
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ui::AX_ROLE_TOOLTIP;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginErrorBubbleView);
};

class LoginUserMenuView : public LoginBaseBubbleView {
 public:
  LoginUserMenuView(const base::string16& message,
                    const base::string16& sub_message,
                    views::View* anchor_view,
                    bool show_remove_user)
      : LoginBaseBubbleView(anchor_view) {
    views::Label* label = CreateLabel(message, SK_ColorWHITE);
    views::Label* sub_label = CreateLabel(
        sub_message, SkColorSetA(SK_ColorWHITE, kSubMessageColorAlpha));
    AddChildView(label);
    AddChildView(sub_label);
    set_anchor_view_insets(gfx::Insets(kAnchorViewUserMenuVerticalSpacingDp,
                                       kAnchorViewHorizontalSpacingDp));

    // TODO: Show remove user in the menu in login screen.
  }

  ~LoginUserMenuView() override = default;

  // views::View:
  const char* GetClassName() const override { return "LoginUserMenuView"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginUserMenuView);
};

class LoginTooltipView : public LoginBaseBubbleView {
 public:
  LoginTooltipView(const base::string16& message, views::View* anchor_view)
      : LoginBaseBubbleView(anchor_view) {
    views::Label* text = CreateLabel(message, SK_ColorWHITE);
    text->SetMultiLine(true);
    AddChildView(text);
  }

  // LoginBaseBubbleView:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ui::AX_ROLE_TOOLTIP;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginTooltipView);
};

}  // namespace

LoginBubble::LoginBubble() {
  Shell::Get()->AddPreTargetHandler(this);
}

LoginBubble::~LoginBubble() {
  Shell::Get()->RemovePreTargetHandler(this);
  if (bubble_view_)
    bubble_view_->GetWidget()->RemoveObserver(this);
}

void LoginBubble::ShowErrorBubble(views::StyledLabel* label,
                                  views::View* anchor_view) {
  if (bubble_view_)
    CloseImmediately();

  bubble_view_ = new LoginErrorBubbleView(label, anchor_view);
  Show();
}

void LoginBubble::ShowUserMenu(const base::string16& message,
                               const base::string16& sub_message,
                               views::View* anchor_view,
                               LoginButton* bubble_opener,
                               bool show_remove_user) {
  if (bubble_view_)
    CloseImmediately();

  bubble_opener_ = bubble_opener;
  bubble_view_ = new LoginUserMenuView(message, sub_message, anchor_view,
                                       show_remove_user);
  Show();
}

void LoginBubble::ShowTooltip(const base::string16& message,
                              views::View* anchor_view) {
  if (bubble_view_)
    CloseImmediately();

  bubble_view_ = new LoginTooltipView(message, anchor_view);
  Show();
}

void LoginBubble::Close() {
  ScheduleAnimation(false /*visible*/);
}

bool LoginBubble::IsVisible() {
  return bubble_view_ && bubble_view_->GetWidget()->IsVisible();
}

void LoginBubble::OnWidgetClosing(views::Widget* widget) {
  bubble_opener_ = nullptr;
  bubble_view_ = nullptr;
  widget->RemoveObserver(this);
}

void LoginBubble::OnWidgetDestroying(views::Widget* widget) {
  OnWidgetClosing(widget);
}

void LoginBubble::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    ProcessPressedEvent(event->AsLocatedEvent());
}

void LoginBubble::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_TAP_DOWN) {
    ProcessPressedEvent(event->AsLocatedEvent());
  }
}

void LoginBubble::OnKeyEvent(ui::KeyEvent* event) {
  if (!bubble_view_ || event->type() != ui::ET_KEY_PRESSED)
    return;

  // If current focus view is the button view, don't process the event here,
  // let the button logic handle the event and determine show/hide behavior.
  if (bubble_opener_ && bubble_opener_->HasFocus())
    return;

  Close();
}

void LoginBubble::OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) {
  if (!bubble_view_)
    return;

  bubble_view_->layer()->GetAnimator()->RemoveObserver(this);
  if (!is_visible_)
    bubble_view_->GetWidget()->Close();
}

void LoginBubble::Show() {
  DCHECK(bubble_view_);
  views::BubbleDialogDelegateView::CreateBubble(bubble_view_)->Show();
  bubble_view_->SetAlignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
  bubble_view_->GetWidget()->AddObserver(this);

  ScheduleAnimation(true /*visible*/);

  // Fire an alert so ChromeVox will read the contents of the bubble.
  bubble_view_->NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);
}

void LoginBubble::CloseImmediately() {
  DCHECK(bubble_view_);
  bubble_view_->layer()->GetAnimator()->RemoveObserver(this);
  bubble_view_->GetWidget()->Close();
  is_visible_ = false;
}

void LoginBubble::ProcessPressedEvent(const ui::LocatedEvent* event) {
  if (!bubble_view_)
    return;

  // Don't process event inside the bubble.
  gfx::Point screen_location = event->location();
  ::wm::ConvertPointToScreen(static_cast<aura::Window*>(event->target()),
                             &screen_location);
  gfx::Rect bounds = bubble_view_->GetWidget()->GetWindowBoundsInScreen();
  if (bounds.Contains(screen_location))
    return;

  // If the user clicks on the button view, don't process the event here,
  // let the button logic handle the event and determine show/hide behavior.
  if (bubble_opener_) {
    gfx::Rect bubble_opener_bounds = bubble_opener_->GetBoundsInScreen();
    if (bubble_opener_bounds.Contains(screen_location))
      return;
  }

  Close();
}

void LoginBubble::ScheduleAnimation(bool visible) {
  if (!bubble_view_ || is_visible_ == visible)
    return;

  if (bubble_opener_) {
    bubble_opener_->AnimateInkDrop(visible ? views::InkDropState::ACTIVATED
                                           : views::InkDropState::DEACTIVATED,
                                   nullptr /*event*/);
  }

  ui::Layer* layer = bubble_view_->layer();
  layer->GetAnimator()->StopAnimating();
  is_visible_ = visible;

  float opacity_start = 0.0f;
  float opacity_end = 1.0f;
  if (!is_visible_)
    std::swap(opacity_start, opacity_end);

  layer->GetAnimator()->AddObserver(this);
  layer->SetOpacity(opacity_start);
  {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kBubbleAnimationDurationMs));
    settings.SetTweenType(is_visible_ ? gfx::Tween::EASE_OUT
                                      : gfx::Tween::EASE_IN);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    layer->SetOpacity(opacity_end);
  }
}

}  // namespace ash
