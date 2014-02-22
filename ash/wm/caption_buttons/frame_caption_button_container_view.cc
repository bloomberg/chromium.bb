// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/caption_buttons/frame_caption_button_container_view.h"

#include "ash/ash_switches.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shell.h"
#include "ash/wm/caption_buttons/alternate_frame_size_button.h"
#include "ash/wm/caption_buttons/frame_caption_button.h"
#include "ash/wm/caption_buttons/frame_maximize_button.h"
#include "grit/ash_resources.h"
#include "grit/ui_strings.h"  // Accessibility names
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/point.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

// The distance between buttons.
const int kDistanceBetweenButtons = -1;

// Converts |point| from |src| to |dst| and hittests against |dst|.
bool ConvertPointToViewAndHitTest(const views::View* src,
                                  const views::View* dst,
                                  const gfx::Point& point) {
  gfx::Point converted(point);
  views::View::ConvertPointToTarget(src, dst, &converted);
  return dst->HitTestPoint(converted);
}

}  // namespace

// static
const char FrameCaptionButtonContainerView::kViewClassName[] =
    "FrameCaptionButtonContainerView";

FrameCaptionButtonContainerView::FrameCaptionButtonContainerView(
    views::Widget* frame,
    MinimizeAllowed minimize_allowed)
    : frame_(frame),
      minimize_button_(NULL),
      size_button_(NULL),
      close_button_(NULL) {
  bool alternate_style = switches::UseAlternateFrameCaptionButtonStyle();

  // Insert the buttons left to right.
  minimize_button_ = new FrameCaptionButton(this, CAPTION_BUTTON_ICON_MINIMIZE);
  minimize_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MINIMIZE));
  // Hide |minimize_button_| when using the non-alternate button style because
  // |size_button_| is capable of minimizing in this case.
  minimize_button_->SetVisible(
      minimize_allowed == MINIMIZE_ALLOWED &&
      (alternate_style || !frame_->widget_delegate()->CanMaximize()));
  AddChildView(minimize_button_);

  if (alternate_style)
    size_button_ = new AlternateFrameSizeButton(this, frame, this);
  else
    size_button_ = new FrameMaximizeButton(this, frame);
  size_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MAXIMIZE));
  size_button_->SetVisible(frame_->widget_delegate()->CanMaximize());
  AddChildView(size_button_);

  close_button_ = new FrameCaptionButton(this, CAPTION_BUTTON_ICON_CLOSE);
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
  AddChildView(close_button_);

  button_separator_ = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_AURA_WINDOW_BUTTON_SEPARATOR).AsImageSkia();
}

FrameCaptionButtonContainerView::~FrameCaptionButtonContainerView() {
}

FrameMaximizeButton*
FrameCaptionButtonContainerView::GetOldStyleSizeButton() {
  return switches::UseAlternateFrameCaptionButtonStyle() ?
      NULL : static_cast<FrameMaximizeButton*>(size_button_);
}

void FrameCaptionButtonContainerView::SetButtonImages(CaptionButtonIcon icon,
                                                      int normal_image_id,
                                                      int hovered_image_id,
                                                      int pressed_image_id) {
  button_icon_id_map_[icon] = ButtonIconIds(normal_image_id,
                                            hovered_image_id,
                                            pressed_image_id);
  FrameCaptionButton* buttons[] = {
    minimize_button_, size_button_, close_button_
  };
  for (size_t i = 0; i < arraysize(buttons); ++i) {
    if (buttons[i]->icon() == icon) {
      buttons[i]->SetImages(icon,
                            FrameCaptionButton::ANIMATE_NO,
                            normal_image_id,
                            hovered_image_id,
                            pressed_image_id);
    }
  }
}

void FrameCaptionButtonContainerView::ResetWindowControls() {
  SetButtonsToNormal(ANIMATE_NO);
}

int FrameCaptionButtonContainerView::NonClientHitTest(
    const gfx::Point& point) const {
  if (close_button_->visible() &&
      ConvertPointToViewAndHitTest(this, close_button_, point)) {
    return HTCLOSE;
  } else if (size_button_->visible() &&
             ConvertPointToViewAndHitTest(this, size_button_, point)) {
    return HTMAXBUTTON;
  } else if (minimize_button_->visible() &&
             ConvertPointToViewAndHitTest(this, minimize_button_, point)) {
    return HTMINBUTTON;
  }
  return HTNOWHERE;
}

gfx::Size FrameCaptionButtonContainerView::GetPreferredSize() {
  int width = 0;
  bool first_visible = true;
  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    if (!child->visible())
      continue;

    width += child_at(i)->GetPreferredSize().width();
    if (!first_visible)
      width += kDistanceBetweenButtons;
    first_visible = false;
  }
  return gfx::Size(width, close_button_->GetPreferredSize().height());
}

void FrameCaptionButtonContainerView::Layout() {
  int x = 0;
  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    if (!child->visible())
      continue;

    gfx::Size size = child->GetPreferredSize();
    child->SetBounds(x, 0, size.width(), size.height());
    x += size.width() + kDistanceBetweenButtons;
  }
}

const char* FrameCaptionButtonContainerView::GetClassName() const {
  return kViewClassName;
}

void FrameCaptionButtonContainerView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  // The alternate button style does not paint the button separator.
  if (!switches::UseAlternateFrameCaptionButtonStyle()) {
    // We should have at most two visible buttons. The button separator is
    // always painted underneath the close button regardless of whether a
    // button other than the close button is visible.
    gfx::Rect divider(close_button_->bounds().origin(),
                      button_separator_.size());
    canvas->DrawImageInt(button_separator_,
                         GetMirroredXForRect(divider),
                         divider.y());
  }
}

void FrameCaptionButtonContainerView::SetButtonIcon(FrameCaptionButton* button,
                                                    CaptionButtonIcon icon,
                                                    Animate animate) {
  // The early return is dependant on |animate| because callers use
  // SetButtonIcon() with ANIMATE_NO to progress |button|'s crossfade animation
  // to the end.
  if (button->icon() == icon &&
      (animate == ANIMATE_YES || !button->IsAnimatingImageSwap())) {
    return;
  }

  FrameCaptionButton::Animate fcb_animate = (animate == ANIMATE_YES) ?
      FrameCaptionButton::ANIMATE_YES : FrameCaptionButton::ANIMATE_NO;
  std::map<CaptionButtonIcon, ButtonIconIds>::const_iterator it =
      button_icon_id_map_.find(icon);
  if (it != button_icon_id_map_.end()) {
    button->SetImages(icon,
                      fcb_animate,
                      it->second.normal_image_id,
                      it->second.hovered_image_id,
                      it->second.pressed_image_id);
  }
}

void FrameCaptionButtonContainerView::ButtonPressed(views::Button* sender,
                                                    const ui::Event& event) {
  // When shift-clicking, slow down animations for visual debugging.
  // We used to do this via an event filter that looked for the shift key being
  // pressed but this interfered with several normal keyboard shortcuts.
  scoped_ptr<ui::ScopedAnimationDurationScaleMode> slow_duration_mode;
  if (event.IsShiftDown()) {
    slow_duration_mode.reset(new ui::ScopedAnimationDurationScaleMode(
        ui::ScopedAnimationDurationScaleMode::SLOW_DURATION));
  }

  // Abort any animations of the button icons.
  SetButtonsToNormal(ANIMATE_NO);

  ash::UserMetricsAction action =
      ash::UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_MINIMIZE;
  if (sender == minimize_button_) {
    frame_->Minimize();
  } else if (sender == size_button_) {
    if (frame_->IsFullscreen()) { // Can be clicked in immersive fullscreen.
      frame_->SetFullscreen(false);
      action = ash::UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_EXIT_FULLSCREEN;
    } else if (frame_->IsMaximized()) {
      frame_->Restore();
      action = ash::UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_RESTORE;
    } else {
      frame_->Maximize();
      action = ash::UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_MAXIMIZE;
    }
  } else if(sender == close_button_) {
    frame_->Close();
    action = ash::UMA_WINDOW_CLOSE_BUTTON_CLICK;
  } else {
    return;
  }
  ash::Shell::GetInstance()->metrics()->RecordUserMetricsAction(action);
}

bool FrameCaptionButtonContainerView::IsMinimizeButtonVisible() const {
  return minimize_button_->visible();
}

void FrameCaptionButtonContainerView::SetButtonsToNormal(Animate animate) {
  SetButtonIcons(CAPTION_BUTTON_ICON_MINIMIZE, CAPTION_BUTTON_ICON_CLOSE,
      animate);
  minimize_button_->SetState(views::Button::STATE_NORMAL);
  size_button_->SetState(views::Button::STATE_NORMAL);
  close_button_->SetState(views::Button::STATE_NORMAL);
}

void FrameCaptionButtonContainerView::SetButtonIcons(
    CaptionButtonIcon minimize_button_icon,
    CaptionButtonIcon close_button_icon,
    Animate animate) {
  SetButtonIcon(minimize_button_, minimize_button_icon, animate);
  SetButtonIcon(close_button_, close_button_icon, animate);
}

const FrameCaptionButton*
FrameCaptionButtonContainerView::PressButtonAt(
    const gfx::Point& position_in_screen,
    const gfx::Insets& pressed_hittest_outer_insets) const {
  DCHECK(switches::UseAlternateFrameCaptionButtonStyle());
  gfx::Point position(position_in_screen);
  views::View::ConvertPointFromScreen(this, &position);

  FrameCaptionButton* buttons[] = {
    close_button_, size_button_, minimize_button_
  };
  FrameCaptionButton* pressed_button = NULL;
  for (size_t i = 0; i < arraysize(buttons); ++i) {
    FrameCaptionButton* button = buttons[i];
    if (!button->visible())
      continue;

    if (button->state() == views::Button::STATE_PRESSED) {
      gfx::Rect expanded_bounds = button->bounds();
      expanded_bounds.Inset(pressed_hittest_outer_insets);
      if (expanded_bounds.Contains(position)) {
        pressed_button = button;
        // Do not break in order to give preference to buttons which are
        // closer to |position_in_screen| than the currently pressed button.
        // TODO(pkotwicz): Make the caption buttons not overlap.
      }
    } else if (ConvertPointToViewAndHitTest(this, button, position)) {
      pressed_button = button;
      break;
    }
  }

  for (size_t i = 0; i < arraysize(buttons); ++i) {
    buttons[i]->SetState(buttons[i] == pressed_button ?
        views::Button::STATE_PRESSED : views::Button::STATE_NORMAL);
  }
  return pressed_button;
}

FrameCaptionButtonContainerView::ButtonIconIds::ButtonIconIds()
    : normal_image_id(-1),
      hovered_image_id(-1),
      pressed_image_id(-1) {
}

FrameCaptionButtonContainerView::ButtonIconIds::ButtonIconIds(int normal_id,
                                                              int hovered_id,
                                                              int pressed_id)
    : normal_image_id(normal_id),
      hovered_image_id(hovered_id),
      pressed_image_id(pressed_id) {
}

FrameCaptionButtonContainerView::ButtonIconIds::~ButtonIconIds() {
}

}  // namespace ash
