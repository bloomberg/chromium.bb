// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_TEST_HELPER_H_
#define ASH_TEST_ASH_TEST_HELPER_H_

#include <stdint.h>

#include <memory>
#include <utility>

#include "ash/assistant/test/test_assistant_service.h"
#include "ash/public/cpp/test/test_ambient_client.h"
#include "ash/public/cpp/test/test_image_downloader.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/session/test_pref_service_provider.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell_delegate.h"
#include "ash/system/message_center/test_notifier_settings_controller.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/test/scoped_command_line.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "ui/aura/test/aura_test_helper.h"

class PrefService;

namespace aura {
class Window;
}

namespace display {
class Display;
}

namespace ui {
class ContextFactory;
}

namespace views {
class TestViewsDelegate;
}

namespace ash {

class AppListTestHelper;
class TestKeyboardControllerObserver;
class TestNewWindowDelegate;

// A helper class that does common initialization required for Ash. Creates a
// root window and an ash::Shell instance with a test delegate.
class AshTestHelper : public aura::test::AuraTestHelper {
 public:
  enum ConfigType {
    // The configuration for shell executable.
    kShell,
    // The configuration for unit tests.
    kUnitTest,
    // The configuration for perf tests. Unlike kUnitTest, this
    // does not disable animations.
    kPerfTest,
  };

  struct InitParams {
    InitParams();
    InitParams(InitParams&&);
    InitParams& operator=(InitParams&&) = default;
    ~InitParams();

    // True if the user should log in.
    bool start_session = true;
    // If this is not set, a TestShellDelegate will be used automatically.
    std::unique_ptr<ShellDelegate> delegate;
    PrefService* local_state = nullptr;
  };

  // Instantiates/destroys an AshTestHelper. This can happen in a
  // single-threaded phase without a backing task environment or ViewsDelegate,
  // and must not create those lest the caller wish to do so.
  explicit AshTestHelper(ConfigType config_type = kUnitTest,
                         ui::ContextFactory* context_factory = nullptr);
  ~AshTestHelper() override;

  // Calls through to SetUp() below, see comments there.
  void SetUp() override;

  // Tears down everything but the Screen instance, which some tests access
  // after this point.  This will be called automatically on destruction if it
  // is not called manually earlier.
  void TearDown() override;

  aura::Window* GetContext() override;
  aura::WindowTreeHost* GetHost() override;
  aura::TestScreen* GetTestScreen() override;
  aura::client::FocusClient* GetFocusClient() override;
  aura::client::CaptureClient* GetCaptureClient() override;

  // Creates the ash::Shell and performs associated initialization according
  // to |init_params|.  When this function returns it guarantees a task
  // environment and ViewsDelegate will exist, the shell will be started, and a
  // window will be showing.
  void SetUp(InitParams init_params);

  display::Display GetSecondaryDisplay() const;

  TestSessionControllerClient* test_session_controller_client() {
    return session_controller_client_.get();
  }
  void set_test_session_controller_client(
      std::unique_ptr<TestSessionControllerClient> session_controller_client) {
    session_controller_client_ = std::move(session_controller_client);
  }
  TestNotifierSettingsController* notifier_settings_controller() {
    return notifier_settings_controller_.get();
  }
  TestSystemTrayClient* system_tray_client() {
    return system_tray_client_.get();
  }
  TestPrefServiceProvider* prefs_provider() { return prefs_provider_.get(); }

  AppListTestHelper* app_list_test_helper() {
    return app_list_test_helper_.get();
  }

  TestKeyboardControllerObserver* test_keyboard_controller_observer() {
    return test_keyboard_controller_observer_.get();
  }

  TestAssistantService* test_assistant_service() {
    return assistant_service_.get();
  }

 private:
  // Scoping objects to manage init/teardown of services.
  class BluezDBusManagerInitializer;
  class PowerPolicyControllerInitializer;

  ConfigType config_type_;
  std::unique_ptr<base::test::ScopedCommandLine> command_line_ =
      std::make_unique<base::test::ScopedCommandLine>();
  std::unique_ptr<chromeos::system::ScopedFakeStatisticsProvider>
      statistics_provider_ =
          std::make_unique<chromeos::system::ScopedFakeStatisticsProvider>();
  std::unique_ptr<TestPrefServiceProvider> prefs_provider_ =
      std::make_unique<TestPrefServiceProvider>();
  std::unique_ptr<TestNotifierSettingsController>
      notifier_settings_controller_ =
          std::make_unique<TestNotifierSettingsController>();
  std::unique_ptr<TestAssistantService> assistant_service_ =
      std::make_unique<TestAssistantService>();
  std::unique_ptr<TestSystemTrayClient> system_tray_client_ =
      std::make_unique<TestSystemTrayClient>();
  std::unique_ptr<AppListTestHelper> app_list_test_helper_;
  std::unique_ptr<BluezDBusManagerInitializer> bluez_dbus_manager_initializer_;
  std::unique_ptr<PowerPolicyControllerInitializer>
      power_policy_controller_initializer_;
  std::unique_ptr<TestNewWindowDelegate> new_window_delegate_;
  std::unique_ptr<views::TestViewsDelegate> test_views_delegate_;
  std::unique_ptr<TestSessionControllerClient> session_controller_client_;
  std::unique_ptr<TestKeyboardControllerObserver>
      test_keyboard_controller_observer_;

  DISALLOW_COPY_AND_ASSIGN(AshTestHelper);
};

}  // namespace ash

#endif  // ASH_TEST_ASH_TEST_HELPER_H_
