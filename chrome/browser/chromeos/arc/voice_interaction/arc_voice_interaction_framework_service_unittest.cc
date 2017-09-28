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
#include "ui/gfx/geometry/rect.h"

namespace arc {

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
    framework_service_ = std::make_unique<ArcVoiceInteractionFrameworkService>(
        profile_.get(), arc_bridge_service_.get());
    framework_instance_ =
        std::make_unique<FakeVoiceInteractionFrameworkInstance>();
    arc_bridge_service_->voice_interaction_framework()->SetInstance(
        framework_instance_.get());

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

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  std::unique_ptr<ArcBridgeService> arc_bridge_service_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
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

}  // namespace arc
