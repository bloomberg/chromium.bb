// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/tooltip_icon.h"

#include "base/basictypes.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/autofill/info_bubble.h"
#include "grit/theme_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/mouse_watcher_view_host.h"
#include "ui/views/painter.h"

namespace autofill {

namespace {

gfx::Insets GetPreferredInsets(const views::View* view) {
  gfx::Size pref_size = view->GetPreferredSize();
  gfx::Rect local_bounds = view->GetLocalBounds();
  gfx::Point origin = local_bounds.CenterPoint();
  origin.Offset(-pref_size.width() / 2, -pref_size.height() / 2);
  return gfx::Insets(origin.y(),
                     origin.x(),
                     local_bounds.bottom() - (origin.y() + pref_size.height()),
                     local_bounds.right() - (origin.x() + pref_size.width()));
}

// An info bubble with some extra positioning magic for tooltip icons.
class TooltipBubble : public InfoBubble {
 public:
  TooltipBubble(views::View* anchor, const base::string16& message)
      : InfoBubble(anchor, message) {}
  virtual ~TooltipBubble() {}

 protected:
  // InfoBubble:
  virtual gfx::Rect GetAnchorRect() const OVERRIDE {
    gfx::Rect bounds = views::BubbleDelegateView::GetAnchorRect();
    bounds.Inset(GetPreferredInsets(anchor()));
    return bounds;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TooltipBubble);
};

}  // namespace

TooltipIcon::TooltipIcon(const base::string16& tooltip)
    : tooltip_(tooltip),
      mouse_inside_(false),
      bubble_(NULL),
      observer_(this) {
  ChangeImageTo(IDR_AUTOFILL_TOOLTIP_ICON);
}

TooltipIcon::~TooltipIcon() {
  HideBubble();
}

// static
const char TooltipIcon::kViewClassName[] = "autofill/TooltipIcon";

const char* TooltipIcon::GetClassName() const {
  return TooltipIcon::kViewClassName;
}

void TooltipIcon::OnMouseEntered(const ui::MouseEvent& event) {
  mouse_inside_ = true;
  show_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(150), this,
                    &TooltipIcon::ShowBubble);
}

void TooltipIcon::OnMouseExited(const ui::MouseEvent& event) {
  show_timer_.Stop();
}

void TooltipIcon::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    ShowBubble();
    event->SetHandled();
  }
}

void TooltipIcon::GetAccessibleState(ui::AXViewState* state) {
  state->name = tooltip_;
}

void TooltipIcon::MouseMovedOutOfHost() {
  if (IsMouseHovered()) {
    mouse_watcher_->Start();
    return;
  }

  mouse_inside_ = false;
  HideBubble();
}

void TooltipIcon::ChangeImageTo(int idr) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  SetImage(rb.GetImageNamed(idr).ToImageSkia());
}

void TooltipIcon::ShowBubble() {
  if (bubble_)
    return;

  ChangeImageTo(IDR_AUTOFILL_TOOLTIP_ICON_H);

  bubble_ = new TooltipBubble(this, tooltip_);
  // When shown due to a gesture event, close on deactivate (i.e. don't use
  // "focusless").
  bubble_->set_can_activate(!mouse_inside_);

  bubble_->Show();
  observer_.Add(bubble_->GetWidget());

  if (mouse_inside_) {
    views::View* frame = bubble_->GetWidget()->non_client_view()->frame_view();
    scoped_ptr<views::MouseWatcherHost> host(
        new views::MouseWatcherViewHost(frame, gfx::Insets()));
    mouse_watcher_.reset(new views::MouseWatcher(host.release(), this));
    mouse_watcher_->Start();
  }
}

void TooltipIcon::HideBubble() {
  if (bubble_)
    bubble_->Hide();
}

void TooltipIcon::OnWidgetDestroyed(views::Widget* widget) {
  observer_.Remove(widget);

  ChangeImageTo(IDR_AUTOFILL_TOOLTIP_ICON);
  mouse_watcher_.reset();
  bubble_ = NULL;
}

}  // namespace autofill
