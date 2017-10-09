// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/shell.h"
#include "ash/shell_test_api.h"
#include "ash/system/palette/mock_palette_tool_delegate.h"
#include "ash/system/palette/palette_ids.h"
#include "ash/system/palette/palette_tool.h"
#include "ash/system/palette/test_palette_delegate.h"
#include "ash/system/palette/tools/capture_region_mode.h"
#include "ash/system/palette/tools/capture_screen_action.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"

namespace ash {

namespace {

// Base class for all create note ash tests.
class ScreenshotToolTest : public AshTestBase {
 public:
  ScreenshotToolTest() {}
  ~ScreenshotToolTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();

    ShellTestApi().SetPaletteDelegate(std::make_unique<TestPaletteDelegate>());

    palette_tool_delegate_ = std::make_unique<MockPaletteToolDelegate>();
  }

  TestPaletteDelegate* test_palette_delegate() {
    return static_cast<TestPaletteDelegate*>(Shell::Get()->palette_delegate());
  }

 protected:
  std::unique_ptr<MockPaletteToolDelegate> palette_tool_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenshotToolTest);
};

}  // namespace

// Verifies that capturing a region triggers the partial screenshot delegate
// method. Invoking the callback passed to the delegate disables the tool.
TEST_F(ScreenshotToolTest, EnablingCaptureRegionCallsDelegateAndDisablesTool) {
  std::unique_ptr<PaletteTool> tool =
      std::make_unique<CaptureRegionMode>(palette_tool_delegate_.get());

  // Starting a partial screenshot calls the calls the palette delegate to start
  // a screenshot session and hides the palette.
  EXPECT_CALL(*palette_tool_delegate_.get(), HidePalette());
  tool->OnEnable();
  EXPECT_EQ(1, test_palette_delegate()->take_partial_screenshot_count());
  testing::Mock::VerifyAndClearExpectations(palette_tool_delegate_.get());

  // Calling the associated callback (partial screenshot finished) will disable
  // the tool.
  EXPECT_CALL(*palette_tool_delegate_.get(),
              DisableTool(PaletteToolId::CAPTURE_REGION));
  test_palette_delegate()->partial_screenshot_done().Run();
}

// Verifies that capturing the screen triggers the screenshot delegate method,
// disables the tool, and hides the palette.
TEST_F(ScreenshotToolTest, EnablingCaptureScreenCallsDelegateAndDisablesTool) {
  std::unique_ptr<PaletteTool> tool =
      std::make_unique<CaptureScreenAction>(palette_tool_delegate_.get());
  EXPECT_CALL(*palette_tool_delegate_.get(),
              DisableTool(PaletteToolId::CAPTURE_SCREEN));
  EXPECT_CALL(*palette_tool_delegate_.get(), HidePaletteImmediately());
  tool->OnEnable();
  EXPECT_EQ(1, test_palette_delegate()->take_screenshot_count());
}

}  // namespace ash
