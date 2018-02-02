// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_input_manager.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/vr/elements/content_element.h"
#include "chrome/browser/vr/elements/keyboard.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/test/mock_keyboard_delegate.h"
#include "chrome/browser/vr/test/mock_text_input_delegate.h"
#include "chrome/browser/vr/test/ui_test.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::StrictMock;

namespace vr {

class ContentElementSceneTest : public UiTest {
 public:
  void SetUp() override {
    UiTest::SetUp();
    CreateScene(kNotInCct, kNotInWebVr);

    // Make test text input.
    text_input_delegate_ =
        std::make_unique<StrictMock<MockTextInputDelegate>>();

    auto* content =
        static_cast<ContentElement*>(scene_->GetUiElementByName(kContentQuad));
    content->SetTextInputDelegate(text_input_delegate_.get());
    OnBeginFrame();
  }

 protected:
  std::unique_ptr<StrictMock<MockTextInputDelegate>> text_input_delegate_;
  testing::Sequence in_sequence_;
};

TEST_F(ContentElementSceneTest, WebInputFocus) {
  auto* content =
      static_cast<ContentElement*>(scene_->GetUiElementByName(kContentQuad));
  // Set mock keyboard delegate.
  auto* kb = static_cast<Keyboard*>(scene_->GetUiElementByName(kKeyboard));
  auto kb_delegate = std::make_unique<StrictMock<MockKeyboardDelegate>>();
  EXPECT_CALL(*kb_delegate, HideKeyboard()).InSequence(in_sequence_);
  kb->SetKeyboardDelegate(kb_delegate.get());

  // Editing web input.
  EXPECT_CALL(*text_input_delegate_, RequestFocus(_)).InSequence(in_sequence_);
  EXPECT_CALL(*kb_delegate, ShowKeyboard()).InSequence(in_sequence_);
  EXPECT_CALL(*kb_delegate, OnBeginFrame()).InSequence(in_sequence_);
  EXPECT_CALL(*kb_delegate, SetTransform(_)).InSequence(in_sequence_);
  ui_->ShowSoftInput(true);
  EXPECT_TRUE(OnBeginFrame());

  // Giving content focus should tell the delegate the focued field's content.
  EXPECT_CALL(*text_input_delegate_, UpdateInput(_)).InSequence(in_sequence_);
  EXPECT_CALL(*kb_delegate, OnBeginFrame()).InSequence(in_sequence_);
  EXPECT_CALL(*kb_delegate, SetTransform(_)).InSequence(in_sequence_);
  content->OnFocusChanged(true);
  EXPECT_TRUE(OnBeginFrame());

  // Updates from the browser should update keyboard state.
  TextInputInfo info(base::ASCIIToUTF16("asdfg"));
  info.selection_start = 1;
  info.selection_end = 1;
  info.composition_start = 0;
  info.composition_end = 1;
  EXPECT_CALL(*text_input_delegate_, UpdateInput(info))
      .InSequence(in_sequence_);
  EXPECT_CALL(*kb_delegate, OnBeginFrame()).InSequence(in_sequence_);
  EXPECT_CALL(*kb_delegate, SetTransform(_)).InSequence(in_sequence_);
  ui_->UpdateWebInputSelectionIndices(1, 1);
  ui_->UpdateWebInputCompositionIndices(0, 1);
  ui_->UpdateWebInputText(base::ASCIIToUTF16("asdfg"));
  EXPECT_TRUE(OnBeginFrame());

  // End editing.
  EXPECT_CALL(*kb_delegate, HideKeyboard()).InSequence(in_sequence_);
  EXPECT_CALL(*kb_delegate, OnBeginFrame()).InSequence(in_sequence_);
  EXPECT_CALL(*kb_delegate, SetTransform(_)).InSequence(in_sequence_);
  ui_->ShowSoftInput(false);
  EXPECT_TRUE(OnBeginFrame());
}

}  // namespace vr
