// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/system/cast/tray_cast.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/test/tray_cast_test_api.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/feature_switch.h"

namespace {

// Execute JavaScript within the context of the extension. Returns the result
// of the execution.
std::unique_ptr<base::Value> ExecuteJavaScript(
    const extensions::Extension* extension,
    const std::string& javascript) {
  extensions::ProcessManager* pm =
      extensions::ProcessManager::Get(ProfileManager::GetActiveUserProfile());
  content::RenderViewHost* host =
      pm->GetBackgroundHostForExtension(extension->id())->render_view_host();
  return content::ExecuteScriptAndGetValue(host->GetMainFrame(), javascript);
}

// Returns the current value within a global JavaScript variable.
std::unique_ptr<base::Value> GetJavaScriptVariable(
    const extensions::Extension* extension,
    const std::string& variable) {
  return ExecuteJavaScript(extension,
                           "(function() { return " + variable + "; })()");
}

bool GetJavaScriptStringVariable(const extensions::Extension* extension,
                                 const std::string& variable,
                                 std::string* result) {
  std::unique_ptr<base::Value> value =
      GetJavaScriptVariable(extension, variable);
  return value->GetAsString(result);
}

bool GetJavaScriptBooleanVariable(const extensions::Extension* extension,
                                  const std::string& variable,
                                  bool* result) {
  std::unique_ptr<base::Value> value =
      GetJavaScriptVariable(extension, variable);
  return value->GetAsBoolean(result);
}

// Ensures that all pending JavaScript execution callbacks are invoked.
void ExecutePendingJavaScript(const extensions::Extension* extension) {
  ExecuteJavaScript(extension, std::string());
}

// Invokes tray->StartCast(id) and returns true if launchDesktopMirroring was
// called with the same id. This automatically creates/destroys the detail view
// and notifies the tray that Chrome has begun casting.
bool StartCastWithVerification(const extensions::Extension* extension,
                               ash::TrayCast* tray,
                               const std::string& receiver_id) {
  ash::SystemTrayItem* system_tray_item = tray;
  ash::TrayCastTestAPI test_tray(tray);

  // We will simulate a button click in the detail view to begin the cast, so we
  // need to make a detail view available.
  std::unique_ptr<views::View> detailed_view =
      base::WrapUnique(system_tray_item->CreateDetailedView(
          ash::user::LoginStatus::LOGGED_IN_USER));

  // Clear out any old state and execute any pending JS calls created from the
  // CreateDetailedView call.
  ExecuteJavaScript(extension, "launchDesktopMirroringReceiverId = ''");

  // Tell the tray item that Chrome has started casting.
  test_tray.StartCast(receiver_id);
  test_tray.OnCastingSessionStartedOrStopped(true);

  system_tray_item->DestroyDetailedView();
  detailed_view.reset();

  std::string got_receiver_id;
  if (!GetJavaScriptStringVariable(
          extension, "launchDesktopMirroringReceiverId", &got_receiver_id))
    return false;
  return receiver_id == got_receiver_id;
}

// Invokes tray->StopCast() and returns true if stopMirroring('user-stop')
// was called in the extension.
bool StopCastWithVerification(const extensions::Extension* extension,
                              ash::TrayCastTestAPI* tray) {
  // Clear out any old state so we can be sure that we set the value here.
  ExecuteJavaScript(extension, "stopMirroringCalled = false");

  // Stop casting.
  tray->StopCast();
  tray->OnCastingSessionStartedOrStopped(false);

  bool result;
  if (!GetJavaScriptBooleanVariable(extension, "stopMirroringCalled", &result))
    return false;
  return result;
}

// Returns the cast tray. The tray initializer may have launched some
// JavaScript callbacks which have not finished executing.
ash::TrayCast* GetTrayCast(const extensions::Extension* extension) {
  ash::SystemTray* tray = ash::Shell::GetInstance()->GetPrimarySystemTray();

  // Make sure we actually popup the tray, otherwise the TrayCast instance will
  // not be created.
  tray->ShowDefaultView(ash::BubbleCreationType::BUBBLE_CREATE_NEW);

  // Creating the tray causes some JavaScript to be executed. Let's try to make
  // sure it is completed.
  if (extension)
    ExecutePendingJavaScript(extension);

  return tray->GetTrayCastForTesting();
}

class SystemTrayTrayCastChromeOSTest : public ExtensionBrowserTest {
 protected:
  SystemTrayTrayCastChromeOSTest() : ExtensionBrowserTest() {}
  ~SystemTrayTrayCastChromeOSTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // SystemTrayTrayCastChromeOSTest tests the behavior of the system tray
    // without Media Router, so we explicitly disable it.
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    override_media_router_.reset(new extensions::FeatureSwitch::ScopedOverride(
        extensions::FeatureSwitch::media_router(), false));
  }

  const extensions::Extension* LoadCastTestExtension() {
    return LoadExtension(test_data_dir_.AppendASCII("tray_cast"));
  }

 private:
  std::unique_ptr<extensions::FeatureSwitch::ScopedOverride>
      override_media_router_;
  DISALLOW_COPY_AND_ASSIGN(SystemTrayTrayCastChromeOSTest);
};

}  // namespace

namespace chromeos {

// A simple sanity check to make sure that the cast config delegate actually
// recognizes the cast extension.
IN_PROC_BROWSER_TEST_F(SystemTrayTrayCastChromeOSTest,
                       CastTraySanityCheckTestExtensionGetsRecognized) {
  ash::CastConfigDelegate* cast_config_delegate = ash::Shell::GetInstance()
                                                      ->system_tray_delegate()
                                                      ->GetCastConfigDelegate();

  EXPECT_FALSE(cast_config_delegate->HasCastExtension());
  const extensions::Extension* extension = LoadCastTestExtension();
  EXPECT_TRUE(cast_config_delegate->HasCastExtension());
  UninstallExtension(extension->id());
}

// Verifies that the cast tray is hidden when there is no extension installed.
IN_PROC_BROWSER_TEST_F(SystemTrayTrayCastChromeOSTest,
                       CastTrayIsHiddenWhenThereIsNoExtension) {
  ash::TrayCastTestAPI tray(GetTrayCast(nullptr));
  EXPECT_TRUE(tray.IsTrayInitialized());
  EXPECT_FALSE(tray.IsTrayVisible());
}

// Verifies that the cast tray is hidden if there are no available receivers,
// even if there is an extension installed.
IN_PROC_BROWSER_TEST_F(SystemTrayTrayCastChromeOSTest,
                       CastTrayIsHiddenWhenThereIsAnExtensionButNoReceivers) {
  const extensions::Extension* extension = LoadCastTestExtension();

  ash::TrayCastTestAPI tray(GetTrayCast(extension));
  EXPECT_TRUE(tray.IsTrayInitialized());
  EXPECT_FALSE(tray.IsTrayVisible());

  UninstallExtension(extension->id());
}

// Verifies that the cast tray is hidden if there are no available receivers,
// even if there is an extension installed that doesn't honor the private API.
IN_PROC_BROWSER_TEST_F(
    SystemTrayTrayCastChromeOSTest,
    CastTrayIsHiddenWhenThereIsAnOldExtensionButNoReceivers) {
  const extensions::Extension* extension = LoadCastTestExtension();

  // Remove the updateDevices listener. This means that the extension will
  // act like an extension before the private-API migration.
  ExecuteJavaScript(extension,
                    "chrome.cast.devicesPrivate.updateDevicesRequested."
                    "removeListener(sendDevices);");

  ash::TrayCastTestAPI tray(GetTrayCast(extension));
  EXPECT_TRUE(tray.IsTrayInitialized());
  EXPECT_FALSE(tray.IsTrayVisible());

  UninstallExtension(extension->id());
}

// Verifies that the cast tray is displayed when there are receivers available.
IN_PROC_BROWSER_TEST_F(SystemTrayTrayCastChromeOSTest,
                       CastTrayIsDisplayedWhenThereIsAnExtensionWithReceivers) {
  const extensions::Extension* extension = LoadCastTestExtension();
  ExecuteJavaScript(extension, "addReceiver('test_id', 'name')");

  ash::TrayCastTestAPI tray(GetTrayCast(extension));

  EXPECT_TRUE(tray.IsTrayInitialized());
  EXPECT_TRUE(tray.IsTrayVisible());

  UninstallExtension(extension->id());
}

// Verifies that we can cast to a specific receiver, stop casting, and then cast
// to another receiver when there is more than one receiver
IN_PROC_BROWSER_TEST_F(SystemTrayTrayCastChromeOSTest,
                       CastTrayMultipleReceivers) {
  const extensions::Extension* extension = LoadCastTestExtension();
  ExecuteJavaScript(extension, "addReceiver('test_id_1', 'name')");
  ExecuteJavaScript(extension, "addReceiver('not_used_0', 'name1')");
  ExecuteJavaScript(extension, "addReceiver('test_id_0', 'name')");
  ExecuteJavaScript(extension, "addReceiver('not_used_1', 'name2')");

  ash::TrayCast* tray = GetTrayCast(extension);
  ash::TrayCastTestAPI test_tray(tray);
  EXPECT_TRUE(StartCastWithVerification(extension, tray, "test_id_0"));

  EXPECT_TRUE(test_tray.IsTrayCastViewVisible());
  EXPECT_TRUE(StopCastWithVerification(extension, &test_tray));
  EXPECT_TRUE(test_tray.IsTraySelectViewVisible());

  EXPECT_TRUE(StartCastWithVerification(extension, tray, "test_id_1"));
  EXPECT_TRUE(test_tray.IsTrayCastViewVisible());
  EXPECT_TRUE(StopCastWithVerification(extension, &test_tray));
  EXPECT_TRUE(test_tray.IsTraySelectViewVisible());

  UninstallExtension(extension->id());
}

// Verifies the stop cast button invokes the JavaScript function
// "stopMirroring('user-stop')".
IN_PROC_BROWSER_TEST_F(SystemTrayTrayCastChromeOSTest,
                       CastTrayStopButtonStopsCast) {
  // Add a receiver that is casting.
  const extensions::Extension* extension = LoadCastTestExtension();
  ExecuteJavaScript(extension, "addReceiver('test_id', 'name', 'title', 1)");

  ash::TrayCastTestAPI test_tray(GetTrayCast(extension));
  test_tray.OnCastingSessionStartedOrStopped(true);
  ExecutePendingJavaScript(extension);
  EXPECT_TRUE(test_tray.IsTrayCastViewVisible());

  // Stop the cast using the UI.
  EXPECT_TRUE(StopCastWithVerification(extension, &test_tray));
  EXPECT_TRUE(test_tray.IsTraySelectViewVisible());

  UninstallExtension(extension->id());
}

// Verifies that the start cast button invokes "launchDesktopMirroring(...)".
IN_PROC_BROWSER_TEST_F(SystemTrayTrayCastChromeOSTest,
                       CastTrayStartButtonStartsCast) {
  const extensions::Extension* extension = LoadCastTestExtension();
  ExecuteJavaScript(extension, "addReceiver('test_id', 'name')");
  ash::TrayCast* tray = GetTrayCast(extension);
  EXPECT_TRUE(StartCastWithVerification(extension, tray, "test_id"));
  UninstallExtension(extension->id());
}

// Verifies that the CastConfigDelegate opens up a tab called "options.html".
IN_PROC_BROWSER_TEST_F(SystemTrayTrayCastChromeOSTest, CastTrayOpenOptions) {
  const extensions::Extension* extension = LoadCastTestExtension();

  ash::CastConfigDelegate* cast_config_delegate = ash::Shell::GetInstance()
                                                      ->system_tray_delegate()
                                                      ->GetCastConfigDelegate();
  cast_config_delegate->LaunchCastOptions();

  const GURL url =
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL();
  EXPECT_TRUE(base::StringPiece(url.GetContent()).ends_with("options.html"));

  UninstallExtension(extension->id());
}

}  // namespace chromeos
