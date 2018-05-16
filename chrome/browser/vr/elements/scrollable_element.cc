// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/scrollable_element.h"

#include "base/numerics/ranges.h"
#include "third_party/blink/public/platform/web_gesture_event.h"

namespace vr {

namespace {

constexpr float kScrollScaleFactor = 1.0f / 400.0f;
constexpr float kFloatTolerance = FLT_EPSILON * 2.0f;

bool SizeEquals(const gfx::SizeF& a, const gfx::SizeF& b) {
  return base::IsApproximatelyEqual(a.width(), b.width(), kFloatTolerance) &&
         base::IsApproximatelyEqual(a.height(), b.height(), kFloatTolerance);
}

}  // namespace

ScrollableElement::ScrollableElement(Orientation orientation)
    : orientation_(orientation) {
  set_scrollable(true);
  set_clip_descendants(true);
  set_bounds_contain_children(true);

  auto inner_element = std::make_unique<UiElement>();
  inner_element->set_bounds_contain_children(true);
  inner_element_ = inner_element.get();
  AddChild(std::move(inner_element));
}

ScrollableElement::~ScrollableElement() = default;

void ScrollableElement::SetMaxSpan(float span) {
  DCHECK(span > 0.0f);
  max_span_ = span;
  auto size = inner_element_->size();
  if (orientation_ == kHorizontal) {
    size.set_width(std::min(size.width(), span));
  } else {
    size.set_height(std::min(size.height(), span));
  }
  SetSize(size.width(), size.height());
}

void ScrollableElement::SetScrollAnchoring(LayoutAlignment anchoring) {
  scrolling_anchoring_ = anchoring;
  SetInitialScroll();
}

float ScrollableElement::ComputeScrollSpan() const {
  float scroll_span;
  if (orientation_ == kVertical) {
    scroll_span = inner_element_->size().height() - size().height();
  } else {
    scroll_span = inner_element_->size().width() - size().width();
  }
  return std::max(scroll_span, 0.0f);
}

void ScrollableElement::SetInitialScroll() {
  float half_scroll_span = ComputeScrollSpan() / 2.0f;
  if (scrolling_anchoring_ == BOTTOM || scrolling_anchoring_ == LEFT) {
    scroll_offset_ = half_scroll_span;
  } else if (scrolling_anchoring_ == TOP || scrolling_anchoring_ == RIGHT) {
    scroll_offset_ = -half_scroll_span;
  } else {
    scroll_offset_ = 0.0f;
  }
  if (orientation_ == kVertical) {
    inner_element_->SetTranslate(0.0f, scroll_offset_, 0.0f);
  } else {
    inner_element_->SetTranslate(scroll_offset_, 0.0f, 0.0f);
  }
}

gfx::RectF ScrollableElement::ComputeContributingChildrenBounds() {
  auto size = inner_element_->size();
  if (orientation_ == kHorizontal) {
    size.set_width(std::min(size.width(), max_span_));
  } else {
    size.set_height(std::min(size.height(), max_span_));
  }
  return gfx::RectF(size);
}

void ScrollableElement::LayOutNonContributingChildren() {
  if (!SizeEquals(inner_size_, inner_element_->size())) {
    inner_size_ = inner_element_->size();
    SetInitialScroll();
  }
}

void ScrollableElement::AddScrollingChild(std::unique_ptr<UiElement> child) {
  inner_element_->AddChild(std::move(child));
}

void ScrollableElement::OnScrollBegin(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& position) {
  cached_transition_ = animation().transition();
  animation().set_transition(Transition());
}

void ScrollableElement::OnScrollUpdate(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& position) {
  const auto& update = gesture->data.scroll_update;
  float half_scroll_span = ComputeScrollSpan() / 2.0f;
  if (orientation_ == kHorizontal) {
    scroll_offset_ -= update.delta_x * kScrollScaleFactor;
    scroll_offset_ =
        base::ClampToRange(scroll_offset_, -half_scroll_span, half_scroll_span);
    inner_element_->SetTranslate(scroll_offset_, 0.0f, 0.0f);
  } else {
    scroll_offset_ -= update.delta_y * kScrollScaleFactor;
    scroll_offset_ =
        base::ClampToRange(scroll_offset_, -half_scroll_span, half_scroll_span);
    inner_element_->SetTranslate(0.0f, scroll_offset_, 0.0f);
  }
}

void ScrollableElement::OnScrollEnd(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& position) {
  animation().set_transition(cached_transition_);
}

bool ScrollableElement::OnBeginFrame(const gfx::Transform&) {
  // TODO(acondor): Remove this calculation of dirtiness
  // (https://crbug.com/843326).
  if (scroll_offset_ != last_scroll_offset_) {
    last_scroll_offset_ = scroll_offset_;
    return true;
  }
  return false;
}

}  // namespace vr
