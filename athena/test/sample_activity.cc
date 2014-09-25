// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/test/sample_activity.h"

#include "ui/gfx/image/image_skia.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace athena {
namespace test {

SampleActivity::SampleActivity(SkColor color,
                               SkColor contents_color,
                               const base::string16& title)
    : color_(color),
      contents_color_(contents_color),
      title_(title),
      contents_view_(NULL),
      current_state_(ACTIVITY_UNLOADED) {
}

SampleActivity::~SampleActivity() {
}

athena::ActivityViewModel* SampleActivity::GetActivityViewModel() {
  return this;
}

void SampleActivity::SetCurrentState(Activity::ActivityState state) {
  current_state_ = state;
}

Activity::ActivityState SampleActivity::GetCurrentState() {
  return current_state_;
}

bool SampleActivity::IsVisible() {
  return contents_view_ && contents_view_->IsDrawn();
}

Activity::ActivityMediaState SampleActivity::GetMediaState() {
  return Activity::ACTIVITY_MEDIA_STATE_NONE;
}

aura::Window* SampleActivity::GetWindow() {
  return !contents_view_ ? NULL
                         : contents_view_->GetWidget()->GetNativeWindow();
}

content::WebContents* SampleActivity::GetWebContents() {
  return NULL;
}

void SampleActivity::Init() {
}

SkColor SampleActivity::GetRepresentativeColor() const {
  return color_;
}

base::string16 SampleActivity::GetTitle() const {
  return title_;
}

gfx::ImageSkia SampleActivity::GetIcon() const {
  return gfx::ImageSkia();
}

bool SampleActivity::UsesFrame() const {
  return true;
}

views::View* SampleActivity::GetContentsView() {
  if (!contents_view_) {
    contents_view_ = new views::View;
    contents_view_->set_background(
        views::Background::CreateSolidBackground(contents_color_));
  }
  return contents_view_;
}

views::Widget* SampleActivity::CreateWidget() {
  return NULL;
}

gfx::ImageSkia SampleActivity::GetOverviewModeImage() {
  return gfx::ImageSkia();
}

void SampleActivity::PrepareContentsForOverview() {
}

void SampleActivity::ResetContentsView() {
}

}  // namespace test
}  // namespace athena
