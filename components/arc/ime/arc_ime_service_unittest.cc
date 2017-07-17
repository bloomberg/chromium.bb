// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/ime/arc_ime_service.h"

#include <memory>
#include <set>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/arc/arc_bridge_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/dummy_input_method.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace arc {

namespace {

class FakeArcImeBridge : public ArcImeBridge {
 public:
  FakeArcImeBridge() : count_send_insert_text_(0) {}

  void SendSetCompositionText(const ui::CompositionText& composition) override {
  }
  void SendConfirmCompositionText() override {
  }
  void SendInsertText(const base::string16& text) override {
    count_send_insert_text_++;
  }
  void SendOnKeyboardBoundsChanging(const gfx::Rect& new_bounds) override {
  }
  void SendExtendSelectionAndDelete(size_t before, size_t after) override {
  }

  int count_send_insert_text() const { return count_send_insert_text_; }

 private:
  int count_send_insert_text_;
};

class FakeInputMethod : public ui::DummyInputMethod {
 public:
  FakeInputMethod() : client_(nullptr),
                      count_show_ime_if_needed_(0),
                      count_cancel_composition_(0),
                      count_set_focused_text_input_client_(0) {}

  void SetFocusedTextInputClient(ui::TextInputClient* client) override {
    count_set_focused_text_input_client_++;
    client_ = client;
  }

  ui::TextInputClient* GetTextInputClient() const override {
    return client_;
  }

  void ShowImeIfNeeded() override {
    count_show_ime_if_needed_++;
  }

  void CancelComposition(const ui::TextInputClient* client) override {
    if (client == client_)
      count_cancel_composition_++;
  }

  void DetachTextInputClient(ui::TextInputClient* client) override {
    if (client_ == client)
      client_ = nullptr;
  }

  int count_show_ime_if_needed() const {
    return count_show_ime_if_needed_;
  }

  int count_cancel_composition() const {
    return count_cancel_composition_;
  }

  int count_set_focused_text_input_client() const {
    return count_set_focused_text_input_client_;
  }

 private:
  ui::TextInputClient* client_;
  int count_show_ime_if_needed_;
  int count_cancel_composition_;
  int count_set_focused_text_input_client_;
};

// Helper class for testing the window focus tracking feature of ArcImeService,
// not depending on the full setup of Exo and Ash.
class FakeArcWindowDelegate : public ArcImeService::ArcWindowDelegate {
 public:
  FakeArcWindowDelegate() : next_id_(0) {}

  bool IsArcWindow(const aura::Window* window) const override {
    return arc_window_id_.count(window->id());
  }

  void RegisterFocusObserver() override {}
  void UnregisterFocusObserver() override {}

  std::unique_ptr<aura::Window> CreateFakeArcWindow() {
    const int id = next_id_++;
    arc_window_id_.insert(id);
    return base::WrapUnique(aura::test::CreateTestWindowWithDelegate(
        &dummy_delegate_, id, gfx::Rect(), nullptr));
  }

  std::unique_ptr<aura::Window> CreateFakeNonArcWindow() {
    const int id = next_id_++;
    return base::WrapUnique(aura::test::CreateTestWindowWithDelegate(
        &dummy_delegate_, id, gfx::Rect(), nullptr));
  }

 private:
  aura::test::TestWindowDelegate dummy_delegate_;
  int next_id_;
  std::set<int> arc_window_id_;
};

}  // namespace

class ArcImeServiceTest : public testing::Test {
 public:
  ArcImeServiceTest() = default;

 protected:
  std::unique_ptr<ArcBridgeService> arc_bridge_service_;
  std::unique_ptr<FakeInputMethod> fake_input_method_;
  std::unique_ptr<ArcImeService> instance_;
  FakeArcImeBridge* fake_arc_ime_bridge_;  // Owned by |instance_|

  FakeArcWindowDelegate* fake_window_delegate_;  // Owned by |instance_|
  std::unique_ptr<aura::Window> arc_win_;

 private:
  void SetUp() override {
    arc_bridge_service_ = base::MakeUnique<ArcBridgeService>();
    instance_ =
        base::MakeUnique<ArcImeService>(nullptr, arc_bridge_service_.get());
    fake_arc_ime_bridge_ = new FakeArcImeBridge();
    instance_->SetImeBridgeForTesting(base::WrapUnique(fake_arc_ime_bridge_));

    fake_input_method_ = base::MakeUnique<FakeInputMethod>();
    instance_->SetInputMethodForTesting(fake_input_method_.get());

    fake_window_delegate_ = new FakeArcWindowDelegate();
    instance_->SetArcWindowDelegateForTesting(
        base::WrapUnique(fake_window_delegate_));
    arc_win_ = fake_window_delegate_->CreateFakeArcWindow();
  }

  void TearDown() override {
    arc_win_.reset();
    fake_window_delegate_ = nullptr;
    fake_arc_ime_bridge_ = nullptr;
    instance_.reset();
    arc_bridge_service_.reset();
  }
};

TEST_F(ArcImeServiceTest, HasCompositionText) {
  instance_->OnWindowFocused(arc_win_.get(), nullptr);

  ui::CompositionText composition;
  composition.text = base::UTF8ToUTF16("nonempty text");

  EXPECT_FALSE(instance_->HasCompositionText());

  instance_->SetCompositionText(composition);
  EXPECT_TRUE(instance_->HasCompositionText());
  instance_->ClearCompositionText();
  EXPECT_FALSE(instance_->HasCompositionText());

  instance_->SetCompositionText(composition);
  EXPECT_TRUE(instance_->HasCompositionText());
  instance_->ConfirmCompositionText();
  EXPECT_FALSE(instance_->HasCompositionText());

  instance_->SetCompositionText(composition);
  EXPECT_TRUE(instance_->HasCompositionText());
  instance_->InsertText(base::UTF8ToUTF16("another text"));
  EXPECT_FALSE(instance_->HasCompositionText());

  instance_->SetCompositionText(composition);
  EXPECT_TRUE(instance_->HasCompositionText());
  instance_->SetCompositionText(ui::CompositionText());
  EXPECT_FALSE(instance_->HasCompositionText());
}

TEST_F(ArcImeServiceTest, ShowImeIfNeeded) {
  instance_->OnWindowFocused(arc_win_.get(), nullptr);

  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_NONE);
  ASSERT_EQ(0, fake_input_method_->count_show_ime_if_needed());

  // Text input type change does not imply the show ime request.
  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(0, fake_input_method_->count_show_ime_if_needed());

  instance_->ShowImeIfNeeded();
  EXPECT_EQ(1, fake_input_method_->count_show_ime_if_needed());
}

TEST_F(ArcImeServiceTest, CancelComposition) {
  instance_->OnWindowFocused(arc_win_.get(), nullptr);

  // The bridge should forward the cancel event to the input method.
  instance_->OnCancelComposition();
  EXPECT_EQ(1, fake_input_method_->count_cancel_composition());
}

TEST_F(ArcImeServiceTest, InsertChar) {
  instance_->OnWindowFocused(arc_win_.get(), nullptr);

  // When text input type is NONE, the event is not forwarded.
  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_NONE);
  instance_->InsertChar(ui::KeyEvent('a', ui::VKEY_A, 0));
  EXPECT_EQ(0, fake_arc_ime_bridge_->count_send_insert_text());

  // When the bridge is accepting text inputs, forward the event.
  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_TEXT);
  instance_->InsertChar(ui::KeyEvent('a', ui::VKEY_A, 0));
  EXPECT_EQ(1, fake_arc_ime_bridge_->count_send_insert_text());
}

TEST_F(ArcImeServiceTest, WindowFocusTracking) {
  std::unique_ptr<aura::Window> arc_win2 =
      fake_window_delegate_->CreateFakeArcWindow();
  std::unique_ptr<aura::Window> nonarc_win =
      fake_window_delegate_->CreateFakeNonArcWindow();

  // ARC window is focused. ArcImeService is set as the text input client.
  instance_->OnWindowFocused(arc_win_.get(), nullptr);
  EXPECT_EQ(instance_.get(), fake_input_method_->GetTextInputClient());
  EXPECT_EQ(1, fake_input_method_->count_set_focused_text_input_client());

  // Focus is moving between ARC windows. No state change should happen.
  instance_->OnWindowFocused(arc_win2.get(), arc_win_.get());
  EXPECT_EQ(instance_.get(), fake_input_method_->GetTextInputClient());
  EXPECT_EQ(1, fake_input_method_->count_set_focused_text_input_client());

  // Focus moved to a non-ARC window. ArcImeService is detached.
  instance_->OnWindowFocused(nonarc_win.get(), arc_win2.get());
  EXPECT_EQ(nullptr, fake_input_method_->GetTextInputClient());
  EXPECT_EQ(1, fake_input_method_->count_set_focused_text_input_client());

  // Focus came back to an ARC window. ArcImeService is re-attached.
  instance_->OnWindowFocused(arc_win_.get(), nonarc_win.get());
  EXPECT_EQ(instance_.get(), fake_input_method_->GetTextInputClient());
  EXPECT_EQ(2, fake_input_method_->count_set_focused_text_input_client());

  // Focus is moving out.
  instance_->OnWindowFocused(nullptr, arc_win_.get());
  EXPECT_EQ(nullptr, fake_input_method_->GetTextInputClient());
  EXPECT_EQ(2, fake_input_method_->count_set_focused_text_input_client());
}

TEST_F(ArcImeServiceTest, GetTextFromRange) {
  instance_->OnWindowFocused(arc_win_.get(), nullptr);

  const base::string16 text = base::ASCIIToUTF16("abcdefghijklmn");
  // Assume the cursor is between 'c' and 'd'.
  const uint32_t cursor_pos = 3;
  const gfx::Range text_range(cursor_pos - 1, cursor_pos + 1);
  const base::string16 text_in_range = text.substr(cursor_pos - 1, 2);
  const gfx::Range selection_range(cursor_pos, cursor_pos);

  instance_->OnCursorRectChangedWithSurroundingText(
      gfx::Rect(0, 0, 1, 1), text_range, text_in_range, selection_range);

  gfx::Range temp;
  instance_->GetTextRange(&temp);
  EXPECT_EQ(text_range, temp);

  base::string16 temp_str;
  instance_->GetTextFromRange(text_range, &temp_str);
  EXPECT_EQ(text_in_range, temp_str);

  instance_->GetSelectionRange(&temp);
  EXPECT_EQ(selection_range, temp);
}

}  // namespace arc
