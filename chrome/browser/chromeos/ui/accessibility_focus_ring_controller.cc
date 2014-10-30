// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/accessibility_focus_ring_controller.h"

#include "ash/display/display_controller.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/ui/focus_ring_layer.h"
#include "ui/gfx/screen.h"

namespace chromeos {

namespace {

// The number of pixels the focus ring is outset from the object it outlines,
// which also determines the border radius of the rounded corners.
// TODO(dmazzoni): take display resolution into account.
const int kAccessibilityFocusRingMargin = 7;

// Time to transition between one location and the next.
const int kTransitionTimeMilliseconds = 300;

// A Region is an unordered collection of Rects that maintains its
// bounding box. Used in the middle of an algorithm that groups
// adjacent and overlapping rects.
struct Region {
  explicit Region(gfx::Rect initial_rect) {
    bounds = initial_rect;
    rects.push_back(initial_rect);
  }
  gfx::Rect bounds;
  std::vector<gfx::Rect> rects;
};

}  // namespace

// static
AccessibilityFocusRingController*
    AccessibilityFocusRingController::GetInstance() {
  return Singleton<AccessibilityFocusRingController>::get();
}

AccessibilityFocusRingController::AccessibilityFocusRingController()
    : compositor_(NULL) {
}

AccessibilityFocusRingController::~AccessibilityFocusRingController() {
}

void AccessibilityFocusRingController::SetFocusRing(
    const std::vector<gfx::Rect>& rects) {
  rects_ = rects;
  Update();
}

void AccessibilityFocusRingController::Update() {
  previous_rings_.swap(rings_);
  rings_.clear();
  RectsToRings(rects_, &rings_);
  layers_.resize(rings_.size());
  for (size_t i = 0; i < rings_.size(); ++i) {
    if (!layers_[i])
      layers_[i].reset(new AccessibilityFocusRingLayer(this));

    if (i > 0) {
      // Focus rings other than the first one don't animate.
      layers_[i]->Set(rings_[i]);
      continue;
    }

    gfx::Rect bounds = rings_[0].GetBounds();
    gfx::Display display =
      gfx::Screen::GetNativeScreen()->GetDisplayMatching(bounds);
    aura::Window* root_window = ash::Shell::GetInstance()->display_controller()
        ->GetRootWindowForDisplayId(display.id());
    ui::Compositor* compositor = root_window->layer()->GetCompositor();
    if (!compositor || root_window != layers_[0]->root_window()) {
      layers_[0]->Set(rings_[0]);
      if (compositor_ && compositor_->HasAnimationObserver(this)) {
        compositor_->RemoveAnimationObserver(this);
        compositor_ = NULL;
      }
      continue;
    }

    focus_change_time_ = base::TimeTicks::Now();
    if (!compositor->HasAnimationObserver(this)) {
      compositor_ = compositor;
      compositor_->AddAnimationObserver(this);
    }
  }
}

void AccessibilityFocusRingController::RectsToRings(
    const std::vector<gfx::Rect>& src_rects,
    std::vector<AccessibilityFocusRing>* rings) const {
  if (src_rects.empty())
    return;

  // Give all of the rects a margin.
  std::vector<gfx::Rect> rects;
  rects.resize(src_rects.size());
  for (size_t i = 0; i < src_rects.size(); ++i) {
    rects[i] = src_rects[i];
    rects[i].Inset(-GetMargin(), -GetMargin());
  }

  // Split the rects into contiguous regions.
  std::vector<Region> regions;
  regions.push_back(Region(rects[0]));
  for (size_t i = 1; i < rects.size(); ++i) {
    bool found = false;
    for (size_t j = 0; j < regions.size(); ++j) {
      if (Intersects(rects[i], regions[j].bounds)) {
        regions[j].rects.push_back(rects[i]);
        regions[j].bounds.Union(rects[i]);
        found = true;
      }
    }
    if (!found) {
      regions.push_back(Region(rects[i]));
    }
  }

  // Keep merging regions that intersect.
  // TODO(dmazzoni): reduce the worst-case complexity! This appears like
  // it could be O(n^3), make sure it's not in practice.
  bool merged;
  do {
    merged = false;
    for (size_t i = 0; i < regions.size() - 1 && !merged; ++i) {
      for (size_t j = i + 1; j < regions.size() && !merged; ++j) {
        if (Intersects(regions[i].bounds, regions[j].bounds)) {
          regions[i].rects.insert(regions[i].rects.end(),
                                  regions[j].rects.begin(),
                                  regions[j].rects.end());
          regions[i].bounds.Union(regions[j].bounds);
          regions.erase(regions.begin() + j);
          merged = true;
        }
      }
    }
  } while (merged);

  for (size_t i = 0; i < regions.size(); ++i) {
    std::sort(regions[i].rects.begin(), regions[i].rects.end());
    rings->push_back(RingFromSortedRects(regions[i].rects));
  }
}

int AccessibilityFocusRingController::GetMargin() const {
  return kAccessibilityFocusRingMargin;
}

// Given a vector of rects that all overlap, already sorted from top to bottom
// and left to right, split them into three shapes covering the top, middle,
// and bottom of a "paragraph shape".
//
// Input:
//
//                       +---+---+
//                       | 1 | 2 |
// +---------------------+---+---+
// |             3               |
// +--------+---------------+----+
// |    4   |         5     |
// +--------+---------------+--+
// |             6             |
// +---------+-----------------+
// |    7    |
// +---------+
//
// Output:
//
//                       +-------+
//                       |  Top  |
// +---------------------+-------+
// |                             |
// |                             |
// |           Middle            |
// |                             |
// |                             |
// +---------+-------------------+
// | Bottom  |
// +---------+
//
// When there's no clear "top" or "bottom" segment, split the overall rect
// evenly so that some of the area still fits into the "top" and "bottom"
// segments.
void AccessibilityFocusRingController::SplitIntoParagraphShape(
    const std::vector<gfx::Rect>& rects,
    gfx::Rect* top,
    gfx::Rect* middle,
    gfx::Rect* bottom) const {
  size_t n = rects.size();

  // Figure out how many rects belong in the top portion.
  gfx::Rect top_rect = rects[0];
  int top_middle = (top_rect.y() + top_rect.bottom()) / 2;
  size_t top_count = 1;
  while (top_count < n && rects[top_count].y() < top_middle) {
    top_rect.Union(rects[top_count]);
    top_middle = (top_rect.y() + top_rect.bottom()) / 2;
    top_count++;
  }

  // Figure out how many rects belong in the bottom portion.
  gfx::Rect bottom_rect = rects[n - 1];
  int bottom_middle = (bottom_rect.y() + bottom_rect.bottom()) / 2;
  size_t bottom_count = std::min(static_cast<size_t>(1), n - top_count);
  while (bottom_count + top_count < n &&
         rects[n - bottom_count - 1].bottom() > bottom_middle) {
    bottom_rect.Union(rects[n - bottom_count - 1]);
    bottom_middle = (bottom_rect.y() + bottom_rect.bottom()) / 2;
    bottom_count++;
  }

  // Whatever's left goes to the middle rect, but if there's no middle or
  // bottom rect, split the existing rects evenly to make one.
  gfx::Rect middle_rect;
  if (top_count + bottom_count < n) {
    middle_rect = rects[top_count];
    for (size_t i = top_count + 1; i < n - bottom_count; i++)
      middle_rect.Union(rects[i]);
  } else if (bottom_count > 0) {
    gfx::Rect enclosing_rect = top_rect;
    enclosing_rect.Union(bottom_rect);
    int middle_top = (top_rect.y() + top_rect.bottom() * 2) / 3;
    int middle_bottom = (bottom_rect.y() * 2 + bottom_rect.bottom()) / 3;
    top_rect.set_height(middle_top - top_rect.y());
    bottom_rect.set_height(bottom_rect.bottom() - middle_bottom);
    bottom_rect.set_y(middle_bottom);
    middle_rect = gfx::Rect(enclosing_rect.x(), middle_top,
                            enclosing_rect.width(), middle_bottom - middle_top);
  } else {
    int middle_top = (top_rect.y() * 2 + top_rect.bottom()) / 3;
    int middle_bottom = (top_rect.y() + top_rect.bottom() * 2) / 3;
    middle_rect = gfx::Rect(top_rect.x(), middle_top,
                            top_rect.width(), middle_bottom - middle_top);
    bottom_rect = gfx::Rect(
        top_rect.x(), middle_bottom,
        top_rect.width(), top_rect.bottom() - middle_bottom);
    top_rect.set_height(middle_top - top_rect.y());
  }

  if (middle_rect.y() > top_rect.bottom()) {
    middle_rect.set_height(
        middle_rect.height() + middle_rect.y() - top_rect.bottom());
    middle_rect.set_y(top_rect.bottom());
  }

  if (middle_rect.bottom() < bottom_rect.y()) {
    middle_rect.set_height(bottom_rect.y() - middle_rect.y());
  }

  *top = top_rect;
  *middle = middle_rect;
  *bottom = bottom_rect;
}

AccessibilityFocusRing AccessibilityFocusRingController::RingFromSortedRects(
    const std::vector<gfx::Rect>& rects) const {
  if (rects.size() == 1)
    return AccessibilityFocusRing::CreateWithRect(rects[0], GetMargin());

  gfx::Rect top;
  gfx::Rect middle;
  gfx::Rect bottom;
  SplitIntoParagraphShape(rects, &top, &middle, &bottom);

  return AccessibilityFocusRing::CreateWithParagraphShape(
      top, middle, bottom, GetMargin());
}

bool AccessibilityFocusRingController::Intersects(
  const gfx::Rect& r1, const gfx::Rect& r2) const {
  int slop = GetMargin();
  return (r2.x() <= r1.right() + slop &&
          r2.right() >= r1.x() - slop &&
          r2.y() <= r1.bottom() + slop &&
          r2.bottom() >= r1.y() - slop);
}

void AccessibilityFocusRingController::OnDeviceScaleFactorChanged() {
  Update();
}

void AccessibilityFocusRingController::OnAnimationStep(
    base::TimeTicks timestamp) {
  if (rings_.empty())
    return;

  CHECK(compositor_);
  CHECK(!rings_.empty());
  CHECK(!layers_.empty());
  CHECK(layers_[0]);

  // It's quite possible for the first 1 or 2 animation frames to be
  // for a timestamp that's earlier than the time we received the
  // focus change, so we just treat those as a delta of zero.
  if (timestamp < focus_change_time_)
    timestamp = focus_change_time_;

  base::TimeDelta delta = timestamp - focus_change_time_;
  base::TimeDelta transition_time =
      base::TimeDelta::FromMilliseconds(kTransitionTimeMilliseconds);
  if (delta >= transition_time) {
    layers_[0]->Set(rings_[0]);
    compositor_->RemoveAnimationObserver(this);
    compositor_ = NULL;
    return;
  }

  double fraction = delta.InSecondsF() / transition_time.InSecondsF();

  // Ease-in effect.
  fraction = pow(fraction, 0.3);

  layers_[0]->Set(AccessibilityFocusRing::Interpolate(
      previous_rings_[0], rings_[0], fraction));
}

}  // namespace chromeos
