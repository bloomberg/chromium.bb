// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

    ShellTestApi().SetPaletteDelegate(base::MakeUnique<TestPaletteDelegate>());

    palette_tool_delegate_ = base::MakeUnique<MockPaletteToolDelegate>();
    tool_ = base::MakeUnique<MetalayerMode>(palette_tool_delegate_.get());
    highlighter_controller_ = base::MakeUnique<HighlighterController>();
    highlighter_test_api_ = base::MakeUnique<HighlighterControllerTestApi>(
        highlighter_controller_.get());
    test_palette_delegate()->set_highlighter_test_api(
        highlighter_test_api_.get());
  }

  void TearDown() override {
    // This needs to be called first to remove the event handler before the
    // shell instance gets torn down.
    test_palette_delegate()->set_highlighter_test_api(nullptr);
    highlighter_test_api_.reset();
    highlighter_controller_.reset();
    tool_.reset();
    AshTestBase::TearDown();
  }

  TestPaletteDelegate* test_palette_delegate() {
    return static_cast<TestPaletteDelegate*>(Shell::Get()->palette_delegate());
  }

 protected:
  std::unique_ptr<HighlighterController> highlighter_controller_;
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

// Verifies that disabling the metalayer support in the delegate disables the
// tool.
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

// Verifies that invoking the callback passed to the delegate disables the tool.
TEST_F(MetalayerToolTest, MetalayerCallbackDisablesPaletteTool) {
  tool_->OnEnable();
  // Calling the associated callback |metalayer_done| will disable the tool.
  EXPECT_CALL(*palette_tool_delegate_.get(),
              DisableTool(PaletteToolId::METALAYER));
  highlighter_test_api_->CallMetalayerDone();
}

}  // namespace ash
