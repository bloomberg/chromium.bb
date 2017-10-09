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
#include "ash/system/palette/tools/create_note_action.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Base class for all create note ash tests.
class CreateNoteTest : public AshTestBase {
 public:
  CreateNoteTest() {}
  ~CreateNoteTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();

    ShellTestApi().SetPaletteDelegate(std::make_unique<TestPaletteDelegate>());

    palette_tool_delegate_ = std::make_unique<MockPaletteToolDelegate>();
    tool_ = std::make_unique<CreateNoteAction>(palette_tool_delegate_.get());
  }

  TestPaletteDelegate* test_palette_delegate() {
    return static_cast<TestPaletteDelegate*>(Shell::Get()->palette_delegate());
  }

 protected:
  std::unique_ptr<MockPaletteToolDelegate> palette_tool_delegate_;
  std::unique_ptr<PaletteTool> tool_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CreateNoteTest);
};

}  // namespace

// The note tool is only visible when there is a note-taking app available.
TEST_F(CreateNoteTest, ViewOnlyCreatedWhenNoteAppIsAvailable) {
  test_palette_delegate()->set_has_note_app(false);
  EXPECT_FALSE(tool_->CreateView());
  tool_->OnViewDestroyed();

  test_palette_delegate()->set_has_note_app(true);
  std::unique_ptr<views::View> view = base::WrapUnique(tool_->CreateView());
  EXPECT_TRUE(view);
  tool_->OnViewDestroyed();
}

// Activating the note tool both creates a note via the delegate and also
// disables the tool and hides the palette.
TEST_F(CreateNoteTest, EnablingToolCreatesNewNoteAndDisablesTool) {
  test_palette_delegate()->set_has_note_app(true);
  std::unique_ptr<views::View> view = base::WrapUnique(tool_->CreateView());

  EXPECT_CALL(*palette_tool_delegate_.get(),
              DisableTool(PaletteToolId::CREATE_NOTE));
  EXPECT_CALL(*palette_tool_delegate_.get(), HidePalette());

  tool_->OnEnable();
  EXPECT_EQ(1, test_palette_delegate()->create_note_count());
}

}  // namespace ash
