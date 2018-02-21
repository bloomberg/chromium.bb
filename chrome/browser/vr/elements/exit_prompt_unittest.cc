// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/exit_prompt.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/elements/exit_prompt_texture.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/test/gfx_util.h"

using ::testing::Return;

namespace vr {

namespace {

class MockExitPromptTexture : public ExitPromptTexture {
 public:
  MockExitPromptTexture() : ExitPromptTexture() {}
  ~MockExitPromptTexture() override {}

  MOCK_CONST_METHOD1(HitsPrimaryButton, bool(const gfx::PointF&));
  MOCK_CONST_METHOD1(HitsSecondaryButton, bool(const gfx::PointF&));

  MOCK_CONST_METHOD1(GetPreferredTextureSize, gfx::Size(int));
  MOCK_CONST_METHOD0(GetDrawnSize, gfx::SizeF());
  MOCK_METHOD2(Draw, void(SkCanvas*, const gfx::Size&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockExitPromptTexture);
};

}  // namespace

class TestExitPrompt : public ExitPrompt {
 public:
  TestExitPrompt();
  ~TestExitPrompt() override {}

  bool primary_button_pressed() const { return primary_button_pressed_; }
  bool secondary_button_pressed() const { return secondary_button_pressed_; }

 private:
  void OnButtonPressed(ExitPrompt::Button button, UiUnsupportedMode reason) {
    switch (button) {
      case ExitPrompt::NONE:
        break;
      case ExitPrompt::PRIMARY:
        primary_button_pressed_ = true;
        break;
      case ExitPrompt::SECONDARY:
        secondary_button_pressed_ = true;
        break;
    }
  }

  bool primary_button_pressed_ = false;
  bool secondary_button_pressed_ = false;
};

TestExitPrompt::TestExitPrompt()
    : ExitPrompt(512,
                 base::BindRepeating(&TestExitPrompt::OnButtonPressed,
                                     base::Unretained(this))) {}

TEST(ExitPromptTest, PrimaryButtonCallbackCalled) {
  TestExitPrompt prompt;
  auto texture = std::make_unique<MockExitPromptTexture>();
  // Called twice from OnButtonDown and twice from OnButtonUp.
  EXPECT_CALL(*texture, HitsPrimaryButton(gfx::PointF()))
      .Times(4)
      .WillRepeatedly(Return(true));
  // Called once from OnButtonDown and once from OnButtonUp (via OnStatUpdated).
  EXPECT_CALL(*texture, HitsSecondaryButton(gfx::PointF()))
      .Times(2)
      .WillRepeatedly(Return(false));
  prompt.SetTextureForTesting(std::move(texture));

  prompt.OnButtonDown(gfx::PointF());
  prompt.OnButtonUp(gfx::PointF());

  EXPECT_TRUE(prompt.primary_button_pressed());
  EXPECT_FALSE(prompt.secondary_button_pressed());
}

TEST(ExitPromptTest, SecondaryButtonCallbackCalled) {
  TestExitPrompt prompt;
  auto texture = std::make_unique<MockExitPromptTexture>();
  // Called twice from OnButtonDown and once from OnButtonUp.
  EXPECT_CALL(*texture, HitsPrimaryButton(gfx::PointF()))
      .Times(3)
      .WillRepeatedly(Return(false));
  // Called twice from OnButtonDown and twice from OnButtonUp.
  EXPECT_CALL(*texture, HitsSecondaryButton(gfx::PointF()))
      .Times(4)
      .WillRepeatedly(Return(true));
  prompt.SetTextureForTesting(std::move(texture));

  prompt.OnButtonDown(gfx::PointF());
  prompt.OnButtonUp(gfx::PointF());

  EXPECT_FALSE(prompt.primary_button_pressed());
  EXPECT_TRUE(prompt.secondary_button_pressed());
}

}  // namespace vr
