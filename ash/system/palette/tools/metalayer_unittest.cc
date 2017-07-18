// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/system/palette/mock_palette_tool_delegate.h"
#include "ash/system/palette/palette_ids.h"
#include "ash/system/palette/palette_tool.h"
#include "ash/system/palette/tools/metalayer_mode.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/shell_test_api.h"
#include "ash/test/test_palette_delegate.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Base class for all metalayer ash tests.
class MetalayerToolTest : public AshTestBase {
 public:
  MetalayerToolTest() {}
  ~MetalayerToolTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();

    ShellTestApi().SetPaletteDelegate(base::MakeUnique<TestPaletteDelegate>());

    palette_tool_delegate_ = base::MakeUnique<MockPaletteToolDelegate>();
    tool_ = base::MakeUnique<MetalayerMode>(palette_tool_delegate_.get());
  }

  TestPaletteDelegate* test_palette_delegate() {
    return static_cast<TestPaletteDelegate*>(Shell::Get()->palette_delegate());
  }

 protected:
  std::unique_ptr<MockPaletteToolDelegate> palette_tool_delegate_;
  std::unique_ptr<PaletteTool> tool_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MetalayerToolTest);
};

}  // namespace

// The metalayer tool is only visible when the delegate supports metalayer.
TEST_F(MetalayerToolTest, ViewOnlyCreatedWhenMetalayerIsSupported) {
  test_palette_delegate()->set_is_metalayer_supported(false);
  EXPECT_FALSE(tool_->CreateView());
  tool_->OnViewDestroyed();

  test_palette_delegate()->set_is_metalayer_supported(true);
  std::unique_ptr<views::View> view = base::WrapUnique(tool_->CreateView());
  EXPECT_TRUE(view);
  tool_->OnViewDestroyed();
}

// Verifies that enabling the tool triggers the metalayer in the delegate.
// Invoking the callback passed to the delegate disables the tool.
TEST_F(MetalayerToolTest, EnablingMetalayerCallsDelegateAndDisablesTool) {
  // Showing a metalayer calls the palette delegate to show
  // the metalayer and hides the palette.
  EXPECT_CALL(*palette_tool_delegate_.get(), HidePalette());
  tool_->OnEnable();
  EXPECT_EQ(1, test_palette_delegate()->show_metalayer_count());
  testing::Mock::VerifyAndClearExpectations(palette_tool_delegate_.get());

  // Calling the associated callback (metalayer closed) will disable the tool.
  EXPECT_CALL(*palette_tool_delegate_.get(),
              DisableTool(PaletteToolId::METALAYER));
  test_palette_delegate()->metalayer_closed().Run();
}

}  // namespace ash
