// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_system.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_tokenizer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/extensions/api/alarms/alarm_manager.h"
#include "chrome/browser/extensions/api/declarative/rules_registry_service.h"
#include "chrome/browser/extensions/api/messaging/message_service.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/browser/extensions/extension_pref_value_map.h"
#include "chrome/browser/extensions/extension_pref_value_map_factory.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/extension_warning_badge_service.h"
#include "chrome/browser/extensions/extension_warning_set.h"
#include "chrome/browser/extensions/lazy_background_task_queue.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/extensions/navigation_observer.h"
#include "chrome/browser/extensions/shell_window_geometry_cache.h"
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
#include "chrome/common/extensions/features/feature.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

using content::BrowserThread;

namespace extensions {

//
// ExtensionSystem
//

ExtensionSystem::ExtensionSystem() {
  // Only set if it hasn't already been set (e.g. by a test).
  if (Feature::GetCurrentChannel() == Feature::GetDefaultChannel())
    Feature::SetCurrentChannel(chrome::VersionInfo::GetChannel());
}

ExtensionSystem::~ExtensionSystem() {
}

// static
ExtensionSystem* ExtensionSystem::Get(Profile* profile) {
  return ExtensionSystemFactory::GetForProfile(profile);
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
  bool extensions_disabled =
      profile_->GetPrefs()->GetBoolean(prefs::kDisableExtensions) ||
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableExtensions);
  extension_prefs_ = ExtensionPrefs::Create(
      profile_->GetPrefs(),
      profile_->GetPath().AppendASCII(ExtensionService::kInstallDirectoryName),
      ExtensionPrefValueMapFactory::GetForProfile(profile_),
      extensions_disabled);
  lazy_background_task_queue_.reset(new LazyBackgroundTaskQueue(profile_));
  event_router_.reset(new EventRouter(profile_, extension_prefs_.get()));

  // Two state stores. The latter, which contains declarative rules, must be
  // loaded immediately so that the rules are ready before we issue network
  // requests.
  state_store_.reset(new StateStore(
      profile_,
      profile_->GetPath().AppendASCII(ExtensionService::kStateStoreName),
      true));
  rules_store_.reset(new StateStore(
      profile_,
      profile_->GetPath().AppendASCII(ExtensionService::kRulesStoreName),
      false));

  shell_window_geometry_cache_.reset(new ShellWindowGeometryCache(
      profile_, extension_prefs_.get()));

  blacklist_.reset(new Blacklist(extension_prefs_.get()));

  standard_management_policy_provider_.reset(
      new StandardManagementPolicyProvider(extension_prefs_.get()));
}

void ExtensionSystemImpl::Shared::RegisterManagementPolicyProviders() {
  DCHECK(standard_management_policy_provider_.get());
  management_policy_->RegisterProvider(
      standard_management_policy_provider_.get());
}

void ExtensionSystemImpl::Shared::Init(bool extensions_enabled) {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  message_service_.reset(new MessageService(lazy_background_task_queue_.get()));
  navigation_observer_.reset(new NavigationObserver(profile_));

  bool allow_noisy_errors = !command_line->HasSwitch(switches::kNoErrorDialogs);
  ExtensionErrorReporter::Init(allow_noisy_errors);

  user_script_master_ = new UserScriptMaster(profile_);

  bool autoupdate_enabled = true;
#if defined(OS_CHROMEOS)
  if (!extensions_enabled)
    autoupdate_enabled = false;
  else
    autoupdate_enabled = !command_line->HasSwitch(switches::kGuestSession);
#endif
  extension_service_.reset(new ExtensionService(
      profile_,
      CommandLine::ForCurrentProcess(),
      profile_->GetPath().AppendASCII(ExtensionService::kInstallDirectoryName),
      extension_prefs_.get(),
      blacklist_.get(),
      autoupdate_enabled,
      extensions_enabled));

  // These services must be registered before the ExtensionService tries to
  // load any extensions.
  {
    management_policy_.reset(new ManagementPolicy);
    RegisterManagementPolicyProviders();
  }

  bool skip_session_extensions = false;
#if defined(OS_CHROMEOS)
  // Skip loading session extensions if we are not in a user session.
  skip_session_extensions = !chromeos::UserManager::Get()->IsUserLoggedIn();
#endif
  extension_service_->component_loader()->AddDefaultComponentExtensions(
      skip_session_extensions);
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
        UnpackedInstaller::Create(extension_service_.get())->
            LoadFromCommandLine(base::FilePath(t.token()), false);
      }
    }
  }

  // Make the chrome://extension-icon/ resource available.
  content::URLDataSource::Add(profile_, new ExtensionIconSource(profile_));

  // Initialize extension event routers. Note that on Chrome OS, this will
  // not succeed if the user has not logged in yet, in which case the
  // event routers are initialized in LoginUtilsImpl::CompleteLogin instead.
  // The InitEventRouters call used to be in BrowserMain, because when bookmark
  // import happened on first run, the bookmark bar was not being correctly
  // initialized (see issue 40144). Now that bookmarks aren't imported and
  // the event routers need to be initialized for every profile individually,
  // initialize them with the extension service.
  // If import is going to run in a separate process (the profile itself is on
  // the main process), wait for import to finish before initializing the
  // routers.
  CHECK(!ProfileManager::IsImportProcess(*command_line));
  if (g_browser_process->profile_manager()->will_import()) {
    extension_service_->InitEventRoutersAfterImport();
  } else {
    extension_service_->InitEventRouters();
  }

  extension_warning_service_.reset(new ExtensionWarningService(profile_));
  extension_warning_badge_service_.reset(
      new ExtensionWarningBadgeService(profile_));
  extension_warning_service_->AddObserver(
      extension_warning_badge_service_.get());
}

void ExtensionSystemImpl::Shared::Shutdown() {
  if (extension_warning_service_.get()) {
    extension_warning_service_->RemoveObserver(
        extension_warning_badge_service_.get());
  }
  if (extension_service_.get())
    extension_service_->Shutdown();
}

base::Clock* ExtensionSystemImpl::Shared::clock() {
  return &clock_;
}

StateStore* ExtensionSystemImpl::Shared::state_store() {
  return state_store_.get();
}

StateStore* ExtensionSystemImpl::Shared::rules_store() {
  return rules_store_.get();
}

ExtensionPrefs* ExtensionSystemImpl::Shared::extension_prefs() {
  return extension_prefs_.get();
}

ShellWindowGeometryCache* ExtensionSystemImpl::Shared::
    shell_window_geometry_cache() {
  return shell_window_geometry_cache_.get();
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

ExtensionInfoMap* ExtensionSystemImpl::Shared::info_map() {
  if (!extension_info_map_)
    extension_info_map_ = new ExtensionInfoMap();
  return extension_info_map_.get();
}

LazyBackgroundTaskQueue*
    ExtensionSystemImpl::Shared::lazy_background_task_queue() {
  return lazy_background_task_queue_.get();
}

MessageService* ExtensionSystemImpl::Shared::message_service() {
  return message_service_.get();
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

//
// ExtensionSystemImpl
//

ExtensionSystemImpl::ExtensionSystemImpl(Profile* profile)
    : profile_(profile) {
  shared_ = ExtensionSystemSharedFactory::GetForProfile(profile);

  if (profile->IsOffTheRecord()) {
    extension_process_manager_.reset(ExtensionProcessManager::Create(profile));
  } else {
    shared_->InitPrefs();
  }
}

ExtensionSystemImpl::~ExtensionSystemImpl() {
  if (rules_registry_service_.get())
    rules_registry_service_->Shutdown();
}

void ExtensionSystemImpl::Shutdown() {
  extension_process_manager_.reset();
}

void ExtensionSystemImpl::InitForRegularProfile(bool extensions_enabled) {
  DCHECK(!profile_->IsOffTheRecord());
  if (user_script_master() || extension_service())
    return;  // Already initialized.

  // The ExtensionInfoMap needs to be created before the
  // ExtensionProcessManager.
  shared_->info_map();

  extension_process_manager_.reset(ExtensionProcessManager::Create(profile_));
  alarm_manager_.reset(new AlarmManager(profile_, shared_->clock()));

  serial_connection_manager_.reset(new ApiResourceManager<SerialConnection>(
      BrowserThread::FILE));
  socket_manager_.reset(new ApiResourceManager<Socket>(BrowserThread::IO));
  usb_device_resource_manager_.reset(
      new ApiResourceManager<UsbDeviceResource>(BrowserThread::IO));

  rules_registry_service_.reset(new RulesRegistryService(profile_));
  rules_registry_service_->RegisterDefaultRulesRegistries();

  shared_->Init(extensions_enabled);
}

void ExtensionSystemImpl::InitForOTRProfile() {
  // Only initialize the RulesRegistryService of the OTR ExtensionSystem if the
  // regular ExtensionSystem has been initialized properly, as we depend on it.
  // Some ChromeOS browser tests don't initialize the regular ExtensionSystem
  // in login-tests.
  if (extension_service()) {
    rules_registry_service_.reset(new RulesRegistryService(profile_));
    rules_registry_service_->RegisterDefaultRulesRegistries();
  }
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

ExtensionProcessManager* ExtensionSystemImpl::process_manager() {
  return extension_process_manager_.get();
}

AlarmManager* ExtensionSystemImpl::alarm_manager() {
  return alarm_manager_.get();
}

StateStore* ExtensionSystemImpl::state_store() {
  return shared_->state_store();
}

StateStore* ExtensionSystemImpl::rules_store() {
  return shared_->rules_store();
}

ExtensionPrefs* ExtensionSystemImpl::extension_prefs() {
  return shared_->extension_prefs();
}

ShellWindowGeometryCache* ExtensionSystemImpl::shell_window_geometry_cache() {
  return shared_->shell_window_geometry_cache();
}

ExtensionInfoMap* ExtensionSystemImpl::info_map() {
  return shared_->info_map();
}

LazyBackgroundTaskQueue* ExtensionSystemImpl::lazy_background_task_queue() {
  return shared_->lazy_background_task_queue();
}

MessageService* ExtensionSystemImpl::message_service() {
  return shared_->message_service();
}

EventRouter* ExtensionSystemImpl::event_router() {
  return shared_->event_router();
}

RulesRegistryService* ExtensionSystemImpl::rules_registry_service() {
  return rules_registry_service_.get();
}

ApiResourceManager<SerialConnection>*
ExtensionSystemImpl::serial_connection_manager() {
  return serial_connection_manager_.get();
}

ApiResourceManager<Socket>* ExtensionSystemImpl::socket_manager() {
  return socket_manager_.get();
}

ApiResourceManager<UsbDeviceResource>*
ExtensionSystemImpl::usb_device_resource_manager() {
  return usb_device_resource_manager_.get();
}

ExtensionWarningService* ExtensionSystemImpl::warning_service() {
  return shared_->warning_service();
}

Blacklist* ExtensionSystemImpl::blacklist() {
  return shared_->blacklist();
}

void ExtensionSystemImpl::RegisterExtensionWithRequestContexts(
    const Extension* extension) {
  base::Time install_time;
  if (extension->location() != Manifest::COMPONENT) {
    install_time = extension_service()->extension_prefs()->
        GetInstallTime(extension->id());
  }
  bool incognito_enabled =
      extension_service()->IsIncognitoEnabled(extension->id());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ExtensionInfoMap::AddExtension, info_map(),
                 make_scoped_refptr(extension), install_time,
                 incognito_enabled));
}

void ExtensionSystemImpl::UnregisterExtensionWithRequestContexts(
    const std::string& extension_id,
    const extension_misc::UnloadedExtensionReason reason) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ExtensionInfoMap::RemoveExtension, info_map(),
                 extension_id, reason));
}

}  // namespace extensions
