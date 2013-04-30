// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/autofill/autocheckout_bubble_controller.h"
#include "components/autofill/browser/autofill_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"

namespace autofill {
namespace {

class TestAutofillMetrics : public AutofillMetrics {
 public:
  TestAutofillMetrics()
      : metric_(NUM_BUBBLE_METRICS) {}
  virtual ~TestAutofillMetrics() {}
  virtual void LogAutocheckoutBubbleMetric(BubbleMetric metric) const OVERRIDE {
    metric_ = metric;
  }

  AutofillMetrics::BubbleMetric metric() const { return metric_; }

 private:
  mutable AutofillMetrics::BubbleMetric metric_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillMetrics);
};

class TestCallback {
 public:
  TestCallback() : accepted_count_(0), dismissed_count_(0) {}
  ~TestCallback() {}

  void Run(bool accepted) {
    if (accepted)
      accepted_count_++;
    else
      dismissed_count_++;
  }

  int accepted_count() const {
    return accepted_count_;
  }

  int dismissed_count() const {
    return dismissed_count_;
  }

  base::Callback<void(bool)> GetCallback() {
    return base::Bind(&TestCallback::Run, base::Unretained(this));
  }

 private:
  int accepted_count_;
  int dismissed_count_;
};

class TestAutocheckoutBubbleController :
  public autofill::AutocheckoutBubbleController {
 public:
  explicit TestAutocheckoutBubbleController(
      const base::Callback<void(bool)>& callback)
      : AutocheckoutBubbleController(gfx::RectF(),
                                     gfx::NativeWindow(),
                                     true /* is_google_user */,
                                     callback) {
    set_metric_logger(new TestAutofillMetrics);
  }
  virtual ~TestAutocheckoutBubbleController() {}

  const TestAutofillMetrics* get_metric_logger() const {
    return static_cast<TestAutofillMetrics*>(const_cast<AutofillMetrics*>(
        AutocheckoutBubbleController::metric_logger()));
  }
};

}  // namespace

TEST(AutocheckoutBubbleControllerTest, BubbleCreationAndDestructionMetrics) {
  // Test bubble created metric.
  TestCallback callback;
  TestAutocheckoutBubbleController controller(callback.GetCallback());

  controller.BubbleCreated();

  EXPECT_EQ(AutofillMetrics::BUBBLE_CREATED,
            controller.get_metric_logger()->metric());
  EXPECT_EQ(0, callback.accepted_count());
  EXPECT_EQ(0, callback.dismissed_count());

  // If neither BubbleAccepted or BubbleCanceled was called the bubble was
  // ignored.
  controller.BubbleDestroyed();

  EXPECT_EQ(AutofillMetrics::BUBBLE_IGNORED,
            controller.get_metric_logger()->metric());
  EXPECT_EQ(0, callback.accepted_count());
  EXPECT_EQ(1, callback.dismissed_count());
}

TEST(AutocheckoutBubbleControllerTest, BubbleAcceptedMetric) {
  // Test bubble accepted metric.
  TestCallback callback;
  TestAutocheckoutBubbleController controller(callback.GetCallback());

  controller.BubbleAccepted();

  EXPECT_EQ(AutofillMetrics::BUBBLE_ACCEPTED,
            controller.get_metric_logger()->metric());
  EXPECT_EQ(1, callback.accepted_count());
  EXPECT_EQ(0, callback.dismissed_count());

  // Test that after a bubble is accepted it is not considered ignored.
  controller.BubbleDestroyed();

  EXPECT_EQ(AutofillMetrics::BUBBLE_ACCEPTED,
            controller.get_metric_logger()->metric());
  EXPECT_EQ(1, callback.accepted_count());
  EXPECT_EQ(0, callback.dismissed_count());
}

TEST(AutocheckoutBubbleControllerTest, BubbleCanceledMetric) {
  // Test bubble dismissed metric.
  TestCallback callback;
  TestAutocheckoutBubbleController controller(callback.GetCallback());

  controller.BubbleCanceled();

  EXPECT_EQ(AutofillMetrics::BUBBLE_DISMISSED,
            controller.get_metric_logger()->metric());
  EXPECT_EQ(0, callback.accepted_count());
  EXPECT_EQ(1, callback.dismissed_count());

  // Test that after a bubble is dismissed it is not considered ignored.
  controller.BubbleDestroyed();

  EXPECT_EQ(AutofillMetrics::BUBBLE_DISMISSED,
            controller.get_metric_logger()->metric());
  EXPECT_EQ(0, callback.accepted_count());
  EXPECT_EQ(1, callback.dismissed_count());
}

}  // namespace autofill
