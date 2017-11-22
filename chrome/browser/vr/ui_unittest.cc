// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/test/mock_browser_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

class FakeContentInputForwarder : public ContentInputForwarder {
 public:
  FakeContentInputForwarder() {}
  ~FakeContentInputForwarder() override {}

  void ForwardEvent(std::unique_ptr<blink::WebInputEvent> event,
                    int content_id) override {}
};

class UiTest : public testing::Test {
 public:
  UiTest() {}
  ~UiTest() override {}

  void SetUp() override {
    UiInitialState initial_state;
    forwarder_ = base::MakeUnique<FakeContentInputForwarder>();
    browser_ = base::MakeUnique<testing::NiceMock<MockBrowserInterface>>();
    ui_ = base::MakeUnique<Ui>(browser_.get(), forwarder_.get(), initial_state);
  }

 protected:
  std::unique_ptr<FakeContentInputForwarder> forwarder_;
  std::unique_ptr<MockBrowserInterface> browser_;
  std::unique_ptr<Ui> ui_;
};

TEST_F(UiTest, SecondExitPromptTriggersOnExitPrompt) {
  ui_->SetExitVrPromptEnabled(true, UiUnsupportedMode::kUnhandledPageInfo);
  // Initiating another exit VR prompt while a previous one was in flight should
  // result in a call to the UiBrowserInterface.
  EXPECT_CALL(*browser_,
              OnExitVrPromptResult(ExitVrPromptChoice::CHOICE_NONE,
                                   UiUnsupportedMode::kUnhandledPageInfo));
  ui_->SetExitVrPromptEnabled(true,
                              UiUnsupportedMode::kAndroidPermissionNeeded);
}

TEST_F(UiTest, ExitPresentAndFullscreenOnAppButtonClick) {
  ui_->SetWebVrMode(true, false);
  // Clicking app button should trigger to exit presentation.
  EXPECT_CALL(*browser_, ExitPresent());
  // And also trigger exit fullscreen.
  EXPECT_CALL(*browser_, ExitFullscreen());
  ui_->OnAppButtonClicked();
}

}  // namespace vr
