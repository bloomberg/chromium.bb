// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_media_analytics_client.h"
#include "chromeos/dbus/media_analytics_client.h"
#include "chromeos/media_perception/media_perception.pb.h"
#include "extensions/browser/api/media_perception_private/media_perception_private_api.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/switches.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

class MediaPerceptionPrivateApiTest : public ShellApiTest {
 public:
  MediaPerceptionPrivateApiTest() {}
  ~MediaPerceptionPrivateApiTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ShellApiTest::SetUpCommandLine(command_line);
    // Whitelist of the extension ID of the test extension.
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        "epcifkihnkjgphfkloaaleeakhpmgdmn");
  }

  void SetUpInProcessBrowserTestFixture() override {
    std::unique_ptr<chromeos::DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    auto media_analytics_client =
        std::make_unique<chromeos::FakeMediaAnalyticsClient>();
    media_analytics_client_ = media_analytics_client.get();
    dbus_setter->SetMediaAnalyticsClient(std::move(media_analytics_client));
  }

  void SetUpOnMainThread() override {
    session_feature_type_ = extensions::ScopedCurrentFeatureSessionType(
        extensions::FeatureSessionType::KIOSK);
    ShellApiTest::SetUpOnMainThread();
  }

  // Ownership is passed on to chromeos::DbusThreadManager.
  chromeos::FakeMediaAnalyticsClient* media_analytics_client_;

 private:
  std::unique_ptr<base::AutoReset<extensions::FeatureSessionType>>
      session_feature_type_;

  DISALLOW_COPY_AND_ASSIGN(MediaPerceptionPrivateApiTest);
};

// Verify that we can set and get mediaPerception system state.
IN_PROC_BROWSER_TEST_F(MediaPerceptionPrivateApiTest, State) {
  ASSERT_TRUE(RunAppTest("media_perception_private/state")) << message_;
}

// Verify that we can request Diagnostics.
IN_PROC_BROWSER_TEST_F(MediaPerceptionPrivateApiTest, GetDiagnostics) {
  // Allows us to validate that the right data comes through the code path.
  mri::Diagnostics diagnostics;
  diagnostics.add_perception_sample()->mutable_frame_perception()->set_frame_id(
      1);
  media_analytics_client_->SetDiagnostics(diagnostics);

  ASSERT_TRUE(RunAppTest("media_perception_private/diagnostics")) << message_;
}

// Verify that we can listen for MediaPerceptionDetection signals and handle
// them.
IN_PROC_BROWSER_TEST_F(MediaPerceptionPrivateApiTest, MediaPerception) {
  extensions::ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser_context());

  ExtensionTestMessageListener handler_registered_listener(
      "mediaPerceptionListenerSet", false);
  ASSERT_TRUE(LoadApp("media_perception_private/media_perception")) << message_;
  ASSERT_TRUE(handler_registered_listener.WaitUntilSatisfied());

  mri::MediaPerception media_perception;
  media_perception.add_frame_perception()->set_frame_id(1);
  ASSERT_TRUE(
      media_analytics_client_->FireMediaPerceptionEvent(media_perception));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
