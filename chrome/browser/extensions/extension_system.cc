// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_system.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/string_tokenizer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/browser/extensions/extension_pref_value_map.h"
#include "chrome/browser/extensions/extension_pref_value_map_factory.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/extension_warning_badge_service.h"
#include "chrome/browser/extensions/extension_warning_set.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/extensions/navigation_observer.h"
#include "chrome/browser/extensions/standard_management_policy_provider.h"
#include "chrome/browser/extensions/state_store.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest.h"

#if defined(ENABLE_NOTIFICATIONS)
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "ui/message_center/notifier_settings.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/chromeos/extensions/device_local_account_management_policy_provider.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/login_state.h"
#endif

using content::BrowserThread;

namespace extensions {

//
// ExtensionSystem
//

ExtensionSystem::ExtensionSystem() {
  // Only set if it hasn't already been set (e.g. by a test).
  if (GetCurrentChannel() == GetDefaultChannel())
    SetCurrentChannel(chrome::VersionInfo::GetChannel());
}

ExtensionSystem::~ExtensionSystem() {
}

// static
ExtensionSystem* ExtensionSystem::Get(Profile* profile) {
  return ExtensionSystemFactory::GetForProfile(profile);
}

// static
ExtensionSystem* ExtensionSystem::GetForBrowserContext(
    content::BrowserContext* profile) {
  return ExtensionSystemFactory::GetForProfile(static_cast<Profile*>(profile));
}

//
// ExtensionSystemImpl::Shared
//

ExtensionSystemImpl::Shared::Shared(Profile* profile)
    : profile_(profile) {
}

ExtensionSystemImpl::Shared::~Shared() {
}

void ExtensionSystemImpl::Shared::InitPrefs() {
  lazy_background_task_queue_.reset(new LazyBackgroundTaskQueue(profile_));
  event_router_.reset(new EventRouter(profile_, ExtensionPrefs::Get(profile_)));
// TODO(yoz): Remove once crbug.com/159265 is fixed.
#if defined(ENABLE_EXTENSIONS)
  // Two state stores. The latter, which contains declarative rules, must be
  // loaded immediately so that the rules are ready before we issue network
  // requests.
  state_store_.reset(new StateStore(
      profile_,
      profile_->GetPath().AppendASCII(extensions::kStateStoreName),
      true));

  rules_store_.reset(new StateStore(
      profile_,
      profile_->GetPath().AppendASCII(extensions::kRulesStoreName),
      false));

  blacklist_.reset(new Blacklist(ExtensionPrefs::Get(profile_)));

  standard_management_policy_provider_.reset(
      new StandardManagementPolicyProvider(ExtensionPrefs::Get(profile_)));

#if defined (OS_CHROMEOS)
  const chromeos::User* user = chromeos::UserManager::Get()->GetActiveUser();
  policy::DeviceLocalAccount::Type device_local_account_type;
  if (user && policy::IsDeviceLocalAccountUser(user->email(),
                                               &device_local_account_type)) {
    device_local_account_management_policy_provider_.reset(
        new chromeos::DeviceLocalAccountManagementPolicyProvider(
            device_local_account_type));
  }
#endif  // defined (OS_CHROMEOS)

#endif  // defined(ENABLE_EXTENSIONS)
}

void ExtensionSystemImpl::Shared::RegisterManagementPolicyProviders() {
// TODO(yoz): Remove once crbug.com/159265 is fixed.
#if defined(ENABLE_EXTENSIONS)
  DCHECK(standard_management_policy_provider_.get());
  management_policy_->RegisterProvider(
      standard_management_policy_provider_.get());

#if defined (OS_CHROMEOS)
  if (device_local_account_management_policy_provider_) {
    management_policy_->RegisterProvider(
        device_local_account_management_policy_provider_.get());
  }
#endif  // defined (OS_CHROMEOS)

#endif  // defined(ENABLE_EXTENSIONS)
}

void ExtensionSystemImpl::Shared::Init(bool extensions_enabled) {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  navigation_observer_.reset(new NavigationObserver(profile_));

  bool allow_noisy_errors = !command_line->HasSwitch(switches::kNoErrorDialogs);
  ExtensionErrorReporter::Init(allow_noisy_errors);

  user_script_master_ = new UserScriptMaster(profile_);

  bool autoupdate_enabled = true;
#if defined(OS_CHROMEOS)
  if (!extensions_enabled)
    autoupdate_enabled = false;
  else
    autoupdate_enabled =
        !command_line->HasSwitch(chromeos::switches::kGuestSession);
#endif
  extension_service_.reset(new ExtensionService(
      profile_,
      CommandLine::ForCurrentProcess(),
      profile_->GetPath().AppendASCII(extensions::kInstallDirectoryName),
      ExtensionPrefs::Get(profile_),
      blacklist_.get(),
      autoupdate_enabled,
      extensions_enabled,
      &ready_));

  // These services must be registered before the ExtensionService tries to
  // load any extensions.
  {
    management_policy_.reset(new ManagementPolicy);
    RegisterManagementPolicyProviders();
  }

  bool skip_session_extensions = false;
#if defined(OS_CHROMEOS)
  // Skip loading session extensions if we are not in a user session.
  skip_session_extensions = !chromeos::LoginState::Get()->IsUserLoggedIn();
  if (!chrome::IsRunningInForcedAppMode()) {
    extension_service_->component_loader()->AddDefaultComponentExtensions(
        skip_session_extensions);
  }
#else
  extension_service_->component_loader()->AddDefaultComponentExtensions(
      skip_session_extensions);
#endif
  if (command_line->HasSwitch(switches::kLoadComponentExtension)) {
    CommandLine::StringType path_list = command_line->GetSwitchValueNative(
        switches::kLoadComponentExtension);
    base::StringTokenizerT<CommandLine::StringType,
        CommandLine::StringType::const_iterator> t(path_list,
                                                   FILE_PATH_LITERAL(","));
    while (t.GetNext()) {
      // Load the component extension manifest synchronously.
      // Blocking the UI thread is acceptable here since
      // this flag designated for developers.
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      extension_service_->component_loader()->AddOrReplace(
          base::FilePath(t.token()));
    }
  }
  extension_service_->Init();

  if (extensions_enabled) {
    // Load any extensions specified with --load-extension.
    // TODO(yoz): Seems like this should move into ExtensionService::Init.
    // But maybe it's no longer important.
    if (command_line->HasSwitch(switches::kLoadExtension)) {
      CommandLine::StringType path_list = command_line->GetSwitchValueNative(
          switches::kLoadExtension);
      base::StringTokenizerT<CommandLine::StringType,
          CommandLine::StringType::const_iterator> t(path_list,
                                                     FILE_PATH_LITERAL(","));
      while (t.GetNext()) {
        std::string extension_id;
        UnpackedInstaller::Create(extension_service_.get())->
            LoadFromCommandLine(base::FilePath(t.token()), &extension_id);
      }
    }
  }

  // Make the chrome://extension-icon/ resource available.
  content::URLDataSource::Add(profile_, new ExtensionIconSource(profile_));

  extension_warning_service_.reset(new ExtensionWarningService(profile_));
  extension_warning_badge_service_.reset(
      new ExtensionWarningBadgeService(profile_));
  extension_warning_service_->AddObserver(
      extension_warning_badge_service_.get());
  error_console_.reset(new ErrorConsole(profile_, extension_service_.get()));
}

void ExtensionSystemImpl::Shared::Shutdown() {
  if (extension_warning_service_) {
    extension_warning_service_->RemoveObserver(
        extension_warning_badge_service_.get());
  }
  if (extension_service_)
    extension_service_->Shutdown();
}

StateStore* ExtensionSystemImpl::Shared::state_store() {
  return state_store_.get();
}

StateStore* ExtensionSystemImpl::Shared::rules_store() {
  return rules_store_.get();
}

ExtensionService* ExtensionSystemImpl::Shared::extension_service() {
  return extension_service_.get();
}

ManagementPolicy* ExtensionSystemImpl::Shared::management_policy() {
  return management_policy_.get();
}

UserScriptMaster* ExtensionSystemImpl::Shared::user_script_master() {
  return user_script_master_.get();
}

InfoMap* ExtensionSystemImpl::Shared::info_map() {
  if (!extension_info_map_.get())
    extension_info_map_ = new InfoMap();
  return extension_info_map_.get();
}

LazyBackgroundTaskQueue*
    ExtensionSystemImpl::Shared::lazy_background_task_queue() {
  return lazy_background_task_queue_.get();
}

EventRouter* ExtensionSystemImpl::Shared::event_router() {
  return event_router_.get();
}

ExtensionWarningService* ExtensionSystemImpl::Shared::warning_service() {
  return extension_warning_service_.get();
}

Blacklist* ExtensionSystemImpl::Shared::blacklist() {
  return blacklist_.get();
}

ErrorConsole* ExtensionSystemImpl::Shared::error_console() {
  return error_console_.get();
}

//
// ExtensionSystemImpl
//

ExtensionSystemImpl::ExtensionSystemImpl(Profile* profile)
    : profile_(profile) {
  shared_ = ExtensionSystemSharedFactory::GetForProfile(profile);

  if (profile->IsOffTheRecord()) {
    process_manager_.reset(ProcessManager::Create(profile));
  } else {
    shared_->InitPrefs();
  }
}

ExtensionSystemImpl::~ExtensionSystemImpl() {
}

void ExtensionSystemImpl::Shutdown() {
  process_manager_.reset();
}

void ExtensionSystemImpl::InitForRegularProfile(
    bool extensions_enabled,
    bool defer_background_creation) {
  DCHECK(!profile_->IsOffTheRecord());
  if (user_script_master() || extension_service())
    return;  // Already initialized.

  // The InfoMap needs to be created before the ProcessManager.
  shared_->info_map();

  process_manager_.reset(ProcessManager::Create(profile_));

  process_manager_->DeferBackgroundHostCreation(
      defer_background_creation);

  shared_->Init(extensions_enabled);
}

ExtensionService* ExtensionSystemImpl::extension_service() {
  return shared_->extension_service();
}

ManagementPolicy* ExtensionSystemImpl::management_policy() {
  return shared_->management_policy();
}

UserScriptMaster* ExtensionSystemImpl::user_script_master() {
  return shared_->user_script_master();
}

ProcessManager* ExtensionSystemImpl::process_manager() {
  return process_manager_.get();
}

StateStore* ExtensionSystemImpl::state_store() {
  return shared_->state_store();
}

StateStore* ExtensionSystemImpl::rules_store() {
  return shared_->rules_store();
}

InfoMap* ExtensionSystemImpl::info_map() { return shared_->info_map(); }

LazyBackgroundTaskQueue* ExtensionSystemImpl::lazy_background_task_queue() {
  return shared_->lazy_background_task_queue();
}

EventRouter* ExtensionSystemImpl::event_router() {
  return shared_->event_router();
}

ExtensionWarningService* ExtensionSystemImpl::warning_service() {
  return shared_->warning_service();
}

Blacklist* ExtensionSystemImpl::blacklist() {
  return shared_->blacklist();
}

const OneShotEvent& ExtensionSystemImpl::ready() const {
  return shared_->ready();
}

ErrorConsole* ExtensionSystemImpl::error_console() {
  return shared_->error_console();
}

void ExtensionSystemImpl::RegisterExtensionWithRequestContexts(
    const Extension* extension) {
  base::Time install_time;
  if (extension->location() != Manifest::COMPONENT) {
    install_time = ExtensionPrefs::Get(profile_)->
        GetInstallTime(extension->id());
  }
  bool incognito_enabled =
      extension_util::IsIncognitoEnabled(extension->id(), extension_service());

  bool notifications_disabled = false;
#if defined(ENABLE_NOTIFICATIONS)
  message_center::NotifierId notifier_id(
      message_center::NotifierId::APPLICATION,
      extension->id());

  DesktopNotificationService* notification_service =
      DesktopNotificationServiceFactory::GetForProfile(profile_);
  notifications_disabled =
      !notification_service->IsNotifierEnabled(notifier_id);
#endif

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&InfoMap::AddExtension, info_map(),
                 make_scoped_refptr(extension), install_time,
                 incognito_enabled, notifications_disabled));
}

void ExtensionSystemImpl::UnregisterExtensionWithRequestContexts(
    const std::string& extension_id,
    const UnloadedExtensionInfo::Reason reason) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&InfoMap::RemoveExtension, info_map(), extension_id, reason));
}

}  // namespace extensions
