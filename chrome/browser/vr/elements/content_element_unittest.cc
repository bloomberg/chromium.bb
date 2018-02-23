// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_input_manager.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/elements/content_element.h"
#include "chrome/browser/vr/elements/keyboard.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/test/mock_keyboard_delegate.h"
#include "chrome/browser/vr/test/mock_text_input_delegate.h"
#include "chrome/browser/vr/test/ui_test.h"
#include "chrome/browser/vr/text_edit_action.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::StrictMock;

namespace vr {

class TestContentInputDelegate : public MockContentInputDelegate {
 public:
  TestContentInputDelegate() { info_ = TextInputInfo(); }

  void SetTextInputInfo(const TextInputInfo& info) { info_ = info; }

  void OnWebInputIndicesChanged(
      int selection_start,
      int selection_end,
      int composition_start,
      int compositon_end,
      base::OnceCallback<void(const TextInputInfo&)> callback) override {
    info_.selection_start = selection_start;
    info_.selection_end = selection_end;
    info_.composition_start = composition_start;
    info_.composition_end = compositon_end;
    std::move(callback).Run(info_);
  }

 private:
  TextInputInfo info_;
};

class ContentElementSceneTest : public UiTest {
 public:
  void SetUp() override {
    UiTest::SetUp();

    UiInitialState state;
    state.in_cct = false;
    state.in_web_vr = false;
    auto content_input_delegate =
        std::make_unique<testing::NiceMock<TestContentInputDelegate>>();
    CreateSceneInternal(state, std::move(content_input_delegate));

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
  static_cast<TestContentInputDelegate*>(content_input_delegate_)
      ->SetTextInputInfo(info);
  info.selection_start = 1;
  info.selection_end = 1;
  info.composition_start = 0;
  info.composition_end = 1;
  EXPECT_CALL(*text_input_delegate_, UpdateInput(info))
      .InSequence(in_sequence_);
  EXPECT_CALL(*kb_delegate, OnBeginFrame()).InSequence(in_sequence_);
  EXPECT_CALL(*kb_delegate, SetTransform(_)).InSequence(in_sequence_);
  ui_->UpdateWebInputIndices(1, 1, 0, 1);
  EXPECT_TRUE(OnBeginFrame());

  // End editing.
  EXPECT_CALL(*kb_delegate, HideKeyboard()).InSequence(in_sequence_);
  EXPECT_CALL(*kb_delegate, OnBeginFrame()).InSequence(in_sequence_);
  EXPECT_CALL(*kb_delegate, SetTransform(_)).InSequence(in_sequence_);
  ui_->ShowSoftInput(false);
  EXPECT_TRUE(OnBeginFrame());
}

class TestContentInputForwarder : public ContentInputForwarder {
 public:
  TestContentInputForwarder() {}
  ~TestContentInputForwarder() override {}

  void ForwardEvent(std::unique_ptr<blink::WebInputEvent>, int) override {}
  void ForwardDialogEvent(std::unique_ptr<blink::WebInputEvent>) override {}

  void OnWebInputEdited(const TextEdits& edits) override { edits_ = edits; }
  void SubmitWebInput() override {}
  void RequestWebInputText(TextStateUpdateCallback) override {
    text_state_requested_ = true;
  }

  TextEdits edits() { return edits_; }
  bool text_state_requested() { return text_state_requested_; }

  void Reset() {
    edits_.clear();
    text_state_requested_ = false;
  }

 private:
  TextEdits edits_;
  bool text_state_requested_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestContentInputForwarder);
};

class ContentElementInputEditingTest : public UiTest {
 public:
  void SetUp() override {
    UiTest::SetUp();

    CreateScene(kNotInCct, kNotInWebVr);

    text_input_delegate_ =
        std::make_unique<StrictMock<MockTextInputDelegate>>();
    input_forwarder_ = std::make_unique<TestContentInputForwarder>();
    content_delegate_ = ui_->GetContentInputDelegateForTest();
    content_delegate_->SetContentInputForwarderForTest(input_forwarder_.get());

    content_ =
        static_cast<ContentElement*>(scene_->GetUiElementByName(kContentQuad));
    content_->SetTextInputDelegate(text_input_delegate_.get());
    OnBeginFrame();
  }

 protected:
  TextInputInfo GetInputInfo(const std::string& text,
                             int selection_start,
                             int selection_end,
                             int composition_start,
                             int composition_end) {
    return TextInputInfo(base::UTF8ToUTF16(text), selection_start,
                         selection_end, composition_start, composition_end);
  }

  TextInputInfo GetInputInfo(const std::string& text,
                             int selection_start,
                             int selection_end) {
    return GetInputInfo(text, selection_start, selection_end, -1, -1);
  }

  void SetInput(const TextInputInfo& current, const TextInputInfo& previous) {
    EditedText info;
    info.current = current;
    info.previous = previous;
    content_->OnInputEdited(info);
  }

  std::unique_ptr<StrictMock<MockTextInputDelegate>> text_input_delegate_;
  std::unique_ptr<TestContentInputForwarder> input_forwarder_;
  ContentInputDelegate* content_delegate_;
  ContentElement* content_;
};

// Test that the diff between the current and previous edited text is calculated
// correctly.
TEST_F(ContentElementInputEditingTest, CommitDiff) {
  // Add a character.
  SetInput(GetInputInfo("a", 1, 1), GetInputInfo("", 0, 0));
  auto edits = input_forwarder_->edits();
  EXPECT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::COMMIT_TEXT,
                                     base::UTF8ToUTF16("a"), 1));

  // Add more characters.
  SetInput(GetInputInfo("asdf", 4, 4), GetInputInfo("a", 1, 1));
  edits = input_forwarder_->edits();
  EXPECT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::COMMIT_TEXT,
                                     base::UTF8ToUTF16("sdf"), 3));

  // Delete a character.
  SetInput(GetInputInfo("asd", 3, 3), GetInputInfo("asdf", 4, 4));
  edits = input_forwarder_->edits();
  EXPECT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::DELETE_TEXT,
                                     base::UTF8ToUTF16(""), -1));

  // Add characters while the cursor is not at the end.
  SetInput(GetInputInfo("asqwed", 5, 5), GetInputInfo("asd", 2, 2));
  edits = input_forwarder_->edits();
  EXPECT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::COMMIT_TEXT,
                                     base::UTF8ToUTF16("qwe"), 3));
}

TEST_F(ContentElementInputEditingTest, CommitDiffWithSelection) {
  // There was a selection and the new text is shorter than the selection text.
  SetInput(GetInputInfo("This a text", 6, 6),
           GetInputInfo("This is text", 5, 7));
  auto edits = input_forwarder_->edits();
  EXPECT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::COMMIT_TEXT,
                                     base::UTF8ToUTF16("a"), 1));

  // There was a selection and the new text is longer than the selection text.
  // This could happen when the user clicks on a keyboard suggestion.
  SetInput(GetInputInfo("This was the text", 12, 12),
           GetInputInfo("This is text", 5, 7));
  edits = input_forwarder_->edits();
  EXPECT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::COMMIT_TEXT,
                                     base::UTF8ToUTF16("was the"), 7));
  // There was a selection and the new text is of the same length as the
  // selection.
  SetInput(GetInputInfo("This ha text", 7, 7),
           GetInputInfo("This is text", 5, 7));
  edits = input_forwarder_->edits();
  EXPECT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::COMMIT_TEXT,
                                     base::UTF8ToUTF16("ha"), 2));
}

TEST_F(ContentElementInputEditingTest, IndicesUpdated) {
  // If the changed indices match with that from the last keyboard edit, the
  // given callback is triggered right away.
  SetInput(GetInputInfo("asdf", 4, 4), GetInputInfo("", 0, 0));
  TextInputInfo model_actual;
  TextInputInfo model_exp(base::UTF8ToUTF16("asdf"));
  content_delegate_->OnWebInputIndicesChanged(
      4, 4, -1, -1,
      base::BindOnce([](TextInputInfo* model,
                        const TextInputInfo& info) { *model = info; },
                     base::Unretained(&model_actual)));
  EXPECT_FALSE(input_forwarder_->text_state_requested());
  EXPECT_EQ(model_actual, model_exp);

  // If the changed indices do not match with that from the last keyboard edit,
  // the latest text state is requested for and the given callback is called
  // when the response arrives.
  content_delegate_->OnWebInputIndicesChanged(
      2, 2, -1, -1,
      base::BindOnce([](TextInputInfo* model,
                        const TextInputInfo& info) { *model = info; },
                     base::Unretained(&model_actual)));
  EXPECT_TRUE(input_forwarder_->text_state_requested());
  content_delegate_->OnWebInputTextChangedForTest(base::UTF8ToUTF16("asdf"));
  model_exp.selection_start = 2;
  model_exp.selection_end = 2;
  EXPECT_EQ(model_actual, model_exp);

  // Now verify that although the keyboard and web contents may have a different
  // view of the current text, a user-triggered update from the keyboard is
  // still sane from the user's point of view.

  // OnWebInputIndicesChanged is called, presumably due to a JS change, but the
  // indices are the same as those from the last update.
  input_forwarder_->Reset();
  TextInputInfo model;
  content_delegate_->OnWebInputIndicesChanged(
      2, 2, -1, -1,
      base::BindOnce([](TextInputInfo* model,
                        const TextInputInfo& info) { *model = info; },
                     base::Unretained(&model)));
  // We don't request the curent text state, so the keyboard and web contents
  // may be out of sync.
  EXPECT_FALSE(input_forwarder_->text_state_requested());
  EXPECT_EQ(model, TextInputInfo());
  // The user presses 'q', and the keyboard gives us an incorrect state, but
  // because we're calculating deltas, we still update web contents with a 'q'.
  SetInput(GetInputInfo("asqdf", 3, 3), GetInputInfo("asdf", 2, 2));
  auto edits = input_forwarder_->edits();
  EXPECT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::COMMIT_TEXT,
                                     base::UTF8ToUTF16("q"), 1));
}

TEST_F(ContentElementInputEditingTest, CompositionDiff) {
  // Start composition
  SetInput(GetInputInfo("a", 1, 1, 0, 1), GetInputInfo("", 0, 0));
  auto edits = input_forwarder_->edits();
  EXPECT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::SET_COMPOSING_TEXT,
                                     base::UTF8ToUTF16("a"), 1));

  // Add more characters.
  SetInput(GetInputInfo("asdf", 4, 4, 0, 4), GetInputInfo("a", 1, 1, 0, 1));
  edits = input_forwarder_->edits();
  EXPECT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::SET_COMPOSING_TEXT,
                                     base::UTF8ToUTF16("asdf"), 3));

  // Delete a few characters.
  SetInput(GetInputInfo("as", 2, 2, 0, 2), GetInputInfo("asdf", 4, 4, 0, 4));
  edits = input_forwarder_->edits();
  EXPECT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::SET_COMPOSING_TEXT,
                                     base::UTF8ToUTF16("as"), -2));

  // Finish composition.
  SetInput(GetInputInfo("as ", 3, 3, -1, -1), GetInputInfo("as", 2, 2, 0, 2));
  edits = input_forwarder_->edits();
  EXPECT_EQ(edits.size(), 2u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::CLEAR_COMPOSING_TEXT));
  EXPECT_EQ(edits[1], TextEditAction(TextEditActionType::COMMIT_TEXT,
                                     base::UTF8ToUTF16("as "), 3));

  // Finish composition, but the text is different. This could happen when the
  // user hits a suggestion that's different from the current composition, but
  // has the same length.
  SetInput(GetInputInfo("lk", 2, 2, -1, -1), GetInputInfo("as", 2, 2, 0, 2));
  edits = input_forwarder_->edits();
  EXPECT_EQ(edits.size(), 2u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::CLEAR_COMPOSING_TEXT));
  EXPECT_EQ(edits[1], TextEditAction(TextEditActionType::COMMIT_TEXT,
                                     base::UTF8ToUTF16("lk"), 2));

  // Finish composition, but the new text is shorter than the previous
  // composition. This could happen when the user hits a suggestion that's
  // shorter than the text they were composing.
  SetInput(GetInputInfo("hi hello", 2, 2, -1, -1),
           GetInputInfo("hii hello", 3, 3, 0, 3));
  edits = input_forwarder_->edits();
  EXPECT_EQ(edits.size(), 2u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::CLEAR_COMPOSING_TEXT));
  EXPECT_EQ(edits[1], TextEditAction(TextEditActionType::COMMIT_TEXT,
                                     base::UTF8ToUTF16("hi"), 2));
}

}  // namespace vr
