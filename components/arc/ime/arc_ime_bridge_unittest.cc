// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/arc/ime/arc_ime_bridge.h"
#include "components/arc/test/fake_arc_bridge_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/dummy_input_method.h"

namespace arc {

namespace {

class FakeArcImeIpcHost : public ArcImeIpcHost {
 public:
  void SendSetCompositionText(const ui::CompositionText& composition) override {
  }
  void SendConfirmCompositionText() override {
  }
  void SendInsertText(const base::string16& text) override {
  }
};

class FakeInputMethod : public ui::DummyInputMethod {
 public:
  FakeInputMethod() : client_(nullptr), count_show_ime_if_needed_(0) {}

  void SetFocusedTextInputClient(ui::TextInputClient* client) override {
    client_ = client;
  }

  ui::TextInputClient* GetTextInputClient() const override {
    return client_;
  }

  void ShowImeIfNeeded() override {
    count_show_ime_if_needed_++;
  }

  int count_show_ime_if_needed() const {
    return count_show_ime_if_needed_;
  }

 private:
  ui::TextInputClient* client_;
  int count_show_ime_if_needed_;
};

}  // namespace

class ArcImeBridgeTest : public testing::Test {
 public:
  ArcImeBridgeTest() {}

 protected:
  scoped_ptr<FakeArcBridgeService> fake_arc_bridge_service_;
  scoped_ptr<FakeInputMethod> fake_input_method_;
  scoped_ptr<ArcImeBridge> instance_;

 private:
  void SetUp() override {
    fake_arc_bridge_service_.reset(new FakeArcBridgeService);
    instance_.reset(new ArcImeBridge(fake_arc_bridge_service_.get()));
    instance_->SetIpcHostForTesting(make_scoped_ptr(new FakeArcImeIpcHost));

    fake_input_method_.reset(new FakeInputMethod);
    instance_->SetInputMethodForTesting(fake_input_method_.get());
  }

  void TearDown() override {
    instance_.reset();
    fake_arc_bridge_service_.reset();
  }
};

TEST_F(ArcImeBridgeTest, HasCompositionText) {
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

TEST_F(ArcImeBridgeTest, ShowImeIfNeeded) {
  fake_input_method_->SetFocusedTextInputClient(instance_.get());
  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_NONE);
  ASSERT_EQ(0, fake_input_method_->count_show_ime_if_needed());

  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(1, fake_input_method_->count_show_ime_if_needed());

  // The type is not changing, hence no call.
  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(1, fake_input_method_->count_show_ime_if_needed());

  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_SEARCH);
  EXPECT_EQ(2, fake_input_method_->count_show_ime_if_needed());

  // Change to NONE should not trigger the showing event.
  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_NONE);
  EXPECT_EQ(2, fake_input_method_->count_show_ime_if_needed());
}

}  // namespace arc
