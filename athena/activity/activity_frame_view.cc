// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/activity_frame_view.h"

#include <algorithm>
#include <vector>

#include "athena/activity/public/activity_view_model.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace athena {

////////////////////////////////////////////////////////////////////////////////
// FrameViewAthena, public:

// static
const char ActivityFrameView::kViewClassName[] = "ActivityFrameView";

ActivityFrameView::ActivityFrameView(views::Widget* frame,
                                     ActivityViewModel* view_model)
    : frame_(frame), view_model_(view_model), title_(new views::Label) {
  title_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  const gfx::FontList& font_list = title_->font_list();
  title_->SetFontList(font_list.Derive(1, gfx::Font::BOLD));
  title_->SetEnabledColor(SK_ColorBLACK);
  AddChildView(title_);
}

ActivityFrameView::~ActivityFrameView() {
}

////////////////////////////////////////////////////////////////////////////////
// ActivityFrameView, views::NonClientFrameView overrides:

gfx::Rect ActivityFrameView::GetBoundsForClientView() const {
  gfx::Rect client_bounds = bounds();
  client_bounds.Inset(0, NonClientTopBorderHeight(), 0, 0);
  return client_bounds;
}

gfx::Rect ActivityFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Rect window_bounds = client_bounds;
  window_bounds.Inset(0, -NonClientTopBorderHeight(), 0, 0);
  return window_bounds;
}

int ActivityFrameView::NonClientHitTest(const gfx::Point& point) {
  return 0;
}

void ActivityFrameView::GetWindowMask(const gfx::Size& size,
                                      gfx::Path* window_mask) {
}

void ActivityFrameView::ResetWindowControls() {
}

void ActivityFrameView::UpdateWindowIcon() {
}

void ActivityFrameView::UpdateWindowTitle() {
  SkColor bgcolor = view_model_->GetRepresentativeColor();
  title_->set_background(views::Background::CreateSolidBackground(bgcolor));
  title_->SetBackgroundColor(bgcolor);
  title_->SetText(frame_->widget_delegate()->GetWindowTitle());
}

////////////////////////////////////////////////////////////////////////////////
// ActivityFrameView, views::View overrides:

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
  title_->SetBounds(0, 0, width(), NonClientTopBorderHeight());
}

////////////////////////////////////////////////////////////////////////////////
// ActivityFrameView, private:

int ActivityFrameView::NonClientTopBorderHeight() const {
  return frame_->IsFullscreen() ? 0 : title_->GetPreferredSize().height();
}

}  // namespace ash
