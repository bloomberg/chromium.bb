// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_controller.h"

#include "ash/highlighter/highlighter_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/chromeos_switches.h"

namespace ash {

class AssistantControllerTest : public AshTestBase {
 protected:
  AssistantControllerTest() = default;
  ~AssistantControllerTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::switches::kAssistantFeature);
    ASSERT_TRUE(chromeos::switches::IsAssistantEnabled());
    AshTestBase::SetUp();
    controller_ = Shell::Get()->assistant_controller();
    DCHECK(controller_);
  }

  const AssistantInteractionModel* interaction_model() {
    return controller_->interaction_model();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  AssistantController* controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AssistantControllerTest);
};

TEST_F(AssistantControllerTest, HighlighterEnabledStatus) {
  HighlighterController* highlighter_controller =
      Shell::Get()->highlighter_controller();
  highlighter_controller->UpdateEnabledState(HighlighterEnabledState::kEnabled);
  EXPECT_EQ(InputModality::kStylus, interaction_model()->input_modality());
  EXPECT_EQ(InteractionState::kActive,
            interaction_model()->interaction_state());

  // Metalayer mode session end should keep interaction state active.
  highlighter_controller->UpdateEnabledState(
      HighlighterEnabledState::kDisabledBySessionEnd);
  EXPECT_EQ(InteractionState::kActive,
            interaction_model()->interaction_state());

  // Disabling directly by user action should make interaction state inactive.
  highlighter_controller->UpdateEnabledState(
      HighlighterEnabledState::kDisabledByUser);
  EXPECT_EQ(InteractionState::kInactive,
            interaction_model()->interaction_state());
}

}  // namespace ash
