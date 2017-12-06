// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_input_manager.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/vr/databinding/binding.h"
#include "chrome/browser/vr/elements/keyboard.h"
#include "chrome/browser/vr/elements/text.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/keyboard_delegate.h"
#include "chrome/browser/vr/model/camera_model.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/test/ui_test.h"
#include "chrome/browser/vr/text_input_delegate.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "chrome/browser/vr/ui_scene_creator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "ui/gfx/render_text.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::StrictMock;

namespace vr {

class MockTextInputDelegate : public TextInputDelegate {
 public:
  MockTextInputDelegate() = default;
  ~MockTextInputDelegate() override = default;

  MOCK_METHOD1(UpdateInput, void(const TextInputInfo& info));
  MOCK_METHOD1(RequestFocus, void(int));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTextInputDelegate);
};

class MockKeyboardDelegate : public KeyboardDelegate {
 public:
  MockKeyboardDelegate() = default;
  ~MockKeyboardDelegate() override = default;

  MOCK_METHOD0(ShowKeyboard, void());
  MOCK_METHOD0(HideKeyboard, void());
  MOCK_METHOD1(SetTransform, void(const gfx::Transform&));
  MOCK_METHOD3(HitTest,
               bool(const gfx::Point3F&, const gfx::Point3F&, gfx::Point3F*));
  MOCK_METHOD0(OnBeginFrame, void());
  MOCK_METHOD1(Draw, void(const CameraModel&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockKeyboardDelegate);
};

class TextInputSceneTest : public UiTest {
 public:
  void SetUp() override {
    UiTest::SetUp();
    CreateScene(kNotInCct, kNotInWebVr);

    // Make test text input.
    text_input_delegate_ =
        base::MakeUnique<StrictMock<MockTextInputDelegate>>();
    text_input_info_ = base::MakeUnique<TextInputInfo>();
    auto text_input = UiSceneCreator::CreateTextInput(
        512, 1, model_, text_input_info_.get(), text_input_delegate_.get());
    text_input_ = text_input.get();
    scene_->AddUiElement(k2dBrowsingForeground, std::move(text_input));
    EXPECT_TRUE(OnBeginFrame());
  }

 protected:
  TextInput* text_input_;
  std::unique_ptr<StrictMock<MockTextInputDelegate>> text_input_delegate_;
  std::unique_ptr<TextInputInfo> text_input_info_;
  testing::Sequence in_sequence_;
};

TEST_F(TextInputSceneTest, InputFieldFocus) {
  // Set mock delegates.
  auto* kb = static_cast<Keyboard*>(scene_->GetUiElementByName(kKeyboard));
  auto kb_delegate = base::MakeUnique<StrictMock<MockKeyboardDelegate>>();
  EXPECT_CALL(*kb_delegate, HideKeyboard()).InSequence(in_sequence_);
  kb->SetKeyboardDelegate(kb_delegate.get());

  // Clicking on the text field should request focus.
  EXPECT_CALL(*text_input_delegate_, RequestFocus(_)).InSequence(in_sequence_);
  text_input_->OnButtonUp(gfx::PointF());

  // Focusing on an input field should show the keyboard and tell the delegate
  // the field's content.
  EXPECT_CALL(*text_input_delegate_, UpdateInput(_)).InSequence(in_sequence_);
  text_input_->OnFocusChanged(true);
  EXPECT_CALL(*kb_delegate, ShowKeyboard()).InSequence(in_sequence_);
  EXPECT_CALL(*kb_delegate, OnBeginFrame()).InSequence(in_sequence_);
  EXPECT_CALL(*kb_delegate, SetTransform(_)).InSequence(in_sequence_);
  EXPECT_TRUE(OnBeginFrame());

  // Focusing out of an input field should hide the keyboard.
  text_input_->OnFocusChanged(false);
  EXPECT_CALL(*kb_delegate, HideKeyboard()).InSequence(in_sequence_);
  EXPECT_CALL(*kb_delegate, OnBeginFrame()).InSequence(in_sequence_);
  EXPECT_CALL(*kb_delegate, SetTransform(_)).InSequence(in_sequence_);
  EXPECT_TRUE(OnBeginFrame());
}

TEST_F(TextInputSceneTest, InputFieldEdit) {
  // UpdateInput should not be called if the text input doesn't have focus.
  EXPECT_CALL(*text_input_delegate_, UpdateInput(_))
      .Times(0)
      .InSequence(in_sequence_);
  text_input_->OnFocusChanged(false);

  // Focus on input field.
  EXPECT_CALL(*text_input_delegate_, UpdateInput(_)).InSequence(in_sequence_);
  text_input_->OnFocusChanged(true);

  // Edits from the keyboard update the underlying text input  model.
  EXPECT_CALL(*text_input_delegate_, UpdateInput(_)).InSequence(in_sequence_);
  TextInputInfo info(base::ASCIIToUTF16("asdfg"));
  text_input_->OnInputEdited(info);
  EXPECT_TRUE(OnBeginFrame());
  EXPECT_EQ(info, *text_input_info_);
}

TEST_F(TextInputSceneTest, ClickOnTextGrabsFocus) {
  EXPECT_CALL(*text_input_delegate_, RequestFocus(_));
  text_input_->get_text_element()->OnButtonUp({0, 0});
}

TEST(TextInputTest, HintText) {
  UiScene scene;

  auto instance =
      base::MakeUnique<TextInput>(512, 10, TextInput::OnFocusChangedCallback(),
                                  TextInput::OnInputEditedCallback());
  instance->set_name(kOmniboxTextField);
  instance->SetSize(1, 0);
  TextInput* element = instance.get();
  scene.root_element().AddChild(std::move(instance));

  // Text field is empty, so we should be showing hint text.
  scene.OnBeginFrame(base::TimeTicks(), kForwardVector);
  EXPECT_GT(element->get_hint_element()->GetTargetOpacity(), 0);

  // When text enters the field, the hint should disappear.
  TextInputInfo info;
  info.text = base::UTF8ToUTF16("text");
  element->UpdateInput(info);
  scene.OnBeginFrame(base::TimeTicks(), kForwardVector);
  EXPECT_EQ(element->get_hint_element()->GetTargetOpacity(), 0);
}

TEST(TextInputTest, Cursor) {
  UiScene scene;

  auto instance =
      base::MakeUnique<TextInput>(512, 10, TextInput::OnFocusChangedCallback(),
                                  TextInput::OnInputEditedCallback());
  instance->set_name(kOmniboxTextField);
  instance->SetSize(1, 0);
  TextInput* element = instance.get();
  scene.root_element().AddChild(std::move(instance));

  // The cursor should not be blinking or visible.
  float initial = element->get_cursor_element()->GetTargetOpacity();
  EXPECT_EQ(initial, 0.f);
  for (int ms = 0; ms <= 2000; ms += 100) {
    scene.OnBeginFrame(MsToTicks(ms), kForwardVector);
    EXPECT_EQ(initial, element->get_cursor_element()->GetTargetOpacity());
  }

  // When focused, the cursor should start blinking.
  element->OnFocusChanged(true);
  initial = element->get_cursor_element()->GetTargetOpacity();
  bool toggled = false;
  for (int ms = 0; ms <= 2000; ms += 100) {
    scene.OnBeginFrame(MsToTicks(ms), kForwardVector);
    if (initial != element->get_cursor_element()->GetTargetOpacity())
      toggled = true;
  }
  EXPECT_TRUE(toggled);

// TODO(cjgrant): Continue with the test cases below, when they're debugged.
#if 0
  TextInputInfo info;
  info.text = base::UTF8ToUTF16("text");

  // When the cursor position moves, the cursor element should move.
  element->UpdateInput(info);
  auto result = element->get_text_element()->LayOutTextForTest({512, 512});
  auto position1 = element->get_text_element()->GetRawCursorBounds();
  info.selection_end = 1;
  element->UpdateInput(info);
  element->get_text_element()->LayOutTextForTest({512, 512});
  auto position2 = element->get_text_element()->GetRawCursorBounds();
  EXPECT_NE(position1, position2);
#endif
}

}  // namespace vr
