// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_container_view.h"

#include <utility>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/services/assistant/test_support/mock_assistant.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace ash {

namespace {

constexpr int kMarginBottomDip = 8;

class AssistantContainerViewTest : public AshTestBase {
 public:
  AssistantContainerViewTest() = default;
  ~AssistantContainerViewTest() override = default;

  void SetUp() override {
    // Enable Assistant feature.
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::switches::kAssistantFeature);
    ASSERT_TRUE(chromeos::switches::IsAssistantEnabled());

    AshTestBase::SetUp();

    // Enable Assistant in settings.
    Shell::Get()->voice_interaction_controller()->NotifySettingsEnabled(true);

    // Cache controller.
    controller_ = Shell::Get()->assistant_controller();
    DCHECK(controller_);

    // Cache UI controller.
    ui_controller_ = controller_->ui_controller();
    DCHECK(ui_controller_);

    SetUpMocks();

    // After mocks are set up our Assistant service is ready for use. Indicate
    // this by changing status from NOT_READY to STOPPED.
    Shell::Get()->voice_interaction_controller()->NotifyStatusChanged(
        mojom::VoiceInteractionState::STOPPED);
  }

  AssistantUiController* ui_controller() { return ui_controller_; }

 private:
  void SetUpMocks() {
    // Mock the Assistant service.
    assistant_ = std::make_unique<chromeos::assistant::MockAssistant>();
    assistant_binding_ =
        std::make_unique<mojo::Binding<chromeos::assistant::mojom::Assistant>>(
            assistant_.get());
    chromeos::assistant::mojom::AssistantPtr assistant;
    assistant_binding_->Bind(mojo::MakeRequest(&assistant));
    controller_->SetAssistant(std::move(assistant));

    // Mock any screen context cache requests by immediately invoking callback.
    ON_CALL(*assistant_, DoCacheScreenContext(testing::_))
        .WillByDefault(testing::Invoke(
            [](base::OnceClosure* callback) { std::move(*callback).Run(); }));
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<chromeos::assistant::MockAssistant> assistant_;

  std::unique_ptr<mojo::Binding<chromeos::assistant::mojom::Assistant>>
      assistant_binding_;

  AssistantController* controller_ = nullptr;
  AssistantUiController* ui_controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AssistantContainerViewTest);
};

}  // namespace

TEST_F(AssistantContainerViewTest, InitialAnchoring) {
  // Guarantee short but non-zero duration for animations.
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Show Assistant UI and grab a reference to our view under test.
  ui_controller()->ShowUi(AssistantEntryPoint::kUnspecified);
  AssistantContainerView* view = ui_controller()->GetViewForTest();

  // We expect the view to appear in the work area where new windows will open.
  gfx::Rect expected_work_area =
      display::Screen::GetScreen()
          ->GetDisplayMatching(
              Shell::Get()->GetRootWindowForNewWindows()->GetBoundsInScreen())
          .work_area();

  // We expect the view to be horizontally centered and bottom aligned.
  gfx::Rect expected_bounds = gfx::Rect(expected_work_area);
  expected_bounds.ClampToCenteredSize(view->size());
  expected_bounds.set_y(expected_work_area.bottom() - view->height() -
                        kMarginBottomDip);

  ASSERT_EQ(expected_bounds, view->GetBoundsInScreen());
}

}  // namespace ash
