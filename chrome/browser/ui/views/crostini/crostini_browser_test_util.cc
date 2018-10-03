// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_browser_test_util.h"

#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_image_loader_client.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/prefs/pref_service.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_change_notifier_factory.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

// ChromeBrowserMainExtraParts used to install a MockNetworkChangeNotifier.
class ChromeBrowserMainExtraPartsNetFactoryInstaller
    : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsNetFactoryInstaller() = default;
  ~ChromeBrowserMainExtraPartsNetFactoryInstaller() override {
    // |network_change_notifier_| needs to be destroyed before |net_installer_|.
    network_change_notifier_.reset();
  }

  net::test::MockNetworkChangeNotifier* network_change_notifier() {
    return network_change_notifier_.get();
  }

  // ChromeBrowserMainExtraParts:
  void PreEarlyInitialization() override {}
  void PostMainMessageLoopStart() override {
    ASSERT_TRUE(net::NetworkChangeNotifier::HasNetworkChangeNotifier());
    net_installer_ =
        std::make_unique<net::NetworkChangeNotifier::DisableForTest>();
    network_change_notifier_ =
        std::make_unique<net::test::MockNetworkChangeNotifier>();
    network_change_notifier_->SetConnectionType(
        net::NetworkChangeNotifier::CONNECTION_WIFI);
  }

 private:
  std::unique_ptr<net::test::MockNetworkChangeNotifier>
      network_change_notifier_;
  std::unique_ptr<net::NetworkChangeNotifier::DisableForTest> net_installer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsNetFactoryInstaller);
};

CrostiniDialogBrowserTest::CrostiniDialogBrowserTest()
    : dir_component_user_override_(component_updater::DIR_COMPONENT_USER) {
  auto image_loader_client =
      std::make_unique<chromeos::FakeImageLoaderClient>();
  image_loader_client_ = image_loader_client.get();
  chromeos::DBusThreadManager::GetSetterForTesting()->SetImageLoaderClient(
      std::move(image_loader_client));
}

void CrostiniDialogBrowserTest::CreatedBrowserMainParts(
    content::BrowserMainParts* browser_main_parts) {
  ChromeBrowserMainParts* chrome_browser_main_parts =
      static_cast<ChromeBrowserMainParts*>(browser_main_parts);
  extra_parts_ = new ChromeBrowserMainExtraPartsNetFactoryInstaller();
  chrome_browser_main_parts->AddParts(extra_parts_);
}

void CrostiniDialogBrowserTest::SetUp() {
  InitCrosTermina();
  crostini::SetCrostiniUIAllowedForTesting(true);
  DialogBrowserTest::SetUp();
}

void CrostiniDialogBrowserTest::SetUpOnMainThread() {
  browser()->profile()->GetPrefs()->SetBoolean(
      crostini::prefs::kCrostiniEnabled, true);
  auto* cros_component_manager =
      g_browser_process->platform_part()->cros_component_manager();
  cros_component_manager->RegisterCompatiblePath(
      imageloader::kTerminaComponentName, cros_termina_resources_.GetPath());
  image_loader_client_->RegisterComponent(
      imageloader::kTerminaComponentName, "1.1",
      cros_termina_resources_.GetPath().value(), base::DoNothing());
}

void CrostiniDialogBrowserTest::InitCrosTermina() {
  base::FilePath component_user_dir;
  ASSERT_TRUE(base::PathService::Get(component_updater::DIR_COMPONENT_USER,
                                     &component_user_dir));
  ASSERT_TRUE(cros_termina_resources_.Set(
      component_user_dir.Append("cros-components")
          .Append(imageloader::kTerminaComponentName)));

  image_loader_client_->SetMountPathForComponent(
      imageloader::kTerminaComponentName, cros_termina_resources_.GetPath());
}

void CrostiniDialogBrowserTest::SetConnectionType(
    net::NetworkChangeNotifier::ConnectionType connection_type) {
  extra_parts_->network_change_notifier()->SetConnectionType(connection_type);
}

void CrostiniDialogBrowserTest::UnregisterTermina() {
  auto* cros_component_manager =
      g_browser_process->platform_part()->cros_component_manager();
  cros_component_manager->UnregisterCompatiblePath(
      imageloader::kTerminaComponentName);
}
