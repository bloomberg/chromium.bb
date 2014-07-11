// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/test/sample_activity.h"

#include "ui/views/background.h"
#include "ui/views/view.h"

namespace athena {
namespace test {

SampleActivity::SampleActivity(SkColor color,
                               SkColor contents_color,
                               const base::string16& title)
    : color_(color),
      contents_color_(contents_color),
      title_(title),
      contents_view_(NULL) {
}

SampleActivity::~SampleActivity() {
}

athena::ActivityViewModel* SampleActivity::GetActivityViewModel() {
  return this;
}

void SampleActivity::Init() {
}

SkColor SampleActivity::GetRepresentativeColor() const {
  return color_;
}

base::string16 SampleActivity::GetTitle() const {
  return title_;
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

}  // namespace test
}  // namespace athena
