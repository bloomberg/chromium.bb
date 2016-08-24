// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/palette/mock_palette_tool_delegate.h"
#include "ash/common/system/chromeos/palette/palette_ids.h"
#include "ash/common/system/chromeos/palette/palette_tool.h"
#include "ash/common/system/chromeos/palette/tools/capture_region_action.h"
#include "ash/common/system/chromeos/palette/tools/capture_screen_action.h"
#include "ash/common/test/test_palette_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"

namespace ash {

namespace {

// Base class for all create note ash tests.
class ScreenshotToolTest : public test::AshTestBase {
 public:
  ScreenshotToolTest() {}
  ~ScreenshotToolTest() override {}

  void SetUp() override {
    test::AshTestBase::SetUp();

    WmShell::Get()->SetPaletteDelegateForTesting(
        base::MakeUnique<TestPaletteDelegate>());

    palette_tool_delegate_ = base::MakeUnique<MockPaletteToolDelegate>();
  }

  TestPaletteDelegate* test_palette_delegate() {
    return static_cast<TestPaletteDelegate*>(
        WmShell::Get()->palette_delegate());
  }

 protected:
  std::unique_ptr<MockPaletteToolDelegate> palette_tool_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenshotToolTest);
};

}  // namespace

// Verifies that capturing a region triggers the partial screenshot delegate
// method, disables the tool, and hides the palette.
TEST_F(ScreenshotToolTest, EnablingCaptureRegionCallsDelegateAndDisablesTool) {
  std::unique_ptr<PaletteTool> tool =
      base::MakeUnique<CaptureRegionAction>(palette_tool_delegate_.get());
  EXPECT_CALL(*palette_tool_delegate_.get(),
              DisableTool(PaletteToolId::CAPTURE_REGION));
  EXPECT_CALL(*palette_tool_delegate_.get(), HidePalette());
  tool->OnEnable();
  EXPECT_EQ(1, test_palette_delegate()->take_partial_screenshot_count());
}

// Verifies that capturing the screen triggers the screenshot delegate method,
// disables the tool, and hides the palette.
TEST_F(ScreenshotToolTest, EnablingCaptureScreenCallsDelegateAndDisablesTool) {
  std::unique_ptr<PaletteTool> tool =
      base::MakeUnique<CaptureScreenAction>(palette_tool_delegate_.get());
  EXPECT_CALL(*palette_tool_delegate_.get(),
              DisableTool(PaletteToolId::CAPTURE_SCREEN));
  EXPECT_CALL(*palette_tool_delegate_.get(), HidePalette());
  tool->OnEnable();
  EXPECT_EQ(1, test_palette_delegate()->take_screenshot_count());
}

}  // namespace ash
