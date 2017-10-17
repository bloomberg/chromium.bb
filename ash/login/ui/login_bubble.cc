// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_bubble.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ui/gfx/font.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// The size of the alert icon in the error bubble.
constexpr int kAlertIconSizeDp = 20;

// Vertical spacing with the anchor view.
constexpr int kAnchorViewVerticalSpacingDp = 48;

// An alpha value for the sub message in the user menu.
constexpr SkAlpha kSubMessageColorAlpha = 0x89;

// Horizontal spacing with the anchor view.
constexpr int kAnchorViewHorizontalSpacingDp = 100;

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
  LoginErrorBubbleView(const base::string16& message, views::View* anchor_view)
      : LoginBaseBubbleView(anchor_view) {
    set_anchor_view_insets(gfx::Insets(kAnchorViewVerticalSpacingDp, 0));

    views::View* alert_view = new views::View();
    alert_view->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kHorizontal, gfx::Insets()));
    views::ImageView* alert_icon = new views::ImageView();
    alert_icon->SetPreferredSize(gfx::Size(kAlertIconSizeDp, kAlertIconSizeDp));
    alert_icon->SetImage(
        gfx::CreateVectorIcon(kLockScreenAlertIcon, SK_ColorWHITE));
    alert_view->AddChildView(alert_icon);
    AddChildView(alert_view);

    views::Label* label = CreateLabel(message, SK_ColorWHITE);
    label->SetMultiLine(true);
    AddChildView(label);
  }

  ~LoginErrorBubbleView() override = default;

  // views::View:
  const char* GetClassName() const override { return "LoginErrorBubbleView"; }

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
    set_anchor_view_insets(gfx::Insets(0, kAnchorViewHorizontalSpacingDp));

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

void LoginBubble::ShowErrorBubble(const base::string16& message,
                                  views::View* anchor_view) {
  DCHECK_EQ(bubble_view_, nullptr);
  bubble_view_ = new LoginErrorBubbleView(message, anchor_view);
  Show();
}

void LoginBubble::ShowUserMenu(const base::string16& message,
                               const base::string16& sub_message,
                               views::View* anchor_view,
                               views::View* bubble_opener,
                               bool show_remove_user) {
  DCHECK_EQ(bubble_view_, nullptr);
  bubble_opener_ = bubble_opener;
  bubble_view_ = new LoginUserMenuView(message, sub_message, anchor_view,
                                       show_remove_user);
  Show();
}

void LoginBubble::ShowTooltip(const base::string16& message,
                              views::View* anchor_view) {
  if (bubble_view_)
    Close();

  bubble_view_ = new LoginTooltipView(message, anchor_view);
  Show();
}

void LoginBubble::Close() {
  if (bubble_view_)
    bubble_view_->GetWidget()->Close();
}

bool LoginBubble::IsVisible() {
  return bubble_view_ && bubble_view_->GetWidget()->IsVisible();
}

void LoginBubble::OnWidgetClosing(views::Widget* widget) {
  bubble_opener_ = nullptr;
  widget->RemoveObserver(this);
  bubble_view_ = nullptr;
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

void LoginBubble::Show() {
  DCHECK(bubble_view_);
  views::BubbleDialogDelegateView::CreateBubble(bubble_view_)->Show();
  bubble_view_->SetAlignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
  bubble_view_->GetWidget()->AddObserver(this);
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

}  // namespace ash
