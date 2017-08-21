// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/shell_test_api.h"
#include "ash/system/palette/mock_palette_tool_delegate.h"
#include "ash/system/palette/palette_ids.h"
#include "ash/system/palette/palette_tool.h"
#include "ash/system/palette/test_palette_delegate.h"
#include "ash/system/palette/tools/metalayer_mode.h"
#include "ash/test/ash_test_base.h"
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

  void TearDown() override {
    // This needs to be called first to remove the event handler before the
    // shell instance gets torn down.
    tool_.reset();
    AshTestBase::TearDown();
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

// The metalayer tool is always visible, but only enabled when the delegate
// supports metalayer AND the voice interaction framework is ready.
TEST_F(MetalayerToolTest, PaletteMenuState) {
  ash::Shell::Get()->NotifyVoiceInteractionStatusChanged(
      ash::VoiceInteractionState::NOT_READY);

  {
    // Voice interaction not ready, metalayer not supported.
    test_palette_delegate()->SetMetalayerSupported(false);
    std::unique_ptr<views::View> view = base::WrapUnique(tool_->CreateView());
    EXPECT_TRUE(view);
    EXPECT_FALSE(view->enabled());
    tool_->OnViewDestroyed();
  }

  {
    // Voice interaction not ready, metalayer supported.
    test_palette_delegate()->SetMetalayerSupported(true);
    std::unique_ptr<views::View> view = base::WrapUnique(tool_->CreateView());
    EXPECT_TRUE(view);
    EXPECT_FALSE(view->enabled());
    tool_->OnViewDestroyed();
  }

  ash::Shell::Get()->NotifyVoiceInteractionStatusChanged(
      ash::VoiceInteractionState::RUNNING);

  {
    // Voice interaction ready, metalayer not supported.
    test_palette_delegate()->SetMetalayerSupported(false);
    std::unique_ptr<views::View> view = base::WrapUnique(tool_->CreateView());
    EXPECT_TRUE(view);
    EXPECT_FALSE(view->enabled());
    tool_->OnViewDestroyed();
  }

  {
    // Voice interaction ready, metalayer supported: the only combination when
    // the view should be enabled.
    test_palette_delegate()->SetMetalayerSupported(true);
    std::unique_ptr<views::View> view = base::WrapUnique(tool_->CreateView());
    EXPECT_TRUE(view);
    EXPECT_TRUE(view->enabled());
    tool_->OnViewDestroyed();
  }
}

// Verifies that enabling/disabling the metalayer tool invokes the delegate.
TEST_F(MetalayerToolTest, EnablingDisablingMetalayerCallsDelegate) {
  // Enabling the metalayer tool calls the delegate to show the metalayer.
  // It should also hide the palette.
  EXPECT_CALL(*palette_tool_delegate_.get(), HidePalette());
  tool_->OnEnable();
  EXPECT_EQ(1, test_palette_delegate()->show_metalayer_count());
  EXPECT_EQ(0, test_palette_delegate()->hide_metalayer_count());
  testing::Mock::VerifyAndClearExpectations(palette_tool_delegate_.get());

  // Enabling the metalayer tool calls the delegate to hide the metalayer.
  tool_->OnDisable();
  EXPECT_EQ(1, test_palette_delegate()->show_metalayer_count());
  EXPECT_EQ(1, test_palette_delegate()->hide_metalayer_count());
  testing::Mock::VerifyAndClearExpectations(palette_tool_delegate_.get());
}

// Verifies that invoking the callback passed to the delegate disables the tool.
TEST_F(MetalayerToolTest, MetalayerCallbackDisablesPaletteTool) {
  tool_->OnEnable();
  // Calling the associated callback (metalayer closed) will disable the tool.
  EXPECT_CALL(*palette_tool_delegate_.get(),
              DisableTool(PaletteToolId::METALAYER));
  test_palette_delegate()->metalayer_closed().Run();
}

// Verifies that disabling the metalayer support in the delegate disables the
// tool.
TEST_F(MetalayerToolTest, MetalayerUnsupportedDisablesPaletteTool) {
  test_palette_delegate()->SetMetalayerSupported(true);
  tool_->OnEnable();
  // Disabling the metalayer support in the delegate will disable the tool.
  EXPECT_CALL(*palette_tool_delegate_.get(),
              DisableTool(PaletteToolId::METALAYER));
  test_palette_delegate()->SetMetalayerSupported(false);
}

}  // namespace ash
