// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/highlighter/highlighter_controller.h"
#include "ash/highlighter/highlighter_controller_test_api.h"
#include "ash/shell.h"
#include "ash/shell_test_api.h"
#include "ash/system/palette/mock_palette_tool_delegate.h"
#include "ash/system/palette/palette_ids.h"
#include "ash/system/palette/palette_tool.h"
#include "ash/system/palette/test_palette_delegate.h"
#include "ash/system/palette/tools/metalayer_mode.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/label.h"
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

    ShellTestApi().SetPaletteDelegate(std::make_unique<TestPaletteDelegate>());

    palette_tool_delegate_ = std::make_unique<MockPaletteToolDelegate>();
    tool_ = std::make_unique<MetalayerMode>(palette_tool_delegate_.get());
    highlighter_test_api_ = std::make_unique<HighlighterControllerTestApi>(
        Shell::Get()->highlighter_controller());
  }

  void TearDown() override {
    // This needs to be called first to reset the controller state before the
    // shell instance gets torn down.
    highlighter_test_api_.reset();
    tool_.reset();
    AshTestBase::TearDown();
  }

  TestPaletteDelegate* test_palette_delegate() {
    return static_cast<TestPaletteDelegate*>(Shell::Get()->palette_delegate());
  }

 protected:
  std::unique_ptr<HighlighterControllerTestApi> highlighter_test_api_;
  std::unique_ptr<MockPaletteToolDelegate> palette_tool_delegate_;
  std::unique_ptr<PaletteTool> tool_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MetalayerToolTest);
};

}  // namespace

// The metalayer tool is always visible, but only enabled when the user
// has enabled the metalayer AND the voice interaction framework is ready.
TEST_F(MetalayerToolTest, PaletteMenuState) {
  const VoiceInteractionState kStates[] = {VoiceInteractionState::NOT_READY,
                                           VoiceInteractionState::STOPPED,
                                           VoiceInteractionState::RUNNING};
  const base::string16 kLoading(base::ASCIIToUTF16("loading"));

  // Iterate over every possible combination of states.
  for (VoiceInteractionState state : kStates) {
    for (int enabled = 0; enabled <= 1; enabled++) {
      for (int context = 0; context <= 1; context++) {
        const bool ready = state != VoiceInteractionState::NOT_READY;
        const bool selectable = enabled && context && ready;

        Shell::Get()->NotifyVoiceInteractionStatusChanged(state);
        Shell::Get()->NotifyVoiceInteractionEnabled(enabled);
        Shell::Get()->NotifyVoiceInteractionContextEnabled(context);

        std::unique_ptr<views::View> view =
            base::WrapUnique(tool_->CreateView());
        EXPECT_TRUE(view);
        EXPECT_EQ(selectable, view->enabled());

        const base::string16 label_text =
            static_cast<HoverHighlightView*>(view.get())->text_label()->text();

        const bool label_contains_loading =
            label_text.find(kLoading) != base::string16::npos;

        EXPECT_EQ(enabled && context && !ready, label_contains_loading);
        tool_->OnViewDestroyed();
      }
    }
  }
}

// Verifies that the metalayer enabled/disabled state propagates to the
// highlighter controller.
TEST_F(MetalayerToolTest, EnablingDisablingMetalayerEnablesDisablesController) {
  // Enabling the metalayer tool enables the highligher controller.
  // It should also hide the palette.
  EXPECT_CALL(*palette_tool_delegate_.get(), HidePalette());
  highlighter_test_api_->ResetEnabledState();
  tool_->OnEnable();
  EXPECT_TRUE(highlighter_test_api_->HandleEnabledStateChangedCalled());
  EXPECT_TRUE(highlighter_test_api_->enabled());
  testing::Mock::VerifyAndClearExpectations(palette_tool_delegate_.get());

  // Disabling the metalayer tool disables the highlighter controller.
  highlighter_test_api_->ResetEnabledState();
  tool_->OnDisable();
  EXPECT_TRUE(highlighter_test_api_->HandleEnabledStateChangedCalled());
  EXPECT_FALSE(highlighter_test_api_->enabled());
  testing::Mock::VerifyAndClearExpectations(palette_tool_delegate_.get());
}

// Verifies that disabling the metalayer support disables the tool.
TEST_F(MetalayerToolTest, MetalayerUnsupportedDisablesPaletteTool) {
  Shell::Get()->NotifyVoiceInteractionStatusChanged(
      VoiceInteractionState::RUNNING);
  Shell::Get()->NotifyVoiceInteractionEnabled(true);
  Shell::Get()->NotifyVoiceInteractionContextEnabled(true);

  // Disabling the user prefs individually should disable the tool.
  tool_->OnEnable();
  EXPECT_CALL(*palette_tool_delegate_.get(),
              DisableTool(PaletteToolId::METALAYER));
  Shell::Get()->NotifyVoiceInteractionEnabled(false);
  testing::Mock::VerifyAndClearExpectations(palette_tool_delegate_.get());
  Shell::Get()->NotifyVoiceInteractionEnabled(true);

  tool_->OnEnable();
  EXPECT_CALL(*palette_tool_delegate_.get(),
              DisableTool(PaletteToolId::METALAYER));
  Shell::Get()->NotifyVoiceInteractionContextEnabled(false);
  testing::Mock::VerifyAndClearExpectations(palette_tool_delegate_.get());
  Shell::Get()->NotifyVoiceInteractionContextEnabled(true);

  // Test VoiceInteractionState changes.
  tool_->OnEnable();

  // Changing the state from RUNNING to STOPPED and back should not disable the
  // tool.
  EXPECT_CALL(*palette_tool_delegate_.get(),
              DisableTool(PaletteToolId::METALAYER))
      .Times(0);
  Shell::Get()->NotifyVoiceInteractionStatusChanged(
      VoiceInteractionState::STOPPED);
  Shell::Get()->NotifyVoiceInteractionStatusChanged(
      VoiceInteractionState::RUNNING);
  testing::Mock::VerifyAndClearExpectations(palette_tool_delegate_.get());

  // Changing the state to NOT_READY should disable the tool.
  EXPECT_CALL(*palette_tool_delegate_.get(),
              DisableTool(PaletteToolId::METALAYER));
  Shell::Get()->NotifyVoiceInteractionStatusChanged(
      VoiceInteractionState::NOT_READY);
  testing::Mock::VerifyAndClearExpectations(palette_tool_delegate_.get());
}

// Verifies that detaching the highlighter client disables the palette tool.
TEST_F(MetalayerToolTest, DetachingClientDisablesPaletteTool) {
  tool_->OnEnable();
  // If the client detaches, the tool should become disabled.
  EXPECT_CALL(*palette_tool_delegate_.get(),
              DisableTool(PaletteToolId::METALAYER));
  highlighter_test_api_->DetachClient();
  testing::Mock::VerifyAndClearExpectations(palette_tool_delegate_.get());

  // If the client attaches again, the tool should not become enabled.
  highlighter_test_api_->AttachClient();
  EXPECT_CALL(*palette_tool_delegate_.get(),
              EnableTool(PaletteToolId::METALAYER))
      .Times(0);
  testing::Mock::VerifyAndClearExpectations(palette_tool_delegate_.get());
}

}  // namespace ash
