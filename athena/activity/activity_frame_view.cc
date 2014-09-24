// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/activity_frame_view.h"

#include "athena/activity/public/activity_view_model.h"
#include "athena/wm/public/window_manager.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/client_view.h"

namespace athena {
namespace {

// The icon size.
const int kIconSize = 32;

// The distance between the icon and the title when the icon is visible.
const int kIconTitleSpacing = 10;

// The height of the top border necessary to display the title without the icon.
const int kDefaultTitleHeight = 25;

// The height of the top border in overview mode.
const int kOverviewTitleHeight = 55;

// The height of the top border for fullscreen and frameless activities in
// overview mode.
const int kOverviewShortTitleHeight = 30;

// The thickness of the left, right and bottom borders in overview mode.
const int kOverviewBorderThickness = 5;

}  // namespace

// static
const char ActivityFrameView::kViewClassName[] = "ActivityFrameView";

ActivityFrameView::ActivityFrameView(views::Widget* frame,
                                     ActivityViewModel* view_model)
    : frame_(frame),
      view_model_(view_model),
      title_(new views::Label),
      icon_(new views::ImageView),
      in_overview_(false) {
  title_->SetEnabledColor(SkColorSetA(SK_ColorBLACK, 0xe5));

  AddChildView(title_);
  AddChildView(icon_);

  UpdateWindowTitle();
  UpdateWindowIcon();

  WindowManager::Get()->AddObserver(this);
}

ActivityFrameView::~ActivityFrameView() {
  WindowManager::Get()->RemoveObserver(this);
}

gfx::Rect ActivityFrameView::GetBoundsForClientView() const {
  gfx::Rect client_bounds = bounds();
  client_bounds.Inset(NonClientBorderInsets());
  return client_bounds;
}

gfx::Rect ActivityFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Rect window_bounds = client_bounds;
  window_bounds.Inset(-NonClientBorderInsets());
  return window_bounds;
}

int ActivityFrameView::NonClientHitTest(const gfx::Point& point) {
  if (!bounds().Contains(point))
    return HTNOWHERE;
  int client_hit_test = frame_->client_view()->NonClientHitTest(point);
  if (client_hit_test != HTNOWHERE)
    return client_hit_test;
  int window_hit_test =
      GetHTComponentForFrame(point, 0, NonClientBorderThickness(), 0, 0, false);
  return (window_hit_test == HTNOWHERE) ? HTCAPTION : client_hit_test;
}

void ActivityFrameView::GetWindowMask(const gfx::Size& size,
                                      gfx::Path* window_mask) {
}

void ActivityFrameView::ResetWindowControls() {
}

void ActivityFrameView::UpdateWindowIcon() {
  // The activity has a frame in overview mode regardless of the value of
  // ActivityViewModel::UsesFrame().
  SkColor bgcolor = view_model_->GetRepresentativeColor();
  set_background(views::Background::CreateSolidBackground(bgcolor));
  title_->SetBackgroundColor(bgcolor);

  if (view_model_->UsesFrame())
    icon_->SetImage(view_model_->GetIcon());
  SchedulePaint();
}

void ActivityFrameView::UpdateWindowTitle() {
  if (!view_model_->UsesFrame())
    return;
  title_->SetText(frame_->widget_delegate()->GetWindowTitle());
  Layout();
}

void ActivityFrameView::SizeConstraintsChanged() {
}

gfx::Size ActivityFrameView::GetPreferredSize() const {
  gfx::Size pref = frame_->client_view()->GetPreferredSize();
  gfx::Rect bounds(0, 0, pref.width(), pref.height());
  return frame_->non_client_view()
      ->GetWindowBoundsForClientBounds(bounds)
      .size();
}

const char* ActivityFrameView::GetClassName() const {
  return kViewClassName;
}

void ActivityFrameView::Layout() {
  if (frame_->IsFullscreen() || !view_model_->UsesFrame()) {
    title_->SetVisible(false);
    icon_->SetVisible(false);
    return;
  }

  title_->SetVisible(true);
  icon_->SetVisible(in_overview_);

  gfx::Size preferred_title_size = title_->GetPreferredSize();
  int top_height = NonClientTopBorderHeight();
  int title_x = 0;
  if (in_overview_) {
    int edge = (top_height - kIconSize) / 2;
    icon_->SetBounds(edge, edge, kIconSize, kIconSize);

    title_x = icon_->bounds().right() + kIconTitleSpacing;
  } else {
    title_x = (width() - preferred_title_size.width()) / 2;
  }

  title_->SetBounds(title_x,
                    (top_height - preferred_title_size.height()) / 2,
                    preferred_title_size.width(),
                    preferred_title_size.height());
}

void ActivityFrameView::OnPaintBackground(gfx::Canvas* canvas) {
  View::OnPaintBackground(canvas);

  // Paint a border around the client view.
  gfx::Rect border_bounds = GetLocalBounds();
  border_bounds.Inset(NonClientBorderInsets());
  border_bounds.Inset(-1, -1, 0, 0);
  canvas->DrawRect(border_bounds, SkColorSetA(SK_ColorGRAY, 0x7f));
}

void ActivityFrameView::OnOverviewModeEnter() {
  view_model_->PrepareContentsForOverview();
  in_overview_ = true;
  InvalidateLayout();
  frame_->client_view()->InvalidateLayout();
  frame_->GetRootView()->Layout();
  SchedulePaint();
}

void ActivityFrameView::OnOverviewModeExit() {
  in_overview_ = false;
  InvalidateLayout();
  frame_->client_view()->InvalidateLayout();
  frame_->GetRootView()->Layout();
  SchedulePaint();
  view_model_->ResetContentsView();
}

void ActivityFrameView::OnSplitViewModeEnter() {
}

void ActivityFrameView::OnSplitViewModeExit() {
}

gfx::Insets ActivityFrameView::NonClientBorderInsets() const {
  int border_thickness = NonClientBorderThickness();
  return gfx::Insets(NonClientTopBorderHeight(),
                     border_thickness,
                     border_thickness,
                     border_thickness);
}

int ActivityFrameView::NonClientBorderThickness() const {
  return in_overview_ ? kOverviewBorderThickness : 0;
}

int ActivityFrameView::NonClientTopBorderHeight() const {
  if (frame_->IsFullscreen() || !view_model_->UsesFrame())
    return in_overview_ ? kOverviewShortTitleHeight : 0;
  return in_overview_ ? kOverviewTitleHeight : kDefaultTitleHeight;
}

}  // namespace athena
