// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/link_disambiguation/link_disambiguation_popup.h"

#include "base/macros.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_event_details.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/controls/image_view.h"

class LinkDisambiguationPopup::ZoomBubbleView
    : public views::BubbleDialogDelegateView {
 public:
  ZoomBubbleView(views::Widget* top_level_widget,
                 const gfx::Rect& target_rect,
                 const gfx::ImageSkia* zoomed_skia_image,
                 LinkDisambiguationPopup* popup,
                 const base::Callback<void(ui::GestureEvent*)>& gesture_cb,
                 const base::Callback<void(ui::MouseEvent*)>& mouse_cb);

 private:
  // views::View overrides
  gfx::Size GetPreferredSize() const override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // views::WidgetObserver overrides
  void OnWidgetClosing(views::Widget* widget) override;

  // views::BubbleDialogDelegate overrides
  int GetDialogButtons() const override;

  const float scale_;
  const base::Callback<void(ui::GestureEvent*)> gesture_cb_;
  const base::Callback<void(ui::MouseEvent*)> mouse_cb_;
  LinkDisambiguationPopup* popup_;
  const gfx::Rect target_rect_;

  DISALLOW_COPY_AND_ASSIGN(ZoomBubbleView);
};

LinkDisambiguationPopup::ZoomBubbleView::ZoomBubbleView(
    views::Widget* top_level_widget,
    const gfx::Rect& target_rect,
    const gfx::ImageSkia* zoomed_skia_image,
    LinkDisambiguationPopup* popup,
    const base::Callback<void(ui::GestureEvent*)>& gesture_cb,
    const base::Callback<void(ui::MouseEvent*)>& mouse_cb)
    : BubbleDialogDelegateView(
          top_level_widget ? top_level_widget->GetContentsView() : nullptr,
          views::BubbleBorder::FLOAT),
      scale_(static_cast<float>(zoomed_skia_image->width()) /
          static_cast<float>(target_rect.width())),
      gesture_cb_(gesture_cb),
      mouse_cb_(mouse_cb),
      popup_(popup),
      target_rect_(target_rect) {
  views::ImageView* image_view = new views::ImageView();
  image_view->SetSize(zoomed_skia_image->size());
  image_view->SetImage(zoomed_skia_image);
  AddChildView(image_view);

  views::BubbleDialogDelegateView::CreateBubble(this);
}

gfx::Size LinkDisambiguationPopup::ZoomBubbleView::GetPreferredSize() const {
  return target_rect_.size();
}

void LinkDisambiguationPopup::ZoomBubbleView::OnMouseEvent(
    ui::MouseEvent* event) {
  // Transform mouse event back to coordinate system of the web content window
  // before providing to the callback.
  ui::MouseEvent xform_event(event->type(), gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), event->flags(),
                             event->changed_button_flags());
  gfx::PointF xform_location(
      (event->location().x() / scale_) + target_rect_.x(),
      (event->location().y() / scale_) + target_rect_.y());
  xform_event.set_location_f(xform_location);
  xform_event.set_root_location_f(xform_location);
  mouse_cb_.Run(&xform_event);
  event->SetHandled();

  // If user completed a click we can close the window.
  if (event->type() == ui::EventType::ET_MOUSE_RELEASED)
    GetWidget()->Close();
}

void LinkDisambiguationPopup::ZoomBubbleView::OnGestureEvent(
    ui::GestureEvent* event) {
  // If we receive gesture events that are outside of our bounds we close
  // ourselves, as perhaps the user has decided on a different part of the page.
  if (event->location().x() > bounds().width() ||
      event->location().y() > bounds().height()) {
    GetWidget()->Close();
    return;
  }

  // Scale the gesture event back to the size of the original |target_rect_|,
  // and then offset it to be relative to that |target_rect_| before sending
  // it back to the callback.
  gfx::PointF xform_location(
      (event->location().x() / scale_) + target_rect_.x(),
      (event->location().y() / scale_) + target_rect_.y());
  ui::GestureEventDetails xform_details(event->details());
  xform_details.set_bounding_box(gfx::RectF(
      (event->details().bounding_box().x() / scale_) + target_rect_.x(),
      (event->details().bounding_box().y() / scale_) + target_rect_.y(),
      event->details().bounding_box().width() / scale_,
      event->details().bounding_box().height() / scale_));
  ui::GestureEvent xform_event(xform_location.x(),
                               xform_location.y(),
                               event->flags(),
                               event->time_stamp(),
                               xform_details);
  gesture_cb_.Run(&xform_event);
  event->SetHandled();

  // If we completed a tap we close ourselves, as the web content will navigate
  // if the user hit a link.
  if (event->type() == ui::EventType::ET_GESTURE_TAP)
    GetWidget()->Close();
}

void LinkDisambiguationPopup::ZoomBubbleView::OnWidgetClosing(
    views::Widget* widget) {
  popup_->InvalidateBubbleView();
}

int LinkDisambiguationPopup::ZoomBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

LinkDisambiguationPopup::LinkDisambiguationPopup()
    : content_(NULL),
      view_(NULL) {
}

LinkDisambiguationPopup::~LinkDisambiguationPopup() {
  Close();
}

void LinkDisambiguationPopup::Show(
    views::Widget* top_level_widget,
    const SkBitmap& zoomed_bitmap,
    const gfx::Rect& target_rect,
    const gfx::NativeView content,
    const base::Callback<void(ui::GestureEvent*)>& gesture_cb,
    const base::Callback<void(ui::MouseEvent*)>& mouse_cb) {
  content_ = content;

  view_ = new ZoomBubbleView(
      top_level_widget,
      target_rect,
      gfx::Image::CreateFrom1xBitmap(zoomed_bitmap).ToImageSkia(),
      this,
      gesture_cb,
      mouse_cb);

  // Center the zoomed bubble over the target rectangle, constrained to the
  // work area in the current display. Since |target_rect| is provided in
  // |content_| coordinate system, we must convert it into Screen coordinates
  // for correct window positioning.
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(content_->GetRootWindow());
  gfx::Point target_screen(target_rect.x() + (target_rect.width() / 2),
      target_rect.y() + (target_rect.height() / 2));
  if (screen_position_client)
    screen_position_client->ConvertPointToScreen(content_, &target_screen);
  gfx::Rect window_bounds(
      target_screen.x() - (zoomed_bitmap.width() / 2),
      target_screen.y() - (zoomed_bitmap.height() / 2),
      zoomed_bitmap.width(),
      zoomed_bitmap.height());
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(content);
  window_bounds.AdjustToFit(display.work_area());
  view_->GetWidget()->SetBounds(window_bounds);
  view_->GetWidget()->Show();
}

void LinkDisambiguationPopup::Close() {
  if (view_ && view_->GetWidget())
    view_->GetWidget()->Close();
}

void LinkDisambiguationPopup::InvalidateBubbleView() {
  view_ = nullptr;
}
