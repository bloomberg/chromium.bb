// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/ui/blimp_screen.h"

#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/display_observer.h"
#include "ui/gfx/geometry/size.h"

namespace blimp {
namespace engine {

namespace {

const int64_t kDisplayId = 1;
const int kNumDisplays = 1;

}  // namespace

BlimpScreen::BlimpScreen() : display_(kDisplayId) {}

BlimpScreen::~BlimpScreen() {}

void BlimpScreen::UpdateDisplayScaleAndSize(float scale,
                                            const gfx::Size& size) {
  if (scale == display_.device_scale_factor() &&
      size == display_.GetSizeInPixel()) {
    return;
  }

  uint32_t metrics = gfx::DisplayObserver::DISPLAY_METRIC_NONE;
  if (scale != display_.device_scale_factor())
    metrics |= gfx::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;

  if (size != display_.GetSizeInPixel())
    metrics |= gfx::DisplayObserver::DISPLAY_METRIC_BOUNDS;

  display_.SetScaleAndBounds(scale, gfx::Rect(size));

  FOR_EACH_OBSERVER(gfx::DisplayObserver, observers_,
                    OnDisplayMetricsChanged(display_, metrics));
}

gfx::Point BlimpScreen::GetCursorScreenPoint() {
  return gfx::Point();
}

gfx::NativeWindow BlimpScreen::GetWindowUnderCursor() {
  NOTIMPLEMENTED();
  return gfx::NativeWindow(nullptr);
}

gfx::NativeWindow BlimpScreen::GetWindowAtScreenPoint(const gfx::Point& point) {
  return window_tree_host_
             ? window_tree_host_->window()->GetTopWindowContainingPoint(point)
             : gfx::NativeWindow(nullptr);
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

void BlimpScreen::AddObserver(gfx::DisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void BlimpScreen::RemoveObserver(gfx::DisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace engine
}  // namespace blimp
