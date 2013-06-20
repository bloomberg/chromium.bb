// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autocheckout_bubble_controller.h"

#include "components/autofill/core/browser/autofill_metrics.h"
#include "grit/generated_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect_conversions.h"

namespace autofill {

AutocheckoutBubbleController::AutocheckoutBubbleController(
    const gfx::RectF& anchor_rect,
    const gfx::NativeWindow& native_window,
    bool is_google_user,
    const base::Callback<void(bool)>& callback)
    : anchor_rect_(gfx::ToEnclosingRect(anchor_rect)),
      native_window_(native_window),
      is_google_user_(is_google_user),
      callback_(callback),
      metric_logger_(new AutofillMetrics),
      had_user_interaction_(false) {}

AutocheckoutBubbleController::~AutocheckoutBubbleController() {}

// static
base::string16 AutocheckoutBubbleController::AcceptText() {
  return l10n_util::GetStringUTF16(IDS_AUTOCHECKOUT_BUBBLE_ACCEPT);
}

// static
base::string16 AutocheckoutBubbleController::CancelText() {
  return l10n_util::GetStringUTF16(IDS_AUTOCHECKOUT_BUBBLE_CANCEL);
}

base::string16 AutocheckoutBubbleController::PromptText() {
  return l10n_util::GetStringUTF16(
      is_google_user_ ? IDS_AUTOCHECKOUT_BUBBLE_PROMPT_SIGNED_IN :
                        IDS_AUTOCHECKOUT_BUBBLE_PROMPT_NOT_SIGNED_IN);
}

gfx::Image AutocheckoutBubbleController::NormalImage() {
  if (!is_google_user_)
    return gfx::Image();

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetImageNamed(IDR_BUY_WITH_GOOGLE_BUTTON);
}

gfx::Image AutocheckoutBubbleController::HoverImage() {
  if (!is_google_user_)
    return gfx::Image();

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetImageNamed(IDR_BUY_WITH_GOOGLE_BUTTON_H);
}

gfx::Image AutocheckoutBubbleController::PressedImage() {
  if (!is_google_user_)
    return gfx::Image();

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetImageNamed(IDR_BUY_WITH_GOOGLE_BUTTON_P);
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
