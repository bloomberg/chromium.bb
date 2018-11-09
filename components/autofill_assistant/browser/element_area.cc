// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/element_area.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/web_controller.h"

namespace autofill_assistant {

namespace {
// Waiting period between two checks.
static constexpr base::TimeDelta kCheckPeriod =
    base::TimeDelta::FromMilliseconds(100);
}  // namespace

ElementArea::ElementArea(WebController* web_controller)
    : web_controller_(web_controller),
      scheduled_update_(false),
      weak_ptr_factory_(this) {
  DCHECK(web_controller_);
}

ElementArea::~ElementArea() = default;

void ElementArea::SetElements(
    const std::vector<std::vector<std::string>>& elements) {
  element_positions_.clear();
  if (elements.empty())
    return;

  for (const auto& selector : elements) {
    element_positions_.emplace_back();
    element_positions_.back().selector = selector;
  }

  if (!scheduled_update_) {
    // Check once and schedule regular updates.
    scheduled_update_ = true;
    KeepUpdatingPositions();
  } else {
    // If regular updates are already scheduled, just force a check of position
    // right away and keep running the scheduled updates.
    UpdatePositions();
  }
}

void ElementArea::UpdatePositions() {
  if (element_positions_.empty())
    return;

  for (auto& position : element_positions_) {
    web_controller_->GetElementPosition(
        position.selector,
        base::BindOnce(&ElementArea::OnGetElementPosition,
                       weak_ptr_factory_.GetWeakPtr(), position.selector));
  }
}

bool ElementArea::IsEmpty() const {
  for (const auto& position : element_positions_) {
    if (position.is_not_empty()) {
      return false;
    }
  }
  return true;
}

ElementArea::ElementPosition::ElementPosition()
    : left(0.0), top(0.0), right(0.0), bottom(0.0) {}
ElementArea::ElementPosition::ElementPosition(const ElementPosition& orig) =
    default;
ElementArea::ElementPosition::~ElementPosition() = default;

void ElementArea::KeepUpdatingPositions() {
  if (element_positions_.empty()) {
    scheduled_update_ = false;
    return;
  }

  UpdatePositions();
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ElementArea::KeepUpdatingPositions,
                     weak_ptr_factory_.GetWeakPtr()),
      kCheckPeriod);
}

void ElementArea::OnGetElementPosition(const std::vector<std::string>& selector,
                                       bool found,
                                       float left,
                                       float top,
                                       float right,
                                       float bottom) {
  for (auto& position : element_positions_) {
    if (position.selector == selector) {
      // found == false, has all coordinates set to 0.0, which clears the area.
      position.left = left;
      position.top = top;
      position.right = right;
      position.bottom = bottom;
      return;
    }
  }
  // If the set of elements has changed, the given selector will not be found in
  // element_positions_. This is fine.
}

bool ElementArea::Contains(float x, float y) const {
  for (const auto& position : element_positions_) {
    if (position.is_not_empty() && x >= position.left && x <= position.right &&
        y >= position.top && y <= position.bottom) {
      return true;
    }
  }
  return false;
}

}  // namespace autofill_assistant
