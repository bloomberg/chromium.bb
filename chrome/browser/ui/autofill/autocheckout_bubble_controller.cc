// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autocheckout_bubble_controller.h"

#include "chrome/browser/autofill/autofill_metrics.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/rect_conversions.h"

namespace autofill {

AutocheckoutBubbleController::AutocheckoutBubbleController(
    const gfx::RectF& anchor_rect,
    const base::Closure& callback)
    : anchor_rect_(gfx::ToEnclosingRect(anchor_rect)),
      callback_(callback),
      metric_logger_(new AutofillMetrics),
      had_user_interaction_(false) {}

AutocheckoutBubbleController::~AutocheckoutBubbleController() {}

// static
string16 AutocheckoutBubbleController::AcceptText() {
  return l10n_util::GetStringUTF16(IDS_AUTOCHECKOUT_BUBBLE_ACCEPT);
}

// static
string16 AutocheckoutBubbleController::CancelText() {
  return l10n_util::GetStringUTF16(IDS_AUTOCHECKOUT_BUBBLE_CANCEL);
}

// static
string16 AutocheckoutBubbleController::PromptText() {
  return l10n_util::GetStringUTF16(IDS_AUTOCHECKOUT_BUBBLE_PROMPT);
}

void AutocheckoutBubbleController::BubbleAccepted() {
  had_user_interaction_ = true;
  metric_logger_->LogAutocheckoutBubbleMetric(AutofillMetrics::BUBBLE_ACCEPTED);
  callback_.Run();
}

void AutocheckoutBubbleController::BubbleCanceled() {
  had_user_interaction_ = true;
  metric_logger_->LogAutocheckoutBubbleMetric(
      AutofillMetrics::BUBBLE_DISMISSED);
}

void AutocheckoutBubbleController::BubbleCreated() const {
  metric_logger_->LogAutocheckoutBubbleMetric(AutofillMetrics::BUBBLE_CREATED);
}

void AutocheckoutBubbleController::BubbleDestroyed() const {
  if (!had_user_interaction_) {
    metric_logger_->LogAutocheckoutBubbleMetric(
        AutofillMetrics::BUBBLE_IGNORED);
  }
}

void AutocheckoutBubbleController::set_metric_logger(
    AutofillMetrics* metric_logger) {
  metric_logger_.reset(metric_logger);
}

}  // namespace autofill
