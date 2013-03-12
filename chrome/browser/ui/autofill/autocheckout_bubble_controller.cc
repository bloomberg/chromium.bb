// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autocheckout_bubble_controller.h"

#include "components/autofill/browser/autofill_metrics.h"
#include "grit/generated_resources.h"
#include "ui/gfx/rect_conversions.h"

namespace autofill {

AutocheckoutBubbleController::AutocheckoutBubbleController(
    const gfx::RectF& anchor_rect,
    const gfx::NativeView& native_view,
    const base::Callback<void(bool)>& callback)
    : anchor_rect_(gfx::ToEnclosingRect(anchor_rect)),
      native_view_(native_view),
      callback_(callback),
      metric_logger_(new AutofillMetrics),
      had_user_interaction_(false) {}

AutocheckoutBubbleController::~AutocheckoutBubbleController() {}

// static
int AutocheckoutBubbleController::AcceptTextID() {
  return IDS_AUTOCHECKOUT_BUBBLE_ACCEPT;
}

// static
int AutocheckoutBubbleController::CancelTextID() {
  return IDS_AUTOCHECKOUT_BUBBLE_CANCEL;
}

// static
int AutocheckoutBubbleController::PromptTextID() {
  return IDS_AUTOCHECKOUT_BUBBLE_PROMPT;
}

void AutocheckoutBubbleController::BubbleAccepted() {
  had_user_interaction_ = true;
  metric_logger_->LogAutocheckoutBubbleMetric(AutofillMetrics::BUBBLE_ACCEPTED);
  callback_.Run(true);
}

void AutocheckoutBubbleController::BubbleCanceled() {
  had_user_interaction_ = true;
  metric_logger_->LogAutocheckoutBubbleMetric(
      AutofillMetrics::BUBBLE_DISMISSED);
  callback_.Run(false);
}

void AutocheckoutBubbleController::BubbleCreated() const {
  metric_logger_->LogAutocheckoutBubbleMetric(AutofillMetrics::BUBBLE_CREATED);
}

void AutocheckoutBubbleController::BubbleDestroyed() const {
  if (!had_user_interaction_) {
    metric_logger_->LogAutocheckoutBubbleMetric(
        AutofillMetrics::BUBBLE_IGNORED);
    callback_.Run(false);
  }
}

void AutocheckoutBubbleController::set_metric_logger(
    AutofillMetrics* metric_logger) {
  metric_logger_.reset(metric_logger);
}

}  // namespace autofill
