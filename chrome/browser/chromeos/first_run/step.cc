// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/step.h"

#include <cctype>

#include "ash/first_run/first_run_helper.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/ui/webui/chromeos/first_run/first_run_actor.h"
#include "ui/gfx/size.h"
#include "ui/views/widget/widget.h"

namespace {

// Converts from "with-dashes-names" to "WithDashesNames".
std::string ToCamelCase(const std::string& name) {
  std::string result;
  bool next_to_upper = true;
  for (size_t i = 0; i < name.length(); ++i) {
    if (name[i] == '-') {
      next_to_upper = true;
    } else if (next_to_upper) {
      result.push_back(std::toupper(name[i]));
      next_to_upper = false;
    } else {
      result.push_back(name[i]);
    }
  }
  return result;
}

}  // namespace

namespace chromeos {
namespace first_run {

Step::Step(const std::string& name,
           ash::FirstRunHelper* shell_helper,
           FirstRunActor* actor)
    : name_(name),
      shell_helper_(shell_helper),
      actor_(actor) {
}

Step::~Step() { RecordCompletion(); }

void Step::Show() {
  show_time_ = base::Time::Now();
  DoShow();
}

void Step::OnBeforeHide() {
  actor()->RemoveBackgroundHoles();
  DoOnBeforeHide();
}

void Step::OnAfterHide() {
  RecordCompletion();
  DoOnAfterHide();
}

gfx::Size Step::GetOverlaySize() const {
  return shell_helper()->GetOverlayWidget()->GetWindowBoundsInScreen().size();
}

void Step::RecordCompletion() {
  if (show_time_.is_null())
    return;
  std::string histogram_name =
      "CrosFirstRun.TimeSpentOnStep" + ToCamelCase(name());
  // Equivalent to using UMA_HISTOGRAM_CUSTOM_TIMES with 50 buckets on range
  // [100ms, 3 min.]. UMA_HISTOGRAM_CUSTOM_TIMES can not be used here, because
  // |histogram_name| is calculated dynamically and changes from call to call.
  base::HistogramBase* histogram = base::Histogram::FactoryTimeGet(
      histogram_name,
      base::TimeDelta::FromMilliseconds(100),
      base::TimeDelta::FromMinutes(3),
      50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddTime(base::Time::Now() - show_time_);
  show_time_ = base::Time();
}

}  // namespace first_run
}  // namespace chromeos

