// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/voice_interaction/arc_voice_interaction_framework_service.h"

#include <memory>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/voice_interaction/highlighter_controller_client.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cras_audio_client.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_util.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/arc/test/fake_voice_interaction_framework_instance.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

class TestHighlighterController : public ash::mojom::HighlighterController {
 public:
  TestHighlighterController() : binding_(this), weak_factory_(this) {}
  ~TestHighlighterController() override = default;

  ash::mojom::HighlighterControllerPtr CreateInterfacePtrAndBind() {
    ash::mojom::HighlighterControllerPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

  void CallHandleSelection(const gfx::Rect& rect) {
    client_->HandleSelection(rect);
  }

  void CallHandleEnabledStateChange(bool enabled) {
    client_->HandleEnabledStateChange(enabled);
  }

  bool client_attached() const { return static_cast<bool>(client_); }

  // ash::mojom::HighlighterController:
  void SetClient(ash::mojom::HighlighterControllerClientPtr client) override {
    DCHECK(!client_);
    client_ = std::move(client);
    client_.set_connection_error_handler(
        base::Bind(&TestHighlighterController::OnClientConnectionLost,
                   weak_factory_.GetWeakPtr()));
  }

  void OnClientConnectionLost() { client_.reset(); }

 private:
  mojo::Binding<ash::mojom::HighlighterController> binding_;
  ash::mojom::HighlighterControllerClientPtr client_;

  base::WeakPtrFactory<TestHighlighterController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestHighlighterController);
};

}  // namespace

class ArcVoiceInteractionFrameworkServiceTest : public ash::AshTestBase {
 public:
  ArcVoiceInteractionFrameworkServiceTest() = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // Setup test profile.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("user@gmail.com");
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestArcProfile"));
    profile_ = profile_builder.Build();

    // Setup dependencies for voice interaction framework service.
    session_manager_ = std::make_unique<session_manager::SessionManager>();
    arc_session_manager_ = std::make_unique<ArcSessionManager>(
        std::make_unique<ArcSessionRunner>(base::Bind(FakeArcSession::Create)));
    arc_bridge_service_ = std::make_unique<ArcBridgeService>();
    highlighter_controller_ = std::make_unique<TestHighlighterController>();
    framework_service_ = std::make_unique<ArcVoiceInteractionFrameworkService>(
        profile_.get(), arc_bridge_service_.get());
    framework_service_->GetHighlighterClientForTesting()
        ->SetControllerForTesting(
            highlighter_controller_->CreateInterfacePtrAndBind());
    framework_instance_ =
        std::make_unique<FakeVoiceInteractionFrameworkInstance>();
    arc_bridge_service_->voice_interaction_framework()->SetInstance(
        framework_instance_.get());
    // Flushing is required for the AttachClient call to get through to the
    // highligther controller.
    FlushHighlighterControllerMojo();

    framework_service()->SetVoiceInteractionSetupCompleted();
  }

  void TearDown() override {
    framework_instance_.reset();
    framework_service_.reset();
    arc_bridge_service_.reset();
    arc_session_manager_.reset();
    session_manager_.reset();
    profile_.reset();
    AshTestBase::TearDown();
  }

 protected:
  ArcBridgeService* arc_bridge_service() { return arc_bridge_service_.get(); }

  ArcVoiceInteractionFrameworkService* framework_service() {
    return framework_service_.get();
  }

  FakeVoiceInteractionFrameworkInstance* framework_instance() {
    return framework_instance_.get();
  }

  TestHighlighterController* highlighter_controller() {
    return highlighter_controller_.get();
  }

  void FlushHighlighterControllerMojo() {
    framework_service_->GetHighlighterClientForTesting()->FlushMojoForTesting();
  }

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  std::unique_ptr<ArcBridgeService> arc_bridge_service_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<TestHighlighterController> highlighter_controller_;
  std::unique_ptr<ArcVoiceInteractionFrameworkService> framework_service_;
  std::unique_ptr<FakeVoiceInteractionFrameworkInstance> framework_instance_;

  DISALLOW_COPY_AND_ASSIGN(ArcVoiceInteractionFrameworkServiceTest);
};

TEST_F(ArcVoiceInteractionFrameworkServiceTest, StartSetupWizard) {
  framework_service()->StartVoiceInteractionSetupWizard();
  // The signal to start setup wizard should be sent.
  EXPECT_EQ(1u, framework_instance()->setup_wizard_count());
}

TEST_F(ArcVoiceInteractionFrameworkServiceTest, ShowSettings) {
  framework_service()->ShowVoiceInteractionSettings();
  // The signal to show voice interaction settings should be sent.
  EXPECT_EQ(1u, framework_instance()->show_settings_count());
}

TEST_F(ArcVoiceInteractionFrameworkServiceTest, StartSession) {
  framework_service()->StartSessionFromUserInteraction(gfx::Rect());
  // The signal to start voice interaction session should be sent.
  EXPECT_EQ(1u, framework_instance()->start_session_count());
}

TEST_F(ArcVoiceInteractionFrameworkServiceTest, StartSessionWithoutFlag) {
  // Remove the voice interaction enabled flag.
  framework_service()->SetVoiceInteractionEnabled(false,
                                                  base::BindOnce([](bool) {}));

  framework_service()->StartSessionFromUserInteraction(gfx::Rect());
  // The signal should not be sent when voice interaction disabled.
  EXPECT_EQ(0u, framework_instance()->start_session_count());
}

TEST_F(ArcVoiceInteractionFrameworkServiceTest, StartSessionWithoutInstance) {
  // Reset the framework instance.
  arc_bridge_service()->voice_interaction_framework()->SetInstance(nullptr);

  framework_service()->StartSessionFromUserInteraction(gfx::Rect());
  // The signal should not be sent when framework instance not ready.
  EXPECT_EQ(0u, framework_instance()->start_session_count());
}

TEST_F(ArcVoiceInteractionFrameworkServiceTest, ToggleSession) {
  framework_service()->ToggleSessionFromUserInteraction();
  // The signal to toggle voice interaction session should be sent.
  EXPECT_EQ(1u, framework_instance()->toggle_session_count());
}

TEST_F(ArcVoiceInteractionFrameworkServiceTest, HotwordTriggered) {
  auto* audio_client = static_cast<chromeos::FakeCrasAudioClient*>(
      chromeos::DBusThreadManager::Get()->GetCrasAudioClient());
  audio_client->NotifyHotwordTriggeredForTesting(0, 0);
  EXPECT_TRUE(framework_service()->ValidateTimeSinceUserInteraction());
}

TEST_F(ArcVoiceInteractionFrameworkServiceTest, HighlighterControllerClient) {
  EXPECT_TRUE(highlighter_controller()->client_attached());

  // Enabled state should propagate to the framework instance.
  highlighter_controller()->CallHandleEnabledStateChange(true);
  FlushHighlighterControllerMojo();
  EXPECT_EQ(1u, framework_instance()->set_metalayer_visibility_count());
  EXPECT_TRUE(framework_instance()->metalayer_visible());

  // Disabled state should propagate to the framework instance.
  framework_instance()->ResetCounters();
  highlighter_controller()->CallHandleEnabledStateChange(false);
  FlushHighlighterControllerMojo();
  EXPECT_EQ(1u, framework_instance()->set_metalayer_visibility_count());
  EXPECT_FALSE(framework_instance()->metalayer_visible());

  // Enable the state again.
  framework_instance()->ResetCounters();
  highlighter_controller()->CallHandleEnabledStateChange(true);
  FlushHighlighterControllerMojo();
  EXPECT_EQ(1u, framework_instance()->set_metalayer_visibility_count());
  EXPECT_TRUE(framework_instance()->metalayer_visible());

  // Simulate a valid selection.
  framework_instance()->ResetCounters();
  const gfx::Rect selection(100, 200, 300, 400);
  highlighter_controller()->CallHandleSelection(selection);
  highlighter_controller()->CallHandleEnabledStateChange(false);
  FlushHighlighterControllerMojo();
  // Neither the selected region nor the state update should reach the
  // framework instance yet.
  EXPECT_EQ(0u, framework_instance()->start_session_for_region_count());
  EXPECT_EQ(0u, framework_instance()->set_metalayer_visibility_count());
  EXPECT_TRUE(framework_instance()->metalayer_visible());
  framework_service()
      ->GetHighlighterClientForTesting()
      ->SimulateSelectionTimeoutForTesting();
  FlushHighlighterControllerMojo();
  // After a timeout, the selected region should reach the framework instance.
  EXPECT_EQ(1u, framework_instance()->start_session_for_region_count());
  EXPECT_EQ(selection.ToString(),
            framework_instance()->selected_region().ToString());
  // However, the state update should not be explicitly sent to the framework
  // instance, since the state change is implied with a valid selection.
  EXPECT_EQ(0u, framework_instance()->set_metalayer_visibility_count());

  // Clear the framework instance to simulate the container crash.
  // The client should become detached.
  arc_bridge_service()->voice_interaction_framework()->SetInstance(nullptr);
  FlushHighlighterControllerMojo();
  EXPECT_FALSE(highlighter_controller()->client_attached());

  // Set the framework instance again to simulate the container restart.
  // The client should become attached again.
  arc_bridge_service()->voice_interaction_framework()->SetInstance(
      framework_instance());
  FlushHighlighterControllerMojo();
  EXPECT_TRUE(highlighter_controller()->client_attached());

  // State update should reach the client normally.
  framework_instance()->ResetCounters();
  highlighter_controller()->CallHandleEnabledStateChange(true);
  FlushHighlighterControllerMojo();
  EXPECT_EQ(1u, framework_instance()->set_metalayer_visibility_count());
  EXPECT_TRUE(framework_instance()->metalayer_visible());
}

}  // namespace arc
