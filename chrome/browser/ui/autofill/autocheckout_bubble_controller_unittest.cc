// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autofill/autofill_metrics.h"
#include "chrome/browser/ui/autofill/autocheckout_bubble_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"

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

class TestClosure {
 public:
  TestClosure() : run_count_(0) {}
  ~TestClosure() {}

  void Run() {
    run_count_++;
  }

  int run_count() const {
    return run_count_;
  }

  base::Closure GetCallback() {
    return base::Bind(&TestClosure::Run, base::Unretained(this));
  }

 private:
  int run_count_;
};

class TestAutocheckoutBubbleController :
  public autofill::AutocheckoutBubbleController {
 public:
  TestAutocheckoutBubbleController(const gfx::RectF anchor_rect,
                                   const base::Closure& callback)
      : AutocheckoutBubbleController(anchor_rect, callback) {
    set_metric_logger(new TestAutofillMetrics);
  }
  virtual ~TestAutocheckoutBubbleController() {}

  const TestAutofillMetrics* get_metric_logger() const {
    return static_cast<TestAutofillMetrics*>(const_cast<AutofillMetrics*>(
        AutocheckoutBubbleController::metric_logger()));
  }
};

}  // namespace

namespace autofill {

TEST(AutocheckoutBubbleControllerTest, BubbleCreationAndDestructionMetrics) {
  // Test bubble created metric.
  TestClosure closure;
  TestAutocheckoutBubbleController controller(gfx::RectF(),
                                              closure.GetCallback());

  controller.BubbleCreated();

  EXPECT_EQ(AutofillMetrics::BUBBLE_CREATED,
            controller.get_metric_logger()->metric());
  EXPECT_EQ(0, closure.run_count());

  // If neither BubbleAccepted or BubbleCanceled was called the bubble was
  // ignored.
  controller.BubbleDestroyed();

  EXPECT_EQ(AutofillMetrics::BUBBLE_IGNORED,
            controller.get_metric_logger()->metric());
  EXPECT_EQ(0, closure.run_count());
}

TEST(AutocheckoutBubbleControllerTest, BubbleAcceptedMetric) {
  // Test bubble accepted metric.
  TestClosure closure;
  TestAutocheckoutBubbleController controller(gfx::RectF(),
                                              closure.GetCallback());

  controller.BubbleAccepted();

  EXPECT_EQ(AutofillMetrics::BUBBLE_ACCEPTED,
            controller.get_metric_logger()->metric());
  EXPECT_EQ(1, closure.run_count());

  // Test that after a bubble is accepted it is not considered ignored.
  controller.BubbleDestroyed();

  EXPECT_EQ(AutofillMetrics::BUBBLE_ACCEPTED,
            controller.get_metric_logger()->metric());
  EXPECT_EQ(1, closure.run_count());
}

TEST(AutocheckoutBubbleControllerTest, BubbleCanceledMetric) {
  // Test bubble dismissed metric.
  TestClosure closure;
  TestAutocheckoutBubbleController controller(gfx::RectF(),
                                              closure.GetCallback());

  controller.BubbleCanceled();

  EXPECT_EQ(AutofillMetrics::BUBBLE_DISMISSED,
            controller.get_metric_logger()->metric());
  EXPECT_EQ(0, closure.run_count());

  // Test that after a bubble is dismissed it is not considered ignored.
  controller.BubbleDestroyed();

  EXPECT_EQ(AutofillMetrics::BUBBLE_DISMISSED,
            controller.get_metric_logger()->metric());
  EXPECT_EQ(0, closure.run_count());
}

}  // namespace autofill
