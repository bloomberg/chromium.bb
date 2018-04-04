// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "cc/base/lap_timer.h"
#include "chrome/browser/vr/cpu_surface_provider.h"
#include "chrome/browser/vr/elements/text.h"
#include "chrome/browser/vr/ganesh_surface_provider.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/test/gl_test_environment.h"
#include "chrome/browser/vr/test/perf_test_utils.h"
#include "skia/ext/texture_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace vr {

namespace {

constexpr size_t kNumberOfRuns = 35;
constexpr float kFontHeightMeters = 0.05f;
constexpr float kTextWidthMeters = 1.0f;
constexpr char kSuiteName[] = "TextPerfTest";

}  // namespace

class TextPerfTest : public testing::Test {
 public:
  void SetUp() override {
    gl_test_environment_ =
        std::make_unique<GlTestEnvironment>(kPixelHalfScreen);
    provider_ = std::make_unique<GaneshSurfaceProvider>();

    text_element_ = std::make_unique<Text>(kFontHeightMeters);
    text_element_->SetSize(kTextWidthMeters, 0);
    text_element_->Initialize(provider_.get());
  }

  void TearDown() override {
    text_element_.reset();
    gl_test_environment_.reset();
  }

 protected:
  void RenderAndLapTimer() {
    static_cast<UiElement*>(text_element_.get())->PrepareToDraw();
    // Make sure all GL commands are applied before we measure the time.
    glFinish();
    timer_.NextLap();
  }

  std::unique_ptr<Text> text_element_;
  cc::LapTimer timer_;

 private:
  std::unique_ptr<GlTestEnvironment> gl_test_environment_;
  std::unique_ptr<SkiaSurfaceProvider> provider_;
};

TEST_F(TextPerfTest, RenderLoremIpsum100Chars) {
  base::string16 text = base::UTF8ToUTF16(kLoremIpsum100Chars);
  timer_.Reset();
  for (size_t i = 0; i < kNumberOfRuns; i++) {
    text[0] = 'a' + (i % 26);
    text_element_->SetText(text);
    RenderAndLapTimer();
  }
  PrintResults(kSuiteName, "render_lorem_ipsum_100_chars", &timer_);
}

TEST_F(TextPerfTest, RenderLoremIpsum700Chars) {
  base::string16 text = base::UTF8ToUTF16(kLoremIpsum700Chars);
  timer_.Reset();
  for (size_t i = 0; i < kNumberOfRuns; i++) {
    text[0] = 'a' + (i % 26);
    text_element_->SetText(text);
    RenderAndLapTimer();
  }
  PrintResults(kSuiteName, "render_lorem_ipsum_700_chars", &timer_);
}

TEST_F(TextPerfTest, RenderLoremIpsum100Chars_NoTextChange) {
  base::string16 text = base::UTF8ToUTF16(kLoremIpsum100Chars);
  text_element_->SetText(text);
  TexturedElement::SetRerenderIfNotDirtyForTesting();
  timer_.Reset();
  for (size_t i = 0; i < kNumberOfRuns; i++) {
    RenderAndLapTimer();
  }
  PrintResults(kSuiteName, "render_lorem_ipsum_100_chars_no_text_change",
               &timer_);
}

TEST_F(TextPerfTest, RenderLoremIpsum700Chars_NoTextChange) {
  base::string16 text = base::UTF8ToUTF16(kLoremIpsum700Chars);
  text_element_->SetText(text);
  TexturedElement::SetRerenderIfNotDirtyForTesting();
  timer_.Reset();
  for (size_t i = 0; i < kNumberOfRuns; i++) {
    RenderAndLapTimer();
  }
  PrintResults(kSuiteName, "render_lorem_ipsum_700_chars_no_text_change",
               &timer_);
}

}  // namespace vr
