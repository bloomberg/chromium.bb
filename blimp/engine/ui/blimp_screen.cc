// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/ui/blimp_screen.h"

namespace blimp {
namespace engine {

namespace {

const int64 kDisplayId = 1;
const float kDefaultScale = 1.0f;
const int kNumDisplays = 1;

}  // namespace

const int BlimpScreen::kDefaultDisplayWidth = 800;
const int BlimpScreen::kDefaultDisplayHeight = 600;

BlimpScreen::BlimpScreen() : display_(kDisplayId) {
  display_.SetScaleAndBounds(
      kDefaultScale, gfx::Rect(kDefaultDisplayWidth, kDefaultDisplayHeight));
}

BlimpScreen::~BlimpScreen() {}

void BlimpScreen::UpdateDisplaySize(const gfx::Size& size) {
  display_.SetScaleAndBounds(kDefaultScale, gfx::Rect(size));
}

gfx::Point BlimpScreen::GetCursorScreenPoint() {
  return gfx::Point();
}

gfx::NativeWindow BlimpScreen::GetWindowUnderCursor() {
  NOTIMPLEMENTED();
  return gfx::NativeWindow(nullptr);
}

gfx::NativeWindow BlimpScreen::GetWindowAtScreenPoint(const gfx::Point& point) {
  NOTIMPLEMENTED();
  return gfx::NativeWindow(nullptr);
}

int BlimpScreen::GetNumDisplays() const {
  return kNumDisplays;
}

std::vector<gfx::Display> BlimpScreen::GetAllDisplays() const {
  return std::vector<gfx::Display>(1, display_);
}

gfx::Display BlimpScreen::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  return display_;
}

gfx::Display BlimpScreen::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return display_;
}

gfx::Display BlimpScreen::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  return display_;
}

gfx::Display BlimpScreen::GetPrimaryDisplay() const {
  return display_;
}

void BlimpScreen::AddObserver(gfx::DisplayObserver* observer) {}

void BlimpScreen::RemoveObserver(gfx::DisplayObserver* observer) {}

}  // namespace engine
}  // namespace blimp
