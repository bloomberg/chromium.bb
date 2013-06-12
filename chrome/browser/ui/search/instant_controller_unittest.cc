// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "chrome/browser/ui/search/instant_controller.h"
#include "chrome/browser/ui/search/instant_overlay.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::HistogramBase;
using base::HistogramSamples;
using base::StatisticsRecorder;

class TestableInstantOverlay : public InstantOverlay {
 public:
  TestableInstantOverlay(InstantController* controller,
                         const std::string& instant_url)
      : InstantOverlay(controller, instant_url) {
  }

  // Overrides from InstantPage
  virtual bool supports_instant() const OVERRIDE {
    return test_supports_instant_;
  }

  virtual bool IsLocal() const OVERRIDE {
    return test_is_local_;
  };

  void set_supports_instant(bool supports_instant) {
    test_supports_instant_ = supports_instant;
  }

  void set_is_local(bool is_local) {
    test_is_local_ = is_local;
  }

 private:
  std::string test_instant_url_;
  bool test_supports_instant_;
  bool test_is_local_;
};

class TestableInstantController : public InstantController {
 public:
  TestableInstantController()
      : InstantController(NULL, true),
        test_instant_url_("http://test_url"),
        test_extended_enabled_(true) {}

  // Overrides from InstantController
  virtual std::string GetInstantURL() const OVERRIDE {
    return test_instant_url_;
  }

  virtual bool extended_enabled() const OVERRIDE {
    return test_extended_enabled_;
  }

  virtual InstantOverlay* overlay() const OVERRIDE {
    return test_overlay_.get();
  }

  void set_instant_url(std::string instant_url) {
    test_instant_url_ = instant_url;
  }

  void set_extended_enabled(bool extended_enabled) {
    test_extended_enabled_ = extended_enabled;
  }

  void set_overlay(InstantOverlay* overlay) {
    test_overlay_.reset(overlay);
  }

private:
  std::string test_instant_url_;
  bool test_extended_enabled_;
  scoped_ptr<InstantOverlay> test_overlay_;
};

class InstantControllerTest : public testing::Test {
 public:
  InstantControllerTest()
      : loop_(base::MessageLoop::TYPE_DEFAULT),
        instant_controller_(new TestableInstantController()) {
  }

  virtual void SetUp() OVERRIDE {
    base::StatisticsRecorder::Initialize();
  }

  TestableInstantController* instant_controller() {
    return instant_controller_.get();
  }

 private:
  base::MessageLoop loop_;
  scoped_ptr<TestableInstantController> instant_controller_;
};

TEST_F(InstantControllerTest, ShouldSwitchToLocalOverlayReturn) {
  InstantController::InstantFallbackReason fallback_reason;

  instant_controller()->set_extended_enabled(false);
  fallback_reason = instant_controller()->ShouldSwitchToLocalOverlay();
  ASSERT_EQ(fallback_reason, InstantController::INSTANT_FALLBACK_NONE);

  instant_controller()->set_extended_enabled(true);
  fallback_reason = instant_controller()->ShouldSwitchToLocalOverlay();
  ASSERT_EQ(fallback_reason, InstantController::INSTANT_FALLBACK_NO_OVERLAY);

  std::string instant_url("http://test_url");
  TestableInstantOverlay* test_overlay =
      new TestableInstantOverlay(instant_controller(), instant_url);
  test_overlay->set_is_local(true);
  instant_controller()->set_overlay(test_overlay);
  fallback_reason = instant_controller()->ShouldSwitchToLocalOverlay();
  ASSERT_EQ(fallback_reason, InstantController::INSTANT_FALLBACK_NONE);

  test_overlay->set_is_local(false);
  instant_controller()->set_instant_url("");
  fallback_reason = instant_controller()->ShouldSwitchToLocalOverlay();
  ASSERT_EQ(fallback_reason,
            InstantController::INSTANT_FALLBACK_INSTANT_URL_EMPTY);

  instant_controller()->set_instant_url("http://instant_url");
  fallback_reason = instant_controller()->ShouldSwitchToLocalOverlay();
  ASSERT_EQ(fallback_reason,
            InstantController::INSTANT_FALLBACK_ORIGIN_PATH_MISMATCH);

  instant_controller()->set_instant_url(instant_url);
  test_overlay->set_supports_instant(false);
  fallback_reason = instant_controller()->ShouldSwitchToLocalOverlay();
  ASSERT_EQ(fallback_reason,
            InstantController::INSTANT_FALLBACK_INSTANT_NOT_SUPPORTED);

  test_overlay->set_supports_instant(true);
  fallback_reason = instant_controller()->ShouldSwitchToLocalOverlay();
  ASSERT_EQ(fallback_reason, InstantController::INSTANT_FALLBACK_NONE);
}
