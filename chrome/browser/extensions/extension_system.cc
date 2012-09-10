// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_system.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/string_tokenizer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/extensions/api/alarms/alarm_manager.h"
#include "chrome/browser/extensions/api/declarative/rules_registry_service.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_devtools_manager.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/browser/extensions/extension_pref_value_map.h"
#include "chrome/browser/extensions/extension_pref_value_map_factory.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/lazy_background_task_queue.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/extensions/api/messaging/message_service.h"
#include "chrome/browser/extensions/navigation_observer.h"
#include "chrome/browser/extensions/shell_window_geometry_cache.h"
#include "chrome/browser/extensions/state_store.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/features/feature.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

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
  extension_prefs_.reset(new ExtensionPrefs(
      profile_->GetPrefs(),
      profile_->GetPath().AppendASCII(ExtensionService::kInstallDirectoryName),
      ExtensionPrefValueMapFactory::GetForProfile(profile_)));
  extension_prefs_->Init(extensions_disabled);

  state_store_.reset(new StateStore(
      profile_,
      profile_->GetPath().AppendASCII(ExtensionService::kStateStoreName)));

  shell_window_geometry_cache_.reset(new ShellWindowGeometryCache(
    profile_, state_store_.get()));

}

void ExtensionSystemImpl::Shared::RegisterManagementPolicyProviders() {
  DCHECK(extension_prefs_.get());
  management_policy_->RegisterProvider(extension_prefs_.get());
}

void ExtensionSystemImpl::Shared::Init(bool extensions_enabled) {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  lazy_background_task_queue_.reset(new LazyBackgroundTaskQueue(profile_));
  message_service_.reset(new MessageService(lazy_background_task_queue_.get()));
  extension_event_router_.reset(new EventRouter(profile_));
  navigation_observer_.reset(new NavigationObserver(profile_));

  ExtensionErrorReporter::Init(true);  // allow noisy errors.

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
      autoupdate_enabled,
      extensions_enabled));

  // These services must be registered before the ExtensionService tries to
  // load any extensions.
  {
    management_policy_.reset(new ManagementPolicy);
    RegisterManagementPolicyProviders();
  }

  extension_service_->component_loader()->AddDefaultComponentExtensions();
  if (command_line->HasSwitch(switches::kLoadComponentExtension)) {
    CommandLine::StringType path_list = command_line->GetSwitchValueNative(
        switches::kLoadComponentExtension);
    StringTokenizerT<CommandLine::StringType,
        CommandLine::StringType::const_iterator> t(path_list,
                                                   FILE_PATH_LITERAL(","));
    while (t.GetNext()) {
      // Load the component extension manifest synchronously.
      // Blocking the UI thread is acceptable here since
      // this flag designated for developers.
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      extension_service_->component_loader()->AddOrReplace(
          FilePath(t.token()));
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
      StringTokenizerT<CommandLine::StringType,
          CommandLine::StringType::const_iterator> t(path_list,
                                                     FILE_PATH_LITERAL(","));
      while (t.GetNext()) {
        UnpackedInstaller::Create(extension_service_.get())->
            LoadFromCommandLine(FilePath(t.token()));
      }
    }
  }

  // Make the chrome://extension-icon/ resource available.
  ChromeURLDataManager::AddDataSource(profile_,
      new ExtensionIconSource(profile_));

  // Initialize extension event routers. Note that on Chrome OS, this will
  // not succeed if the user has not logged in yet, in which case the
  // event routers are initialized in LoginUtilsImpl::CompleteLogin instead.
  // The InitEventRouters call used to be in BrowserMain, because when bookmark
  // import happened on first run, the bookmark bar was not being correctly
  // initialized (see issue 40144). Now that bookmarks aren't imported and
  // the event routers need to be initialized for every profile individually,
  // initialize them with the extension service.
  // If this profile is being created as part of the import process, never
  // initialize the event routers. If import is going to run in a separate
  // process (the profile itself is on the main process), wait for import to
  // finish before initializing the routers.
  if (!command_line->HasSwitch(switches::kImport) &&
      !command_line->HasSwitch(switches::kImportFromFile)) {
    if (g_browser_process->profile_manager()->will_import()) {
      extension_service_->InitEventRoutersAfterImport();
    } else {
      extension_service_->InitEventRouters();
    }
  }
}

void ExtensionSystemImpl::Shared::Shutdown() {
  if (extension_service_.get())
    extension_service_->Shutdown();
}

StateStore* ExtensionSystemImpl::Shared::state_store() {
  return state_store_.get();
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
  return extension_event_router_.get();
}

//
// ExtensionSystemImpl
//

ExtensionSystemImpl::ExtensionSystemImpl(Profile* profile)
    : profile_(profile),
      extension_devtools_manager_(NULL) {
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

  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
      switches::kEnableExtensionTimelineApi)) {
    extension_devtools_manager_ = new ExtensionDevToolsManager(profile_);
  }

  // The ExtensionInfoMap needs to be created before the
  // ExtensionProcessManager.
  shared_->info_map();

  extension_process_manager_.reset(ExtensionProcessManager::Create(profile_));
  alarm_manager_.reset(new AlarmManager(profile_, &base::Time::Now));

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

ExtensionDevToolsManager* ExtensionSystemImpl::devtools_manager() {
  // TODO(mpcomplete): in incognito, figure out whether we should
  // return the original profile's version.
  return extension_devtools_manager_.get();
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

void ExtensionSystemImpl::RegisterExtensionWithRequestContexts(
    const Extension* extension) {
  base::Time install_time;
  if (extension->location() != Extension::COMPONENT) {
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
