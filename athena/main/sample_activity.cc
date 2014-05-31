// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/sample_activity.h"

#include "ui/aura/window.h"

SampleActivity::SampleActivity(SkColor color,
                               SkColor content_color,
                               const std::string& title)
    : color_(color), content_color_(content_color), title_(title) {
}

SampleActivity::~SampleActivity() {
}

athena::ActivityViewModel* SampleActivity::GetActivityViewModel() {
  return this;
}

SkColor SampleActivity::GetRepresentativeColor() {
  return color_;
}

std::string SampleActivity::GetTitle() {
  return title_;
}

aura::Window* SampleActivity::GetNativeWindow() {
  if (!window_) {
    window_.reset(new aura::Window(NULL));
    window_->Init(aura::WINDOW_LAYER_SOLID_COLOR);
    window_->layer()->SetColor(content_color_);
  }
  return window_.get();
}
