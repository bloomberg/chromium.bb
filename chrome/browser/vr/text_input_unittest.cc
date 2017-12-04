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
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/keyboard_delegate.h"
#include "chrome/browser/vr/model/camera_model.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/test/ui_test.h"
#include "chrome/browser/vr/text_input_delegate.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "chrome/browser/vr/ui_scene_creator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::StrictMock;

namespace vr {

namespace {

#if defined(OS_LINUX)
// The purpose here is to catch forgetting to update operator==. Ideally this
// should go in TextInputInfo, but the size of base::string16 varies accross
// platforms (i.e 24 bytes on linux and 20 bytes on android), so we can't add it
// there in a generic way.
static constexpr size_t kTextInputInfoSize = 32;
static_assert(kTextInputInfoSize == sizeof(TextInputInfo),
              "If new fields are added to TextInputInfo, we must explicitly "
              "bump this size and update operator== below");
#endif

}  // namespace

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

class TextInputTest : public UiTest {
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

TEST_F(TextInputTest, InputFieldFocus) {
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

TEST_F(TextInputTest, InputFieldEdit) {
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

}  // namespace vr
