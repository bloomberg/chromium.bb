// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_platform_part_chromeos.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/ash_config.h"
#include "chrome/browser/chromeos/chrome_service_name.h"
#include "chrome/browser/chromeos/login/session/chrome_session_manager.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/chromeos/net/delay_network_call.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system/automatic_reboot_manager.h"
#include "chrome/browser/chromeos/system/device_disabling_manager.h"
#include "chrome/browser/chromeos/system/device_disabling_manager_default_delegate.h"
#include "chrome/browser/chromeos/system/system_clock.h"
#include "chrome/browser/chromeos/system/timezone_resolver_manager.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/browser/lifetime/keep_alive_types.h"
#include "chrome/browser/lifetime/scoped_keep_alive.h"
#include "chrome/browser/prefs/active_profile_pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/geolocation/simple_geolocation_provider.h"
#include "chromeos/timezone/timezone_resolver.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/service.h"

#if defined(USE_OZONE)
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/ui/public/cpp/input_devices/input_device_controller.h"
#include "services/ui/public/cpp/input_devices/input_device_controller_client.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#endif

namespace {
// Packaged service implementation used to expose miscellaneous application
// control features. This is a singleton service which runs on the main thread
// and never stops.
class ChromeServiceChromeOS : public service_manager::Service,
                              public mash::mojom::Launchable {
 public:
  ChromeServiceChromeOS() {
#if defined(USE_OZONE)
    input_device_controller_.AddInterface(&interfaces_);
#endif
    interfaces_.AddInterface<mash::mojom::Launchable>(
        base::Bind(&ChromeServiceChromeOS::Create, base::Unretained(this)));
  }
  ~ChromeServiceChromeOS() override {}

  static std::unique_ptr<service_manager::Service> CreateService() {
    return base::MakeUnique<ChromeServiceChromeOS>();
  }

 private:
  void CreateNewWindowImpl(bool is_incognito) {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    chrome::NewEmptyWindow(is_incognito ? profile->GetOffTheRecordProfile()
                                        : profile);
  }

  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& remote_info,
                       const std::string& name,
                       mojo::ScopedMessagePipeHandle handle) override {
    interfaces_.BindInterface(name, std::move(handle), remote_info);
  }

  // mash::mojom::Launchable:
  void Launch(uint32_t what, mash::mojom::LaunchMode how) override {
    bool is_incognito;
    switch (what) {
      case mash::mojom::kWindow:
        is_incognito = false;
        break;
      case mash::mojom::kIncognitoWindow:
        is_incognito = true;
        break;
      default:
        NOTREACHED();
    }

    bool reuse = how != mash::mojom::LaunchMode::MAKE_NEW;
    if (reuse) {
      Profile* profile = ProfileManager::GetActiveUserProfile();
      Browser* browser = chrome::FindTabbedBrowser(
          is_incognito ? profile->GetOffTheRecordProfile() : profile, false);
      if (browser) {
        browser->window()->Show();
        return;
      }
    }

    CreateNewWindowImpl(is_incognito);
  }

  void Create(mash::mojom::LaunchableRequest request,
              const service_manager::BindSourceInfo& source_info) {
    bindings_.AddBinding(this, std::move(request));
  }

  service_manager::BinderRegistryWithArgs<
      const service_manager::BindSourceInfo&>
      interfaces_;
  mojo::BindingSet<mash::mojom::Launchable> bindings_;
#if defined(USE_OZONE)
  ui::InputDeviceController input_device_controller_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeServiceChromeOS);
};

std::unique_ptr<service_manager::Service> CreateEmbeddedUIService(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    base::WeakPtr<ui::ImageCursorsSet> image_cursors_set_weak_ptr) {
  ui::Service::InProcessConfig config;
  config.resource_runner = task_runner;
  config.image_cursors_set_weak_ptr = image_cursors_set_weak_ptr;
  return base::MakeUnique<ui::Service>(&config);
}

}  // namespace

BrowserProcessPlatformPart::BrowserProcessPlatformPart()
    : created_profile_helper_(false) {}

BrowserProcessPlatformPart::~BrowserProcessPlatformPart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BrowserProcessPlatformPart::InitializeAutomaticRebootManager() {
  DCHECK(!automatic_reboot_manager_);

  automatic_reboot_manager_.reset(new chromeos::system::AutomaticRebootManager(
      std::unique_ptr<base::TickClock>(new base::DefaultTickClock)));
}

void BrowserProcessPlatformPart::ShutdownAutomaticRebootManager() {
  automatic_reboot_manager_.reset();
}

void BrowserProcessPlatformPart::InitializeChromeUserManager() {
  DCHECK(!chrome_user_manager_);
  chrome_user_manager_ =
      chromeos::ChromeUserManagerImpl::CreateChromeUserManager();
  chrome_user_manager_->Initialize();
}

void BrowserProcessPlatformPart::DestroyChromeUserManager() {
  chrome_user_manager_->Destroy();
  chrome_user_manager_.reset();
}

void BrowserProcessPlatformPart::InitializeDeviceDisablingManager() {
  DCHECK(!device_disabling_manager_);

  device_disabling_manager_delegate_.reset(
      new chromeos::system::DeviceDisablingManagerDefaultDelegate);
  device_disabling_manager_.reset(new chromeos::system::DeviceDisablingManager(
      device_disabling_manager_delegate_.get(),
      chromeos::CrosSettings::Get(),
      user_manager::UserManager::Get()));
  device_disabling_manager_->Init();
}

void BrowserProcessPlatformPart::ShutdownDeviceDisablingManager() {
  device_disabling_manager_.reset();
  device_disabling_manager_delegate_.reset();
}

void BrowserProcessPlatformPart::InitializeSessionManager() {
  DCHECK(!session_manager_);
  session_manager_ = base::MakeUnique<chromeos::ChromeSessionManager>();
}

void BrowserProcessPlatformPart::ShutdownSessionManager() {
  session_manager_.reset();
}

void BrowserProcessPlatformPart::RegisterKeepAlive() {
  DCHECK(!keep_alive_);
  keep_alive_.reset(
      new ScopedKeepAlive(KeepAliveOrigin::BROWSER_PROCESS_CHROMEOS,
                          KeepAliveRestartOption::DISABLED));
}

void BrowserProcessPlatformPart::UnregisterKeepAlive() {
  keep_alive_.reset();
}

chromeos::ProfileHelper* BrowserProcessPlatformPart::profile_helper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!created_profile_helper_)
    CreateProfileHelper();
  return profile_helper_.get();
}

policy::BrowserPolicyConnectorChromeOS*
BrowserProcessPlatformPart::browser_policy_connector_chromeos() {
  return static_cast<policy::BrowserPolicyConnectorChromeOS*>(
      g_browser_process->browser_policy_connector());
}

chromeos::system::TimeZoneResolverManager*
BrowserProcessPlatformPart::GetTimezoneResolverManager() {
  if (!timezone_resolver_manager_.get()) {
    timezone_resolver_manager_.reset(
        new chromeos::system::TimeZoneResolverManager());
  }
  return timezone_resolver_manager_.get();
}

chromeos::TimeZoneResolver* BrowserProcessPlatformPart::GetTimezoneResolver() {
  if (!timezone_resolver_.get()) {
    timezone_resolver_.reset(new chromeos::TimeZoneResolver(
        GetTimezoneResolverManager(),
        g_browser_process->system_request_context(),
        chromeos::SimpleGeolocationProvider::DefaultGeolocationProviderURL(),
        base::Bind(&chromeos::system::ApplyTimeZone),
        base::Bind(&chromeos::DelayNetworkCall,
                   base::TimeDelta::FromMilliseconds(
                       chromeos::kDefaultNetworkRetryDelayMS)),
        g_browser_process->local_state()));
  }
  return timezone_resolver_.get();
}

void BrowserProcessPlatformPart::StartTearDown() {
  // interactive_ui_tests check for memory leaks before this object is
  // destroyed.  So we need to destroy |timezone_resolver_| here.
  timezone_resolver_.reset();
  profile_helper_.reset();
}

std::unique_ptr<policy::BrowserPolicyConnector>
BrowserProcessPlatformPart::CreateBrowserPolicyConnector() {
  return std::unique_ptr<policy::BrowserPolicyConnector>(
      new policy::BrowserPolicyConnectorChromeOS());
}

void BrowserProcessPlatformPart::RegisterInProcessServices(
    content::ContentBrowserClient::StaticServiceMap* services) {
  {
    service_manager::EmbeddedServiceInfo info;
    info.factory = base::Bind(&ChromeServiceChromeOS::CreateService);
    info.task_runner = base::ThreadTaskRunnerHandle::Get();
    services->insert(std::make_pair(chromeos::kChromeServiceName, info));
  }

  {
    service_manager::EmbeddedServiceInfo info;
    info.factory = base::Bind([] {
      return std::unique_ptr<service_manager::Service>(
          base::MakeUnique<ActiveProfilePrefService>());
    });
    info.task_runner = base::ThreadTaskRunnerHandle::Get();
    services->insert(std::make_pair(prefs::mojom::kForwarderServiceName, info));
  }

  if (!ash_util::IsRunningInMash()) {
    service_manager::EmbeddedServiceInfo info;
    info.factory = base::Bind(&ash_util::CreateEmbeddedAshService,
                              base::ThreadTaskRunnerHandle::Get());
    info.task_runner = base::ThreadTaskRunnerHandle::Get();
    services->insert(std::make_pair(ash::mojom::kServiceName, info));
  }

  if (chromeos::GetAshConfig() == ash::Config::MUS) {
    service_manager::EmbeddedServiceInfo info;
    info.factory = base::Bind(&CreateEmbeddedUIService,
                              base::ThreadTaskRunnerHandle::Get(),
                              image_cursors_set_.GetWeakPtr());
    info.use_own_thread = true;
    info.message_loop_type = base::MessageLoop::TYPE_UI;
    info.thread_priority = base::ThreadPriority::DISPLAY;
    services->insert(std::make_pair(ui::mojom::kServiceName, info));
  }
}

chromeos::system::SystemClock* BrowserProcessPlatformPart::GetSystemClock() {
  if (!system_clock_.get())
    system_clock_.reset(new chromeos::system::SystemClock());
  return system_clock_.get();
}

void BrowserProcessPlatformPart::DestroySystemClock() {
  system_clock_.reset();
}

void BrowserProcessPlatformPart::AddCompatibleCrOSComponent(
    const std::string& name) {
  compatible_cros_components_.insert(name);
}

bool BrowserProcessPlatformPart::IsCompatibleCrOSComponent(
    const std::string& name) {
  return compatible_cros_components_.count(name) > 0;
}

#if defined(USE_OZONE)
ui::InputDeviceControllerClient*
BrowserProcessPlatformPart::GetInputDeviceControllerClient() {
  if (!input_device_controller_client_) {
    const std::string service_name =
        chromeos::GetAshConfig() == ash::Config::CLASSIC
            ? chromeos::kChromeServiceName
            : ui::mojom::kServiceName;
    input_device_controller_client_ =
        base::MakeUnique<ui::InputDeviceControllerClient>(
            content::ServiceManagerConnection::GetForProcess()->GetConnector(),
            service_name);
  }
  return input_device_controller_client_.get();
}
#endif

void BrowserProcessPlatformPart::CreateProfileHelper() {
  DCHECK(!created_profile_helper_ && !profile_helper_);
  created_profile_helper_ = true;
  profile_helper_.reset(new chromeos::ProfileHelper());
}
