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
static constexpr base::TimeDelta kCheckDelay =
    base::TimeDelta::FromMilliseconds(100);
}  // namespace

ElementArea::ElementArea(WebController* web_controller)
    : web_controller_(web_controller),
      scheduled_update_(false),
      weak_ptr_factory_(this) {
  DCHECK(web_controller_);
}

ElementArea::~ElementArea() = default;

void ElementArea::SetElements(const std::vector<Selector>& elements) {
  element_positions_.clear();

  for (const auto& selector : elements) {
    element_positions_.emplace_back();
    element_positions_.back().selector = selector;
  }
  ReportUpdate();

  if (element_positions_.empty())
    return;

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
    if (!position.rect.empty()) {
      return false;
    }
  }
  return true;
}

ElementArea::ElementPosition::ElementPosition() = default;
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
      kCheckDelay);
}

void ElementArea::OnGetElementPosition(const Selector& selector,
                                       bool found,
                                       const RectF& rect) {
  for (auto& position : element_positions_) {
    if (position.selector == selector) {
      // found == false, has all coordinates set to 0.0, which clears the area.
      position.rect = rect;
      ReportUpdate();
      return;
    }
  }
  // If the set of elements has changed, the given selector will not be found in
  // element_positions_. This is fine.
}

bool ElementArea::Contains(float x, float y) const {
  for (const auto& position : element_positions_) {
    if (position.rect.Contains(x, y)) {
      return true;
    }
  }
  return false;
}

void ElementArea::ReportUpdate() {
  if (!on_update_)
    return;

  std::vector<RectF> areas;
  for (auto& position : element_positions_) {
    if (!position.rect.empty()) {
      areas.emplace_back(position.rect);
    }
  }
  on_update_.Run(!element_positions_.empty(), areas);
}

}  // namespace autofill_assistant
