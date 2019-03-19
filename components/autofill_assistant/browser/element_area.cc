// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/element_area.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/web_controller.h"

namespace autofill_assistant {

namespace {
// Waiting period between two checks.
static constexpr base::TimeDelta kCheckDelay =
    base::TimeDelta::FromMilliseconds(100);
}  // namespace

ElementArea::ElementArea(ScriptExecutorDelegate* delegate)
    : delegate_(delegate), scheduled_update_(false), weak_ptr_factory_(this) {
  DCHECK(delegate_);
}

ElementArea::~ElementArea() = default;

void ElementArea::Clear() {
  rectangles_.clear();
  ReportUpdate();
}

void ElementArea::SetFromProto(const ElementAreaProto& proto) {
  rectangles_.clear();
  for (const auto& rectangle_proto : proto.rectangles()) {
    rectangles_.emplace_back();
    Rectangle& rectangle = rectangles_.back();
    rectangle.full_width = rectangle_proto.full_width();
    DVLOG(3) << "Touchable Rectangle"
             << (rectangle.full_width ? " (full_width)" : "") << ":";
    for (const auto& element_proto : rectangle_proto.elements()) {
      rectangle.positions.emplace_back();
      ElementPosition& position = rectangle.positions.back();
      position.selector = Selector(element_proto);
      DVLOG(3) << "  " << position.selector;
    }
  }
  ReportUpdate();

  if (rectangles_.empty())
    return;

  if (!scheduled_update_) {
    // Check once and schedule regular updates.
    scheduled_update_ = true;
    KeepUpdatingElementPositions();
  } else {
    // If regular updates are already scheduled, just force a check of position
    // right away and keep running the scheduled updates.
    UpdatePositions();
  }
}

void ElementArea::UpdatePositions() {
  if (rectangles_.empty())
    return;

  for (auto& rectangle : rectangles_) {
    for (auto& position : rectangle.positions) {
      // To avoid reporting partial rectangles, all element positions become
      // pending at the same time.
      position.pending_update = true;
    }
    for (auto& position : rectangle.positions) {
      delegate_->GetWebController()->GetElementPosition(
          position.selector,
          base::BindOnce(&ElementArea::OnGetElementPosition,
                         weak_ptr_factory_.GetWeakPtr(), position.selector));
    }
  }
}

void ElementArea::GetRectangles(std::vector<RectF>* area) {
  for (auto& rectangle : rectangles_) {
    area->emplace_back();
    rectangle.FillRect(&area->back());
  }
}

ElementArea::ElementPosition::ElementPosition() = default;
ElementArea::ElementPosition::ElementPosition(const ElementPosition& orig) =
    default;
ElementArea::ElementPosition::~ElementPosition() = default;

ElementArea::Rectangle::Rectangle() = default;
ElementArea::Rectangle::Rectangle(const Rectangle& orig) = default;
ElementArea::Rectangle::~Rectangle() = default;

bool ElementArea::Rectangle::IsPending() const {
  for (const auto& position : positions) {
    if (position.pending_update)
      return true;
  }
  return false;
}

void ElementArea::Rectangle::FillRect(RectF* rect) const {
  bool has_first_rect = false;
  for (const auto& position : positions) {
    if (position.rect.empty())
      continue;

    if (!has_first_rect) {
      *rect = position.rect;
      has_first_rect = true;
      continue;
    }
    rect->top = std::min(rect->top, position.rect.top);
    rect->bottom = std::max(rect->bottom, position.rect.bottom);
    rect->left = std::min(rect->left, position.rect.left);
    rect->right = std::max(rect->right, position.rect.right);
  }
  if (has_first_rect && full_width) {
    rect->left = 0.0;
    rect->right = 1.0;
  }
  return;
}

void ElementArea::KeepUpdatingElementPositions() {
  if (rectangles_.empty()) {
    scheduled_update_ = false;
    return;
  }

  UpdatePositions();
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ElementArea::KeepUpdatingElementPositions,
                     weak_ptr_factory_.GetWeakPtr()),
      kCheckDelay);
}

void ElementArea::OnGetElementPosition(const Selector& selector,
                                       bool found,
                                       const RectF& rect) {
  // found == false, has all coordinates set to 0.0, which clears the area.
  bool updated = false;
  for (auto& rectangle : rectangles_) {
    for (auto& position : rectangle.positions) {
      if (selector == position.selector) {
        position.pending_update = false;
        position.rect = rect;
        updated = true;
      }
    }
  }
  if (updated) {
    ReportUpdate();
  }
  // If the set of elements has changed, the given selector will not be found in
  // rectangles_. This is fine.
}

void ElementArea::ReportUpdate() {
  if (!on_update_)
    return;

  for (const auto& rectangle : rectangles_) {
    if (rectangle.IsPending()) {
      // We don't have everything we need yet
      return;
    }
  }

  std::vector<RectF> area;
  GetRectangles(&area);
  on_update_.Run(area);
}

}  // namespace autofill_assistant
