// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_system.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/string_tokenizer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/extensions/extension_devtools_manager.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_navigation_observer.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/browser/extensions/extension_pref_value_map.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/lazy_background_task_queue.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

//
// ExtensionSystem
//

ExtensionSystem::ExtensionSystem() {
}

ExtensionSystem::~ExtensionSystem() {
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
      profile_->GetExtensionPrefValueMap()));
  extension_prefs_->Init(extensions_disabled);
}

void ExtensionSystemImpl::Shared::InitInfoMap() {
  // The ExtensionInfoMap needs to be created before the
  // ExtensionProcessManager.
  extension_info_map_ = new ExtensionInfoMap();
}

void ExtensionSystemImpl::Shared::Init(bool extensions_enabled) {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  lazy_background_task_queue_.reset(new LazyBackgroundTaskQueue(profile_));
  extension_event_router_.reset(new ExtensionEventRouter(profile_));
  extension_message_service_.reset(new ExtensionMessageService(
      lazy_background_task_queue_.get()));
  extension_navigation_observer_.reset(
      new ExtensionNavigationObserver(profile_));

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
      scoped_refptr<extensions::UnpackedInstaller> installer =
          extensions::UnpackedInstaller::Create(extension_service_.get());
      while (t.GetNext()) {
        installer->LoadFromCommandLine(FilePath(t.token()));
      }
    }
  }

  // Make the chrome://extension-icon/ resource available.
  profile_->GetChromeURLDataManager()->AddDataSource(
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

ExtensionService* ExtensionSystemImpl::Shared::extension_service() {
  return extension_service_.get();
}

UserScriptMaster* ExtensionSystemImpl::Shared::user_script_master() {
  return user_script_master_.get();
}

ExtensionInfoMap* ExtensionSystemImpl::Shared::info_map() {
  return extension_info_map_.get();
}

LazyBackgroundTaskQueue*
ExtensionSystemImpl::Shared::lazy_background_task_queue() {
  return lazy_background_task_queue_.get();
}

ExtensionMessageService* ExtensionSystemImpl::Shared::message_service() {
  return extension_message_service_.get();
}

ExtensionEventRouter* ExtensionSystemImpl::Shared::event_router() {
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
}

void ExtensionSystemImpl::Shutdown() {
  extension_process_manager_.reset();
}

void ExtensionSystemImpl::Init(bool extensions_enabled) {
  DCHECK(!profile_->IsOffTheRecord());
  if (user_script_master() || extension_service())
    return;  // Already initialized.

  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
      switches::kEnableExtensionTimelineApi)) {
    extension_devtools_manager_ = new ExtensionDevToolsManager(profile_);
  }

  shared_->InitInfoMap();

  extension_process_manager_.reset(ExtensionProcessManager::Create(profile_));

  shared_->Init(extensions_enabled);
}

ExtensionService* ExtensionSystemImpl::extension_service() {
  return shared_->extension_service();
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

ExtensionInfoMap* ExtensionSystemImpl::info_map() {
  return shared_->info_map();
}

LazyBackgroundTaskQueue* ExtensionSystemImpl::lazy_background_task_queue() {
  return shared_->lazy_background_task_queue();
}

ExtensionMessageService* ExtensionSystemImpl::message_service() {
  return shared_->message_service();
}

ExtensionEventRouter* ExtensionSystemImpl::event_router() {
  return shared_->event_router();
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
