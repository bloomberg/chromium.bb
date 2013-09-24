// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_service.h"

#include <algorithm>
#include <iterator>
#include <set>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/app_runtime/app_runtime_api.h"
#include "chrome/browser/extensions/api/declarative/rules_registry_service.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/api/runtime/runtime_api.h"
#include "chrome/browser/extensions/api/storage/settings_frontend.h"
#include "chrome/browser/extensions/app_sync_data.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/data_deleter.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_disabled_ui.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_error_ui.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_sorting.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/extension_sync_data.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/external_install_ui.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/external_provider_interface.h"
#include "chrome/browser/extensions/installed_loader.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/update_observer.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/ntp/thumbnail_source.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/extensions/background_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "chrome/common/extensions/incognito_handler.h"
#include "chrome/common/extensions/manifest_handlers/app_isolation_info.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/shared_module_info.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/extensions/permissions/permissions_data.h"
#include "chrome/common/extensions/sync_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/startup_metric_utils/startup_metric_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "grit/generated_resources.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error_factory.h"
#include "ui/webui/web_ui_util.h"
#include "url/gurl.h"
#include "webkit/browser/database/database_tracker.h"
#include "webkit/browser/database/database_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/install_limiter.h"
#include "webkit/browser/fileapi/file_system_backend.h"
#include "webkit/browser/fileapi/file_system_context.h"
#endif

using content::BrowserContext;
using content::BrowserThread;
using content::DevToolsAgentHost;
using extensions::CrxInstaller;
using extensions::Extension;
using extensions::ExtensionIdSet;
using extensions::ExtensionInfo;
using extensions::FeatureSwitch;
using extensions::Manifest;
using extensions::PermissionMessage;
using extensions::PermissionMessages;
using extensions::PermissionSet;
using extensions::SharedModuleInfo;
using extensions::UnloadedExtensionInfo;

namespace errors = extensions::manifest_errors;

namespace {

// Histogram values for logging events related to externally installed
// extensions.
enum ExternalExtensionEvent {
  EXTERNAL_EXTENSION_INSTALLED = 0,
  EXTERNAL_EXTENSION_IGNORED,
  EXTERNAL_EXTENSION_REENABLED,
  EXTERNAL_EXTENSION_UNINSTALLED,
  EXTERNAL_EXTENSION_BUCKET_BOUNDARY,
};

// Prompt the user this many times before considering an extension acknowledged.
static const int kMaxExtensionAcknowledgePromptCount = 3;

// Wait this many seconds after an extensions becomes idle before updating it.
static const int kUpdateIdleDelay = 5;

// Wait this many seconds before trying to garbage collect extensions again.
static const int kGarbageCollectRetryDelay = 30;

// Wait this many seconds after startup to see if there are any extensions
// which can be garbage collected.
static const int kGarbageCollectStartupDelay = 30;

static bool IsSharedModule(const Extension* extension) {
  return SharedModuleInfo::IsSharedModule(extension);
}

static bool IsCWSSharedModule(const Extension* extension) {
  return extension->from_webstore() && IsSharedModule(extension);
}

class SharedModuleProvider : public extensions::ManagementPolicy::Provider {
 public:
  SharedModuleProvider() {}
  virtual ~SharedModuleProvider() {}

  virtual std::string GetDebugPolicyProviderName() const OVERRIDE {
    return "SharedModuleProvider";
  }

  virtual bool UserMayModifySettings(const Extension* extension,
                                     string16* error) const OVERRIDE {
    return !IsCWSSharedModule(extension);
  }

  virtual bool MustRemainEnabled(const Extension* extension,
                                 string16* error) const OVERRIDE {
    return IsCWSSharedModule(extension);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedModuleProvider);
};


}  // namespace

ExtensionService::ExtensionRuntimeData::ExtensionRuntimeData()
    : background_page_ready(false),
      being_upgraded(false),
      has_used_webrequest(false) {
}

ExtensionService::ExtensionRuntimeData::~ExtensionRuntimeData() {
}

// ExtensionService.

const char ExtensionService::kLocalAppSettingsDirectoryName[] =
    "Local App Settings";
const char ExtensionService::kLocalExtensionSettingsDirectoryName[] =
    "Local Extension Settings";
const char ExtensionService::kSyncAppSettingsDirectoryName[] =
    "Sync App Settings";
const char ExtensionService::kSyncExtensionSettingsDirectoryName[] =
    "Sync Extension Settings";
const char ExtensionService::kManagedSettingsDirectoryName[] =
    "Managed Extension Settings";
const char ExtensionService::kStateStoreName[] = "Extension State";
const char ExtensionService::kRulesStoreName[] = "Extension Rules";

void ExtensionService::CheckExternalUninstall(const std::string& id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Check if the providers know about this extension.
  extensions::ProviderCollection::const_iterator i;
  for (i = external_extension_providers_.begin();
       i != external_extension_providers_.end(); ++i) {
    DCHECK(i->get()->IsReady());
    if (i->get()->HasExtension(id))
      return;  // Yup, known extension, don't uninstall.
  }

  // We get the list of external extensions to check from preferences.
  // It is possible that an extension has preferences but is not loaded.
  // For example, an extension that requires experimental permissions
  // will not be loaded if the experimental command line flag is not used.
  // In this case, do not uninstall.
  if (!GetInstalledExtension(id)) {
    // We can't call UninstallExtension with an unloaded/invalid
    // extension ID.
    LOG(WARNING) << "Attempted uninstallation of unloaded/invalid extension "
                 << "with id: " << id;
    return;
  }
  UninstallExtension(id, true, NULL);
}

void ExtensionService::SetFileTaskRunnerForTesting(
    base::SequencedTaskRunner* task_runner) {
  file_task_runner_ = task_runner;
}

void ExtensionService::ClearProvidersForTesting() {
  external_extension_providers_.clear();
}

void ExtensionService::AddProviderForTesting(
    extensions::ExternalProviderInterface* test_provider) {
  CHECK(test_provider);
  external_extension_providers_.push_back(
      linked_ptr<extensions::ExternalProviderInterface>(test_provider));
}

bool ExtensionService::OnExternalExtensionUpdateUrlFound(
    const std::string& id,
    const GURL& update_url,
    Manifest::Location location) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(Extension::IdIsValid(id));

  const Extension* extension = GetExtensionById(id, true);
  if (extension) {
    // Already installed. Skip this install if the current location has
    // higher priority than |location|.
    Manifest::Location current = extension->location();
    if (current == Manifest::GetHigherPriorityLocation(current, location))
      return false;
    // Otherwise, overwrite the current installation.
  }

  // Add |id| to the set of pending extensions.  If it can not be added,
  // then there is already a pending record from a higher-priority install
  // source.  In this case, signal that this extension will not be
  // installed by returning false.
  if (!pending_extension_manager()->AddFromExternalUpdateUrl(
          id, update_url, location)) {
    return false;
  }

  update_once_all_providers_are_ready_ = true;
  return true;
}

const Extension* ExtensionService::GetInstalledApp(const GURL& url) const {
  const Extension* extension = extensions_.GetExtensionOrAppByURL(url);
  return (extension && extension->is_app()) ? extension : NULL;
}

bool ExtensionService::IsInstalledApp(const GURL& url) const {
  return !!GetInstalledApp(url);
}

const Extension* ExtensionService::GetIsolatedAppForRenderer(
    int renderer_child_id) const {
  std::set<std::string> extension_ids =
      process_map_.GetExtensionsInProcess(renderer_child_id);
  // All apps in one process share the same partition.
  // It is only possible for the app to have isolated storage
  // if there is only 1 app in the process.
  if (extension_ids.size() != 1)
    return NULL;

  const extensions::Extension* extension =
      extensions_.GetByID(*(extension_ids.begin()));
  // We still need to check if the extension has isolated storage,
  // because it's common for there to be one extension in a process
  // without isolated storage.
  if (extension &&
      extensions::AppIsolationInfo::HasIsolatedStorage(extension))
    return extension;

  return NULL;
}

// static
// This function is used to implement the command-line switch
// --uninstall-extension, and to uninstall an extension via sync.  The LOG
// statements within this function are used to inform the user if the uninstall
// cannot be done.
bool ExtensionService::UninstallExtensionHelper(
    ExtensionService* extensions_service,
    const std::string& extension_id) {
  // We can't call UninstallExtension with an invalid extension ID.
  if (!extensions_service->GetInstalledExtension(extension_id)) {
    LOG(WARNING) << "Attempted uninstallation of non-existent extension with "
                 << "id: " << extension_id;
    return false;
  }

  // The following call to UninstallExtension will not allow an uninstall of a
  // policy-controlled extension.
  string16 error;
  if (!extensions_service->UninstallExtension(extension_id, false, &error)) {
    LOG(WARNING) << "Cannot uninstall extension with id " << extension_id
                 << ": " << error;
    return false;
  }

  return true;
}

ExtensionService::ExtensionService(Profile* profile,
                                   const CommandLine* command_line,
                                   const base::FilePath& install_directory,
                                   extensions::ExtensionPrefs* extension_prefs,
                                   extensions::Blacklist* blacklist,
                                   bool autoupdate_enabled,
                                   bool extensions_enabled,
                                   extensions::OneShotEvent* ready)
    : extensions::Blacklist::Observer(blacklist),
      profile_(profile),
      system_(extensions::ExtensionSystem::Get(profile)),
      extension_prefs_(extension_prefs),
      blacklist_(blacklist),
      settings_frontend_(extensions::SettingsFrontend::Create(profile)),
      pending_extension_manager_(*this),
      install_directory_(install_directory),
      extensions_enabled_(extensions_enabled),
      show_extensions_prompts_(true),
      install_updates_when_idle_(true),
      ready_(ready),
      toolbar_model_(this),
      menu_manager_(profile),
      update_once_all_providers_are_ready_(false),
      browser_terminating_(false),
      installs_delayed_for_gc_(false),
      is_first_run_(false),
      app_sync_bundle_(this),
      extension_sync_bundle_(this) {
#if defined(OS_CHROMEOS)
  disable_garbage_collection_ = false;
#endif
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Figure out if extension installation should be enabled.
  if (command_line->HasSwitch(switches::kDisableExtensions) ||
      profile->GetPrefs()->GetBoolean(prefs::kDisableExtensions)) {
    extensions_enabled_ = false;
  }

  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_UPGRADE_RECOMMENDED,
                 content::NotificationService::AllBrowserContextsAndSources());
  pref_change_registrar_.Init(profile->GetPrefs());
  base::Closure callback =
      base::Bind(&ExtensionService::OnExtensionInstallPrefChanged,
                 base::Unretained(this));
  pref_change_registrar_.Add(prefs::kExtensionInstallAllowList, callback);
  pref_change_registrar_.Add(prefs::kExtensionInstallDenyList, callback);
  pref_change_registrar_.Add(prefs::kExtensionAllowedTypes, callback);

  // Set up the ExtensionUpdater
  if (autoupdate_enabled) {
    int update_frequency = kDefaultUpdateFrequencySeconds;
    if (command_line->HasSwitch(switches::kExtensionsUpdateFrequency)) {
      base::StringToInt(command_line->GetSwitchValueASCII(
          switches::kExtensionsUpdateFrequency),
          &update_frequency);
    }
    updater_.reset(new extensions::ExtensionUpdater(this,
                                                    extension_prefs,
                                                    profile->GetPrefs(),
                                                    profile,
                                                    update_frequency));
  }

  component_loader_.reset(
      new extensions::ComponentLoader(this,
                                      profile->GetPrefs(),
                                      g_browser_process->local_state()));

  if (extensions_enabled_) {
    extensions::ExternalProviderImpl::CreateExternalProviders(
        this, profile_, &external_extension_providers_);
  }

  // Set this as the ExtensionService for extension sorting to ensure it
  // cause syncs if required.
  extension_prefs_->extension_sorting()->SetExtensionService(this);

  is_first_run_ = !extension_prefs_->SetAlertSystemFirstRun();

#if defined(ENABLE_EXTENSIONS)
  extension_action_storage_manager_.reset(
      new extensions::ExtensionActionStorageManager(profile_));
#endif

  shared_module_policy_provider_.reset(new SharedModuleProvider);

  // How long is the path to the Extensions directory?
  UMA_HISTOGRAM_CUSTOM_COUNTS("Extensions.ExtensionRootPathLength",
                              install_directory_.value().length(), 0, 500, 100);
}

const ExtensionSet* ExtensionService::extensions() const {
  return &extensions_;
}

const ExtensionSet* ExtensionService::disabled_extensions() const {
  return &disabled_extensions_;
}

const ExtensionSet* ExtensionService::terminated_extensions() const {
  return &terminated_extensions_;
}

const ExtensionSet* ExtensionService::blacklisted_extensions() const {
  return &blacklisted_extensions_;
}

const ExtensionSet* ExtensionService::delayed_installs() const {
  return &delayed_installs_;
}

scoped_ptr<const ExtensionSet>
    ExtensionService::GenerateInstalledExtensionsSet() const {
  scoped_ptr<ExtensionSet> installed_extensions(new ExtensionSet());
  installed_extensions->InsertAll(extensions_);
  installed_extensions->InsertAll(disabled_extensions_);
  installed_extensions->InsertAll(terminated_extensions_);
  installed_extensions->InsertAll(blacklisted_extensions_);
  return installed_extensions.PassAs<const ExtensionSet>();
}

extensions::PendingExtensionManager*
    ExtensionService::pending_extension_manager() {
  return &pending_extension_manager_;
}

ExtensionService::~ExtensionService() {
  // No need to unload extensions here because they are profile-scoped, and the
  // profile is in the process of being deleted.

  extensions::ProviderCollection::const_iterator i;
  for (i = external_extension_providers_.begin();
       i != external_extension_providers_.end(); ++i) {
    extensions::ExternalProviderInterface* provider = i->get();
    provider->ServiceShutdown();
  }
}

void ExtensionService::SetSyncStartFlare(
    const syncer::SyncableService::StartSyncFlare& flare) {
  flare_ = flare;
}

void ExtensionService::Shutdown() {
  system_->management_policy()->UnregisterProvider(
      shared_module_policy_provider_.get());
}

const Extension* ExtensionService::GetExtensionById(
    const std::string& id, bool include_disabled) const {
  int include_mask = INCLUDE_ENABLED;
  if (include_disabled) {
    // Include blacklisted extensions here because there are hundreds of
    // callers of this function, and many might assume that this includes those
    // that have been disabled due to blacklisting.
    include_mask |= INCLUDE_DISABLED | INCLUDE_BLACKLISTED;
  }
  return GetExtensionById(id, include_mask);
}

GURL ExtensionService::GetSiteForExtensionId(const std::string& extension_id) {
  return content::SiteInstance::GetSiteForURL(
      profile_,
      Extension::GetBaseURLFromExtensionId(extension_id));
}

const Extension* ExtensionService::GetExtensionById(
    const std::string& id, int include_mask) const {
  std::string lowercase_id = StringToLowerASCII(id);
  if (include_mask & INCLUDE_ENABLED) {
    const Extension* extension = extensions_.GetByID(lowercase_id);
    if (extension)
      return extension;
  }
  if (include_mask & INCLUDE_DISABLED) {
    const Extension* extension = disabled_extensions_.GetByID(lowercase_id);
    if (extension)
      return extension;
  }
  if (include_mask & INCLUDE_TERMINATED) {
    const Extension* extension = terminated_extensions_.GetByID(lowercase_id);
    if (extension)
      return extension;
  }
  if (include_mask & INCLUDE_BLACKLISTED) {
    const Extension* extension = blacklisted_extensions_.GetByID(lowercase_id);
    if (extension)
      return extension;
  }
  return NULL;
}

void ExtensionService::Init() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK(!is_ready());  // Can't redo init.
  DCHECK_EQ(extensions_.size(), 0u);

  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kInstallFromWebstore) ||
      cmd_line->HasSwitch(switches::kLimitedInstallFromWebstore)) {
    // The sole purpose of this launch is to install a new extension from CWS
    // and immediately terminate: loading already installed extensions is
    // unnecessary and may interfere with the inline install dialog (e.g. if an
    // extension listens to onStartup and opens a window).
    SetReadyAndNotifyListeners();
  } else {
    // LoadAllExtensions() calls OnLoadedInstalledExtensions().
    component_loader_->LoadAll();
    extensions::InstalledLoader(this).LoadAllExtensions();

    // Attempt to re-enable extensions whose only disable reason is reloading.
    std::vector<std::string> extensions_to_enable;
    for (ExtensionSet::const_iterator iter = disabled_extensions_.begin();
        iter != disabled_extensions_.end(); ++iter) {
      const Extension* e = iter->get();
      if (extension_prefs_->GetDisableReasons(e->id()) ==
          Extension::DISABLE_RELOAD) {
        extensions_to_enable.push_back(e->id());
      }
    }
    for (std::vector<std::string>::iterator it = extensions_to_enable.begin();
         it != extensions_to_enable.end(); ++it) {
      EnableExtension(*it);
    }

    // Finish install (if possible) of extensions that were still delayed while
    // the browser was shut down.
    scoped_ptr<extensions::ExtensionPrefs::ExtensionsInfo> delayed_info(
        extension_prefs_->GetAllDelayedInstallInfo());
    for (size_t i = 0; i < delayed_info->size(); ++i) {
      ExtensionInfo* info = delayed_info->at(i).get();
      scoped_refptr<const Extension> extension(NULL);
      if (info->extension_manifest) {
        std::string error;
        extension = Extension::Create(
            info->extension_path,
            info->extension_location,
            *info->extension_manifest,
            extension_prefs_->GetDelayedInstallCreationFlags(
                info->extension_id),
            info->extension_id,
            &error);
        if (extension.get())
          delayed_installs_.Insert(extension);
      }
    }
    MaybeFinishDelayedInstallations();

    scoped_ptr<extensions::ExtensionPrefs::ExtensionsInfo> delayed_info2(
        extension_prefs_->GetAllDelayedInstallInfo());
    UMA_HISTOGRAM_COUNTS_100("Extensions.UpdateOnLoad",
                             delayed_info2->size() - delayed_info->size());

    SetReadyAndNotifyListeners();

    // TODO(erikkay) this should probably be deferred to a future point
    // rather than running immediately at startup.
    CheckForExternalUpdates();

    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ExtensionService::GarbageCollectExtensions, AsWeakPtr()),
        base::TimeDelta::FromSeconds(kGarbageCollectStartupDelay));

    if (extension_prefs_->NeedsStorageGarbageCollection()) {
      GarbageCollectIsolatedStorage();
      extension_prefs_->SetNeedsStorageGarbageCollection(false);
    }
    system_->management_policy()->RegisterProvider(
        shared_module_policy_provider_.get());
  }
}

bool ExtensionService::UpdateExtension(const std::string& id,
                                       const base::FilePath& extension_path,
                                       const GURL& download_url,
                                       CrxInstaller** out_crx_installer) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (browser_terminating_) {
    LOG(WARNING) << "Skipping UpdateExtension due to browser shutdown";
    // Leak the temp file at extension_path. We don't want to add to the disk
    // I/O burden at shutdown, we can't rely on the I/O completing anyway, and
    // the file is in the OS temp directory which should be cleaned up for us.
    return false;
  }

  const extensions::PendingExtensionInfo* pending_extension_info =
      pending_extension_manager()->GetById(id);

  const Extension* extension = GetInstalledExtension(id);
  if (!pending_extension_info && !extension) {
    LOG(WARNING) << "Will not update extension " << id
                 << " because it is not installed or pending";
    // Delete extension_path since we're not creating a CrxInstaller
    // that would do it for us.
    if (!GetFileTaskRunner()->PostTask(
            FROM_HERE,
            base::Bind(
                &extension_file_util::DeleteFile, extension_path, false)))
      NOTREACHED();

    return false;
  }

  // We want a silent install only for non-pending extensions and
  // pending extensions that have install_silently set.
  scoped_ptr<ExtensionInstallPrompt> client;
  if (pending_extension_info && !pending_extension_info->install_silently())
    client.reset(ExtensionInstallUI::CreateInstallPromptWithProfile(profile_));

  scoped_refptr<CrxInstaller> installer(
      CrxInstaller::Create(this, client.Pass()));
  installer->set_expected_id(id);
  if (pending_extension_info) {
    installer->set_install_source(pending_extension_info->install_source());
    if (pending_extension_info->install_silently())
      installer->set_allow_silent_install(true);
  } else if (extension) {
    installer->set_install_source(extension->location());
  }
  // If the extension was installed from or has migrated to the webstore, or
  // its auto-update URL is from the webstore, treat it as a webstore install.
  // Note that we ignore some older extensions with blank auto-update URLs
  // because we are mostly concerned with restrictions on NaCl extensions,
  // which are newer.
  int creation_flags = Extension::NO_FLAGS;
  if ((extension && extension->from_webstore()) ||
      (extension && extensions::ManifestURL::UpdatesFromGallery(extension)) ||
      (!extension && extension_urls::IsWebstoreUpdateUrl(
           pending_extension_info->update_url()))) {
    creation_flags |= Extension::FROM_WEBSTORE;
  }

  // Bookmark apps being updated is kind of a contradiction, but that's because
  // we mark the default apps as bookmark apps, and they're hosted in the web
  // store, thus they can get updated. See http://crbug.com/101605 for more
  // details.
  if (extension && extension->from_bookmark())
    creation_flags |= Extension::FROM_BOOKMARK;

  if (extension && extension->was_installed_by_default())
    creation_flags |= Extension::WAS_INSTALLED_BY_DEFAULT;

  installer->set_creation_flags(creation_flags);

  installer->set_delete_source(true);
  installer->set_download_url(download_url);
  installer->set_install_cause(extension_misc::INSTALL_CAUSE_UPDATE);
  installer->InstallCrx(extension_path);

  if (out_crx_installer)
    *out_crx_installer = installer.get();

  return true;
}

void ExtensionService::ReloadExtension(const std::string extension_id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If the extension is already reloading, don't reload again.
  if (extension_prefs_->GetDisableReasons(extension_id) &
      Extension::DISABLE_RELOAD) {
    return;
  }

  base::FilePath path;
  const Extension* current_extension = GetExtensionById(extension_id, false);

  // Disable the extension if it's loaded. It might not be loaded if it crashed.
  if (current_extension) {
    // If the extension has an inspector open for its background page, detach
    // the inspector and hang onto a cookie for it, so that we can reattach
    // later.
    // TODO(yoz): this is not incognito-safe!
    ExtensionProcessManager* manager = system_->process_manager();
    extensions::ExtensionHost* host =
        manager->GetBackgroundHostForExtension(extension_id);
    if (host && DevToolsAgentHost::HasFor(host->render_view_host())) {
      // Look for an open inspector for the background page.
      scoped_refptr<DevToolsAgentHost> agent_host =
          DevToolsAgentHost::GetOrCreateFor(host->render_view_host());
      agent_host->DisconnectRenderViewHost();
      orphaned_dev_tools_[extension_id] = agent_host;
    }

    path = current_extension->path();
    // BeingUpgraded is set back to false when the extension is added.
    SetBeingUpgraded(current_extension, true);
    DisableExtension(extension_id, Extension::DISABLE_RELOAD);
    reloading_extensions_.insert(extension_id);
  } else {
    path = unloaded_extension_paths_[extension_id];
  }

  if (delayed_installs_.Contains(extension_id)) {
    FinishDelayedInstallation(extension_id);
    return;
  }

  // If we're reloading a component extension, use the component extension
  // loader's reloader.
  if (component_loader_->Exists(extension_id)) {
    SetBeingReloaded(extension_id, true);
    component_loader_->Reload(extension_id);
    SetBeingReloaded(extension_id, false);
    return;
  }

  // Check the installed extensions to see if what we're reloading was already
  // installed.
  SetBeingReloaded(extension_id, true);
  scoped_ptr<ExtensionInfo> installed_extension(
      extension_prefs_->GetInstalledExtensionInfo(extension_id));
  if (installed_extension.get() &&
      installed_extension->extension_manifest.get()) {
    extensions::InstalledLoader(this).Load(*installed_extension, false);
  } else {
    // Otherwise, the extension is unpacked (location LOAD).
    // We should always be able to remember the extension's path. If it's not in
    // the map, someone failed to update |unloaded_extension_paths_|.
    CHECK(!path.empty());
    extensions::UnpackedInstaller::Create(this)->Load(path);
  }
  // When reloading is done, mark this extension as done reloading.
  SetBeingReloaded(extension_id, false);
}

bool ExtensionService::UninstallExtension(
    std::string extension_id,
    bool external_uninstall,
    string16* error) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_refptr<const Extension> extension(GetInstalledExtension(extension_id));

  // Callers should not send us nonexistent extensions.
  CHECK(extension.get());

  // Policy change which triggers an uninstall will always set
  // |external_uninstall| to true so this is the only way to uninstall
  // managed extensions.
  if (!external_uninstall &&
      !system_->management_policy()->UserMayModifySettings(
        extension.get(), error)) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_UNINSTALL_NOT_ALLOWED,
        content::Source<Profile>(profile_),
        content::Details<const Extension>(extension.get()));
    return false;
  }

  // Extract the data we need for sync now, but don't actually sync until we've
  // completed the uninstallation.
  // TODO(tim): If we get here and IsSyncing is false, this will cause
  // "back from the dead" style bugs, because sync will add-back the extension
  // that was uninstalled here when MergeDataAndStartSyncing is called.
  // See crbug.com/256795.
  syncer::SyncChange sync_change;
  if (extensions::sync_helper::IsSyncableApp(extension.get())) {
    if (app_sync_bundle_.IsSyncing())
      sync_change = app_sync_bundle_.CreateSyncChangeToDelete(extension.get());
    else if (is_ready() && !flare_.is_null())
      flare_.Run(syncer::APPS);  // Tell sync to start ASAP.
  } else if (extensions::sync_helper::IsSyncableExtension(extension.get())) {
    if (extension_sync_bundle_.IsSyncing())
      sync_change = extension_sync_bundle_.CreateSyncChangeToDelete(extension);
    else if (is_ready() && !flare_.is_null())
      flare_.Run(syncer::EXTENSIONS);  // Tell sync to start ASAP.
  }

  if (IsUnacknowledgedExternalExtension(extension.get())) {
    UMA_HISTOGRAM_ENUMERATION("Extensions.ExternalExtensionEvent",
                              EXTERNAL_EXTENSION_UNINSTALLED,
                              EXTERNAL_EXTENSION_BUCKET_BOUNDARY);
    if (extensions::ManifestURL::UpdatesFromGallery(extension.get())) {
      UMA_HISTOGRAM_ENUMERATION("Extensions.ExternalExtensionEventWebstore",
                                EXTERNAL_EXTENSION_UNINSTALLED,
                                EXTERNAL_EXTENSION_BUCKET_BOUNDARY);
    } else {
      UMA_HISTOGRAM_ENUMERATION("Extensions.ExternalExtensionEventNonWebstore",
                                EXTERNAL_EXTENSION_UNINSTALLED,
                                EXTERNAL_EXTENSION_BUCKET_BOUNDARY);
    }
  }
  UMA_HISTOGRAM_ENUMERATION("Extensions.UninstallType",
                            extension->GetType(), 100);
  RecordPermissionMessagesHistogram(extension.get(),
                                    "Extensions.Permissions_Uninstall");

  // Unload before doing more cleanup to ensure that nothing is hanging on to
  // any of these resources.
  UnloadExtension(extension_id, extension_misc::UNLOAD_REASON_UNINSTALL);

  // Tell the backend to start deleting installed extensions on the file thread.
  if (!Manifest::IsUnpackedLocation(extension->location())) {
    if (!GetFileTaskRunner()->PostTask(
            FROM_HERE,
            base::Bind(
                &extension_file_util::UninstallExtension,
                install_directory_,
                extension_id)))
      NOTREACHED();
  }

  GURL launch_web_url_origin(
      extensions::AppLaunchInfo::GetLaunchWebURL(extension.get()).GetOrigin());
  bool is_storage_isolated =
      extensions::AppIsolationInfo::HasIsolatedStorage(extension.get());

  if (is_storage_isolated) {
    BrowserContext::AsyncObliterateStoragePartition(
        profile_,
        GetSiteForExtensionId(extension_id),
        base::Bind(&ExtensionService::OnNeedsToGarbageCollectIsolatedStorage,
                   AsWeakPtr()));
  } else {
    if (extension->is_hosted_app() &&
        !profile_->GetExtensionSpecialStoragePolicy()->
            IsStorageProtected(launch_web_url_origin)) {
      extensions::DataDeleter::StartDeleting(
          profile_, extension_id, launch_web_url_origin);
    }
    extensions::DataDeleter::StartDeleting(profile_, extension_id,
                                           extension->url());
  }

  UntrackTerminatedExtension(extension_id);

  // Notify interested parties that we've uninstalled this extension.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
      content::Source<Profile>(profile_),
      content::Details<const Extension>(extension.get()));

  if (app_sync_bundle_.HasExtensionId(extension_id) &&
      sync_change.sync_data().GetDataType() == syncer::APPS) {
    app_sync_bundle_.ProcessDeletion(extension_id, sync_change);
  } else if (extension_sync_bundle_.HasExtensionId(extension_id) &&
             sync_change.sync_data().GetDataType() == syncer::EXTENSIONS) {
    extension_sync_bundle_.ProcessDeletion(extension_id, sync_change);
  }

  delayed_installs_.Remove(extension_id);

  PruneSharedModulesOnUninstall(extension.get());

  extension_prefs_->OnExtensionUninstalled(extension_id, extension->location(),
                                           external_uninstall);

  // Track the uninstallation.
  UMA_HISTOGRAM_ENUMERATION("Extensions.ExtensionUninstalled", 1, 2);

  return true;
}

bool ExtensionService::IsExtensionEnabled(
    const std::string& extension_id) const {
  if (extensions_.Contains(extension_id) ||
      terminated_extensions_.Contains(extension_id)) {
    return true;
  }

  if (disabled_extensions_.Contains(extension_id) ||
      blacklisted_extensions_.Contains(extension_id)) {
    return false;
  }

  // If the extension hasn't been loaded yet, check the prefs for it. Assume
  // enabled unless otherwise noted.
  return !extension_prefs_->IsExtensionDisabled(extension_id) &&
         !extension_prefs_->IsExternalExtensionUninstalled(extension_id);
}

bool ExtensionService::IsExternalExtensionUninstalled(
    const std::string& extension_id) const {
  return extension_prefs_->IsExternalExtensionUninstalled(extension_id);
}

bool ExtensionService::IsExtensionEnabledForLauncher(
    const std::string& extension_id) const {
  return IsExtensionEnabled(extension_id) &&
      !GetTerminatedExtension(extension_id);
}

void ExtensionService::EnableExtension(const std::string& extension_id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (IsExtensionEnabled(extension_id))
    return;

  extension_prefs_->SetExtensionState(extension_id, Extension::ENABLED);
  extension_prefs_->ClearDisableReasons(extension_id);

  const Extension* extension = disabled_extensions_.GetByID(extension_id);
  // This can happen if sync enables an extension that is not
  // installed yet.
  if (!extension)
    return;

  if (IsUnacknowledgedExternalExtension(extension)) {
    UMA_HISTOGRAM_ENUMERATION("Extensions.ExternalExtensionEvent",
                              EXTERNAL_EXTENSION_REENABLED,
                              EXTERNAL_EXTENSION_BUCKET_BOUNDARY);
    if (extensions::ManifestURL::UpdatesFromGallery(extension)) {
      UMA_HISTOGRAM_ENUMERATION("Extensions.ExternalExtensionEventWebstore",
                                EXTERNAL_EXTENSION_REENABLED,
                                EXTERNAL_EXTENSION_BUCKET_BOUNDARY);
    } else {
      UMA_HISTOGRAM_ENUMERATION("Extensions.ExternalExtensionEventNonWebstore",
                                EXTERNAL_EXTENSION_REENABLED,
                                EXTERNAL_EXTENSION_BUCKET_BOUNDARY);
    }
    AcknowledgeExternalExtension(extension->id());
  }

  // Move it over to the enabled list.
  extensions_.Insert(make_scoped_refptr(extension));
  disabled_extensions_.Remove(extension->id());

  NotifyExtensionLoaded(extension);

  // Notify listeners that the extension was enabled.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_ENABLED,
      content::Source<Profile>(profile_),
      content::Details<const Extension>(extension));

  SyncExtensionChangeIfNeeded(*extension);
}

void ExtensionService::DisableExtension(
    const std::string& extension_id,
    Extension::DisableReason disable_reason) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // The extension may have been disabled already.
  if (!IsExtensionEnabled(extension_id))
    return;

  const Extension* extension = GetInstalledExtension(extension_id);
  // |extension| can be NULL if sync disables an extension that is not
  // installed yet.
  if (extension &&
      disable_reason != Extension::DISABLE_RELOAD &&
      !system_->management_policy()->UserMayModifySettings(extension, NULL)) {
    return;
  }

  extension_prefs_->SetExtensionState(extension_id, Extension::DISABLED);
  extension_prefs_->AddDisableReason(extension_id, disable_reason);

  int include_mask = INCLUDE_EVERYTHING & ~INCLUDE_DISABLED;
  extension = GetExtensionById(extension_id, include_mask);
  if (!extension)
    return;

  // Reset the background_page_ready flag
  if (extensions::BackgroundInfo::HasBackgroundPage(extension))
    extension_runtime_data_[extension->id()].background_page_ready = false;

  // Move it over to the disabled list. Don't send a second unload notification
  // for terminated extensions being disabled.
  disabled_extensions_.Insert(make_scoped_refptr(extension));
  if (extensions_.Contains(extension->id())) {
    extensions_.Remove(extension->id());
    NotifyExtensionUnloaded(extension, extension_misc::UNLOAD_REASON_DISABLE);
  } else {
    terminated_extensions_.Remove(extension->id());
  }

  SyncExtensionChangeIfNeeded(*extension);
}

void ExtensionService::DisableUserExtensions(
    const std::vector<std::string>& except_ids) {
  extensions::ManagementPolicy* management_policy =
      system_->management_policy();
  extensions::ExtensionList to_disable;

  for (ExtensionSet::const_iterator extension = extensions_.begin();
      extension != extensions_.end(); ++extension) {
    if (management_policy->UserMayModifySettings(extension->get(), NULL))
      to_disable.push_back(*extension);
  }
  for (ExtensionSet::const_iterator extension = terminated_extensions_.begin();
      extension != terminated_extensions_.end(); ++extension) {
    if (management_policy->UserMayModifySettings(extension->get(), NULL))
      to_disable.push_back(*extension);
  }

  for (extensions::ExtensionList::const_iterator extension = to_disable.begin();
      extension != to_disable.end(); ++extension) {
    if ((*extension)->was_installed_by_default() &&
        extension_urls::IsWebstoreUpdateUrl(
            extensions::ManifestURL::GetUpdateURL(*extension)))
      continue;
    const std::string& id = (*extension)->id();
    if (except_ids.end() == std::find(except_ids.begin(), except_ids.end(), id))
      DisableExtension(id, extensions::Extension::DISABLE_USER_ACTION);
  }
}

void ExtensionService::GrantPermissionsAndEnableExtension(
    const Extension* extension) {
  GrantPermissions(extension);
  RecordPermissionMessagesHistogram(
      extension, "Extensions.Permissions_ReEnable");
  extension_prefs_->SetDidExtensionEscalatePermissions(extension, false);
  EnableExtension(extension->id());
}

void ExtensionService::GrantPermissions(const Extension* extension) {
  CHECK(extension);
  extensions::PermissionsUpdater perms_updater(profile());
  perms_updater.GrantActivePermissions(extension);
}

// static
void ExtensionService::RecordPermissionMessagesHistogram(
    const Extension* extension, const char* histogram) {
  // Since this is called from multiple sources, and since the histogram macros
  // use statics, we need to manually lookup the histogram ourselves.
  base::HistogramBase* counter = base::LinearHistogram::FactoryGet(
      histogram,
      1,
      PermissionMessage::kEnumBoundary,
      PermissionMessage::kEnumBoundary + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);

  PermissionMessages permissions =
      extensions::PermissionsData::GetPermissionMessages(extension);
  if (permissions.empty()) {
    counter->Add(PermissionMessage::kNone);
  } else {
    for (PermissionMessages::iterator it = permissions.begin();
         it != permissions.end(); ++it)
      counter->Add(it->id());
  }
}

void ExtensionService::NotifyExtensionLoaded(const Extension* extension) {
  // The ChromeURLRequestContexts need to be first to know that the extension
  // was loaded, otherwise a race can arise where a renderer that is created
  // for the extension may try to load an extension URL with an extension id
  // that the request context doesn't yet know about. The profile is responsible
  // for ensuring its URLRequestContexts appropriately discover the loaded
  // extension.
  system_->RegisterExtensionWithRequestContexts(extension);

  // Tell renderers about the new extension, unless it's a theme (renderers
  // don't need to know about themes).
  if (!extension->is_theme()) {
    for (content::RenderProcessHost::iterator i(
            content::RenderProcessHost::AllHostsIterator());
         !i.IsAtEnd(); i.Advance()) {
      content::RenderProcessHost* host = i.GetCurrentValue();
      Profile* host_profile =
          Profile::FromBrowserContext(host->GetBrowserContext());
      if (host_profile->GetOriginalProfile() ==
          profile_->GetOriginalProfile()) {
        std::vector<ExtensionMsg_Loaded_Params> loaded_extensions(
            1, ExtensionMsg_Loaded_Params(extension));
        host->Send(
            new ExtensionMsg_Loaded(loaded_extensions));
      }
    }
  }

  // Tell subsystems that use the EXTENSION_LOADED notification about the new
  // extension.
  //
  // NOTE: It is important that this happen after notifying the renderers about
  // the new extensions so that if we navigate to an extension URL in
  // NOTIFICATION_EXTENSION_LOADED, the renderer is guaranteed to know about it.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_LOADED,
      content::Source<Profile>(profile_),
      content::Details<const Extension>(extension));

  // Tell a random-ass collection of other subsystems about the new extension.
  // TODO(aa): What should we do with all this goop? Can it move into the
  // relevant objects via EXTENSION_LOADED?

  profile_->GetExtensionSpecialStoragePolicy()->
      GrantRightsForExtension(extension);

  UpdateActiveExtensionsInCrashReporter();

  // If the extension has permission to load chrome://favicon/ resources we need
  // to make sure that the FaviconSource is registered with the
  // ChromeURLDataManager.
  if (extensions::PermissionsData::HasHostPermission(
          extension, GURL(chrome::kChromeUIFaviconURL))) {
    FaviconSource* favicon_source = new FaviconSource(profile_,
                                                      FaviconSource::FAVICON);
    content::URLDataSource::Add(profile_, favicon_source);
  }

#if !defined(OS_ANDROID)
  // Same for chrome://theme/ resources.
  if (extensions::PermissionsData::HasHostPermission(
          extension, GURL(chrome::kChromeUIThemeURL))) {
    ThemeSource* theme_source = new ThemeSource(profile_);
    content::URLDataSource::Add(profile_, theme_source);
  }
#endif

  // Same for chrome://thumb/ resources.
  if (extensions::PermissionsData::HasHostPermission(
          extension, GURL(chrome::kChromeUIThumbnailURL))) {
    ThumbnailSource* thumbnail_source = new ThumbnailSource(profile_, false);
    content::URLDataSource::Add(profile_, thumbnail_source);
  }
}

void ExtensionService::NotifyExtensionUnloaded(
    const Extension* extension,
    extension_misc::UnloadedExtensionReason reason) {
  UnloadedExtensionInfo details(extension, reason);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_UNLOADED,
      content::Source<Profile>(profile_),
      content::Details<UnloadedExtensionInfo>(&details));

  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    content::RenderProcessHost* host = i.GetCurrentValue();
    Profile* host_profile =
        Profile::FromBrowserContext(host->GetBrowserContext());
    if (host_profile->GetOriginalProfile() == profile_->GetOriginalProfile())
      host->Send(new ExtensionMsg_Unloaded(extension->id()));
  }

  system_->UnregisterExtensionWithRequestContexts(extension->id(), reason);
  profile_->GetExtensionSpecialStoragePolicy()->
      RevokeRightsForExtension(extension);

#if defined(OS_CHROMEOS)
  // Revoke external file access for the extension from its file system context.
  // It is safe to access the extension's storage partition at this point. The
  // storage partition may get destroyed only after the extension gets unloaded.
  GURL site = extensions::ExtensionSystem::Get(profile_)->extension_service()->
      GetSiteForExtensionId(extension->id());
  fileapi::FileSystemContext* filesystem_context =
      BrowserContext::GetStoragePartitionForSite(profile_, site)->
          GetFileSystemContext();
  if (filesystem_context && filesystem_context->external_backend()) {
    filesystem_context->external_backend()->
        RevokeAccessForExtension(extension->id());
  }
#endif

  UpdateActiveExtensionsInCrashReporter();
}

Profile* ExtensionService::profile() {
  return profile_;
}

extensions::ExtensionPrefs* ExtensionService::extension_prefs() {
  return extension_prefs_;
}

extensions::SettingsFrontend* ExtensionService::settings_frontend() {
  return settings_frontend_.get();
}

extensions::ContentSettingsStore* ExtensionService::GetContentSettingsStore() {
  return extension_prefs()->content_settings_store();
}

bool ExtensionService::is_ready() {
  return ready_->is_signaled();
}

base::SequencedTaskRunner* ExtensionService::GetFileTaskRunner() {
  if (file_task_runner_.get())
    return file_task_runner_.get();

  // We should be able to interrupt any part of extension install process during
  // shutdown. SKIP_ON_SHUTDOWN ensures that not started extension install tasks
  // will be ignored/deleted while we will block on started tasks.
  std::string token("ext_install-");
  token.append(profile_->GetPath().AsUTF8Unsafe());
  file_task_runner_ = BrowserThread::GetBlockingPool()->
      GetSequencedTaskRunnerWithShutdownBehavior(
        BrowserThread::GetBlockingPool()->GetNamedSequenceToken(token),
        base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  return file_task_runner_.get();
}

extensions::ExtensionUpdater* ExtensionService::updater() {
  return updater_.get();
}

void ExtensionService::CheckManagementPolicy() {
  std::vector<std::string> to_be_removed;

  // Loop through extensions list, unload installed extensions.
  for (ExtensionSet::const_iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    const Extension* extension = (iter->get());
    if (!system_->management_policy()->UserMayLoad(extension, NULL))
      to_be_removed.push_back(extension->id());
  }

  // UnloadExtension will change the extensions_ list. So, we should
  // call it outside the iterator loop.
  for (size_t i = 0; i < to_be_removed.size(); ++i)
    UnloadExtension(to_be_removed[i], extension_misc::UNLOAD_REASON_DISABLE);
}

void ExtensionService::CheckForUpdatesSoon() {
  if (updater()) {
    if (AreAllExternalProvidersReady()) {
      updater()->CheckSoon();
    } else {
      // Sync can start updating before all the external providers are ready
      // during startup. Start the update as soon as those providers are ready,
      // but not before.
      update_once_all_providers_are_ready_ = true;
    }
  } else {
    LOG(WARNING) << "CheckForUpdatesSoon() called with auto-update turned off";
  }
}

syncer::SyncMergeResult ExtensionService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  CHECK(sync_processor.get());
  CHECK(sync_error_factory.get());

  switch (type) {
    case syncer::EXTENSIONS:
      extension_sync_bundle_.SetupSync(sync_processor.release(),
                                       sync_error_factory.release(),
                                       initial_sync_data);
      break;

    case syncer::APPS:
      app_sync_bundle_.SetupSync(sync_processor.release(),
                                 sync_error_factory.release(),
                                 initial_sync_data);
      break;

    default:
      LOG(FATAL) << "Got " << type << " ModelType";
  }

  // Process local extensions.
  // TODO(yoz): Determine whether pending extensions should be considered too.
  //            See crbug.com/104399.
  syncer::SyncDataList sync_data_list = GetAllSyncData(type);
  syncer::SyncChangeList sync_change_list;
  for (syncer::SyncDataList::const_iterator i = sync_data_list.begin();
       i != sync_data_list.end();
       ++i) {
    switch (type) {
        case syncer::EXTENSIONS:
          sync_change_list.push_back(
              extension_sync_bundle_.CreateSyncChange(*i));
          break;
        case syncer::APPS:
          sync_change_list.push_back(app_sync_bundle_.CreateSyncChange(*i));
          break;
      default:
        LOG(FATAL) << "Got " << type << " ModelType";
    }
  }


  if (type == syncer::EXTENSIONS) {
    extension_sync_bundle_.ProcessSyncChangeList(sync_change_list);
  } else if (type == syncer::APPS) {
    app_sync_bundle_.ProcessSyncChangeList(sync_change_list);
  }

  return syncer::SyncMergeResult(type);
}

void ExtensionService::StopSyncing(syncer::ModelType type) {
  if (type == syncer::APPS) {
    app_sync_bundle_.Reset();
  } else if (type == syncer::EXTENSIONS) {
    extension_sync_bundle_.Reset();
  }
}

syncer::SyncDataList ExtensionService::GetAllSyncData(
    syncer::ModelType type) const {
  if (type == syncer::EXTENSIONS)
    return extension_sync_bundle_.GetAllSyncData();
  if (type == syncer::APPS)
    return app_sync_bundle_.GetAllSyncData();

  // We should only get sync data for extensions and apps.
  NOTREACHED();

  return syncer::SyncDataList();
}

syncer::SyncError ExtensionService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  for (syncer::SyncChangeList::const_iterator i = change_list.begin();
      i != change_list.end();
      ++i) {
    syncer::ModelType type = i->sync_data().GetDataType();
    if (type == syncer::EXTENSIONS) {
      extension_sync_bundle_.ProcessSyncChange(
          extensions::ExtensionSyncData(*i));
    } else if (type == syncer::APPS) {
      app_sync_bundle_.ProcessSyncChange(extensions::AppSyncData(*i));
    }
  }

  extension_prefs()->extension_sorting()->FixNTPOrdinalCollisions();

  return syncer::SyncError();
}

extensions::ExtensionSyncData ExtensionService::GetExtensionSyncData(
    const Extension& extension) const {
  return extensions::ExtensionSyncData(extension,
                                       IsExtensionEnabled(extension.id()),
                                       IsIncognitoEnabled(extension.id()));
}

extensions::AppSyncData ExtensionService::GetAppSyncData(
    const Extension& extension) const {
  return extensions::AppSyncData(
      extension,
      IsExtensionEnabled(extension.id()),
      IsIncognitoEnabled(extension.id()),
      extension_prefs_->extension_sorting()->GetAppLaunchOrdinal(
          extension.id()),
      extension_prefs_->extension_sorting()->GetPageOrdinal(extension.id()));
}

std::vector<extensions::ExtensionSyncData>
  ExtensionService::GetExtensionSyncDataList() const {
  std::vector<extensions::ExtensionSyncData> extension_sync_list;
  extension_sync_bundle_.GetExtensionSyncDataListHelper(extensions_,
                                                        &extension_sync_list);
  extension_sync_bundle_.GetExtensionSyncDataListHelper(disabled_extensions_,
                                                        &extension_sync_list);
  extension_sync_bundle_.GetExtensionSyncDataListHelper(terminated_extensions_,
                                                        &extension_sync_list);

  std::vector<extensions::ExtensionSyncData> pending_extensions =
      extension_sync_bundle_.GetPendingData();
  extension_sync_list.insert(extension_sync_list.begin(),
                             pending_extensions.begin(),
                             pending_extensions.end());

  return extension_sync_list;
}

std::vector<extensions::AppSyncData> ExtensionService::GetAppSyncDataList()
    const {
  std::vector<extensions::AppSyncData> app_sync_list;
  app_sync_bundle_.GetAppSyncDataListHelper(extensions_, &app_sync_list);
  app_sync_bundle_.GetAppSyncDataListHelper(disabled_extensions_,
                                            &app_sync_list);
  app_sync_bundle_.GetAppSyncDataListHelper(terminated_extensions_,
                                            &app_sync_list);

  std::vector<extensions::AppSyncData> pending_apps =
      app_sync_bundle_.GetPendingData();
  app_sync_list.insert(app_sync_list.begin(),
                       pending_apps.begin(),
                       pending_apps.end());

  return app_sync_list;
}

bool ExtensionService::ProcessExtensionSyncData(
    const extensions::ExtensionSyncData& extension_sync_data) {
  if (!ProcessExtensionSyncDataHelper(extension_sync_data,
                                      syncer::EXTENSIONS)) {
    extension_sync_bundle_.AddPendingExtension(extension_sync_data.id(),
                                               extension_sync_data);
    CheckForUpdatesSoon();
    return false;
  }

  return true;
}

bool ExtensionService::ProcessAppSyncData(
    const extensions::AppSyncData& app_sync_data) {
  const std::string& id = app_sync_data.id();

  if (app_sync_data.app_launch_ordinal().IsValid() &&
      app_sync_data.page_ordinal().IsValid()) {
    extension_prefs_->extension_sorting()->SetAppLaunchOrdinal(
        id,
        app_sync_data.app_launch_ordinal());
    extension_prefs_->extension_sorting()->SetPageOrdinal(
        id,
        app_sync_data.page_ordinal());
  }

  if (!ProcessExtensionSyncDataHelper(app_sync_data.extension_sync_data(),
                                      syncer::APPS)) {
    app_sync_bundle_.AddPendingApp(id, app_sync_data);
    CheckForUpdatesSoon();
    return false;
  }

  return true;
}

bool ExtensionService::IsCorrectSyncType(const Extension& extension,
                                         syncer::ModelType type) const {
  if (type == syncer::EXTENSIONS &&
      extensions::sync_helper::IsSyncableExtension(&extension)) {
    return true;
  }

  if (type == syncer::APPS &&
      extensions::sync_helper::IsSyncableApp(&extension)) {
    return true;
  }

  return false;
}

bool ExtensionService::ProcessExtensionSyncDataHelper(
    const extensions::ExtensionSyncData& extension_sync_data,
    syncer::ModelType type) {
  const std::string& id = extension_sync_data.id();
  const Extension* extension = GetInstalledExtension(id);

  // TODO(bolms): we should really handle this better.  The particularly bad
  // case is where an app becomes an extension or vice versa, and we end up with
  // a zombie extension that won't go away.
  if (extension && !IsCorrectSyncType(*extension, type))
    return true;

  // Handle uninstalls first.
  if (extension_sync_data.uninstalled()) {
    if (!UninstallExtensionHelper(this, id)) {
      LOG(WARNING) << "Could not uninstall extension " << id
                   << " for sync";
    }
    return true;
  }

  // Extension from sync was uninstalled by the user as external extensions.
  // Honor user choice and skip installation/enabling.
  if (IsExternalExtensionUninstalled(id)) {
    LOG(WARNING) << "Extension with id " << id
                 << " from sync was uninstalled as external extension";
    return true;
  }

  // Set user settings.
  // If the extension has been disabled from sync, it may not have
  // been installed yet, so we don't know if the disable reason was a
  // permissions increase.  That will be updated once CheckPermissionsIncrease
  // is called for it.
  if (extension_sync_data.enabled())
    EnableExtension(id);
  else
    DisableExtension(id, Extension::DISABLE_UNKNOWN_FROM_SYNC);

  // We need to cache some version information here because setting the
  // incognito flag invalidates the |extension| pointer (it reloads the
  // extension).
  bool extension_installed = (extension != NULL);
  int result = extension ?
      extension->version()->CompareTo(extension_sync_data.version()) : 0;
  SetIsIncognitoEnabled(id, extension_sync_data.incognito_enabled());
  extension = NULL;  // No longer safe to use.

  if (extension_installed) {
    // If the extension is already installed, check if it's outdated.
    if (result < 0) {
      // Extension is outdated.
      return false;
    }
  } else {
    // TODO(akalin): Replace silent update with a list of enabled
    // permissions.
    const bool kInstallSilently = true;

    CHECK(type == syncer::EXTENSIONS || type == syncer::APPS);
    extensions::PendingExtensionInfo::ShouldAllowInstallPredicate filter =
        (type == syncer::APPS) ? extensions::sync_helper::IsSyncableApp :
                                 extensions::sync_helper::IsSyncableExtension;

    if (!pending_extension_manager()->AddFromSync(
            id,
            extension_sync_data.update_url(),
            filter,
            kInstallSilently)) {
      LOG(WARNING) << "Could not add pending extension for " << id;
      // This means that the extension is already pending installation, with a
      // non-INTERNAL location.  Add to pending_sync_data, even though it will
      // never be removed (we'll never install a syncable version of the
      // extension), so that GetAllSyncData() continues to send it.
    }
    // Track pending extensions so that we can return them in GetAllSyncData().
    return false;
  }

  return true;
}

bool ExtensionService::IsIncognitoEnabled(
    const std::string& extension_id) const {
  const Extension* extension = GetInstalledExtension(extension_id);
  if (extension && !extension->can_be_incognito_enabled())
    return false;
  // If this is an existing component extension we always allow it to
  // work in incognito mode.
  if (extension && extension->location() == Manifest::COMPONENT)
    return true;
  if (extension && extension->force_incognito_enabled())
    return true;

  // Check the prefs.
  return extension_prefs_->IsIncognitoEnabled(extension_id);
}

void ExtensionService::SetIsIncognitoEnabled(
    const std::string& extension_id, bool enabled) {
  const Extension* extension = GetInstalledExtension(extension_id);
  if (extension && !extension->can_be_incognito_enabled())
    return;
  if (extension && extension->location() == Manifest::COMPONENT) {
    // This shouldn't be called for component extensions unless they are
    // syncable.
    DCHECK(extensions::sync_helper::IsSyncable(extension));

    // If we are here, make sure the we aren't trying to change the value.
    DCHECK_EQ(enabled, IsIncognitoEnabled(extension_id));

    return;
  }

  // Broadcast unloaded and loaded events to update browser state. Only bother
  // if the value changed and the extension is actually enabled, since there is
  // no UI otherwise.
  bool old_enabled = extension_prefs_->IsIncognitoEnabled(extension_id);
  if (enabled == old_enabled)
    return;

  extension_prefs_->SetIsIncognitoEnabled(extension_id, enabled);

  bool extension_is_enabled = extensions_.Contains(extension_id);

  // When we reload the extension the ID may be invalidated if we've passed it
  // by const ref everywhere. Make a copy to be safe.
  std::string id = extension_id;
  if (extension_is_enabled)
    ReloadExtension(id);

  // Reloading the extension invalidates the |extension| pointer.
  extension = GetInstalledExtension(id);
  if (extension)
    SyncExtensionChangeIfNeeded(*extension);
}

bool ExtensionService::CanCrossIncognito(const Extension* extension) const {
  // We allow the extension to see events and data from another profile iff it
  // uses "spanning" behavior and it has incognito access. "split" mode
  // extensions only see events for a matching profile.
  CHECK(extension);
  return IsIncognitoEnabled(extension->id()) &&
         !extensions::IncognitoInfo::IsSplitMode(extension);
}

bool ExtensionService::CanLoadInIncognito(const Extension* extension) const {
  if (extension->is_hosted_app())
    return true;
  // Packaged apps and regular extensions need to be enabled specifically for
  // incognito (and split mode should be set).
  return extensions::IncognitoInfo::IsSplitMode(extension) &&
         IsIncognitoEnabled(extension->id());
}

void ExtensionService::OnExtensionMoved(
    const std::string& moved_extension_id,
    const std::string& predecessor_extension_id,
    const std::string& successor_extension_id) {
  extension_prefs_->extension_sorting()->OnExtensionMoved(
      moved_extension_id,
      predecessor_extension_id,
      successor_extension_id);

  const Extension* extension = GetInstalledExtension(moved_extension_id);
  if (extension)
    SyncExtensionChangeIfNeeded(*extension);
}

bool ExtensionService::AllowFileAccess(const Extension* extension) const {
  return (CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDisableExtensionsFileAccessCheck) ||
          extension_prefs_->AllowFileAccess(extension->id()));
}

void ExtensionService::SetAllowFileAccess(const Extension* extension,
                                          bool allow) {
  // Reload to update browser state. Only bother if the value changed and the
  // extension is actually enabled, since there is no UI otherwise.
  bool old_allow = AllowFileAccess(extension);
  if (allow == old_allow)
    return;

  extension_prefs_->SetAllowFileAccess(extension->id(), allow);

  bool extension_is_enabled = extensions_.Contains(extension->id());
  if (extension_is_enabled)
    ReloadExtension(extension->id());
}

// Some extensions will autoupdate themselves externally from Chrome.  These
// are typically part of some larger client application package.  To support
// these, the extension will register its location in the the preferences file
// (and also, on Windows, in the registry) and this code will periodically
// check that location for a .crx file, which it will then install locally if
// a new version is available.
// Errors are reported through ExtensionErrorReporter. Succcess is not
// reported.
void ExtensionService::CheckForExternalUpdates() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Note that this installation is intentionally silent (since it didn't
  // go through the front-end).  Extensions that are registered in this
  // way are effectively considered 'pre-bundled', and so implicitly
  // trusted.  In general, if something has HKLM or filesystem access,
  // they could install an extension manually themselves anyway.

  // Ask each external extension provider to give us a call back for each
  // extension they know about. See OnExternalExtension(File|UpdateUrl)Found.
  extensions::ProviderCollection::const_iterator i;
  for (i = external_extension_providers_.begin();
       i != external_extension_providers_.end(); ++i) {
    extensions::ExternalProviderInterface* provider = i->get();
    provider->VisitRegisteredExtension();
  }

  // Do any required work that we would have done after completion of all
  // providers.
  if (external_extension_providers_.empty()) {
    OnAllExternalProvidersReady();
  }
}

void ExtensionService::OnExternalProviderReady(
    const extensions::ExternalProviderInterface* provider) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(provider->IsReady());

  // An external provider has finished loading.  We only take action
  // if all of them are finished. So we check them first.
  if (AreAllExternalProvidersReady())
    OnAllExternalProvidersReady();
}

bool ExtensionService::AreAllExternalProvidersReady() const {
  extensions::ProviderCollection::const_iterator i;
  for (i = external_extension_providers_.begin();
       i != external_extension_providers_.end(); ++i) {
    if (!i->get()->IsReady())
      return false;
  }
  return true;
}

void ExtensionService::OnAllExternalProvidersReady() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::TimeDelta elapsed = base::Time::Now() - profile_->GetStartTime();
  UMA_HISTOGRAM_TIMES("Extension.ExternalProvidersReadyAfter", elapsed);

  // Install any pending extensions.
  if (update_once_all_providers_are_ready_ && updater()) {
    update_once_all_providers_are_ready_ = false;
    updater()->CheckNow(extensions::ExtensionUpdater::CheckParams());
  }

  // Uninstall all the unclaimed extensions.
  scoped_ptr<extensions::ExtensionPrefs::ExtensionsInfo> extensions_info(
      extension_prefs_->GetInstalledExtensionsInfo());
  for (size_t i = 0; i < extensions_info->size(); ++i) {
    ExtensionInfo* info = extensions_info->at(i).get();
    if (Manifest::IsExternalLocation(info->extension_location))
      CheckExternalUninstall(info->extension_id);
  }
  IdentifyAlertableExtensions();
}

void ExtensionService::IdentifyAlertableExtensions() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Build up the lists of extensions that require acknowledgment. If this is
  // the first time, grandfather extensions that would have caused
  // notification.
  extension_error_ui_.reset(ExtensionErrorUI::Create(this));

  bool did_show_alert = false;
  if (PopulateExtensionErrorUI(extension_error_ui_.get())) {
    if (!is_first_run_) {
      CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
      did_show_alert = extension_error_ui_->ShowErrorInBubbleView();
    } else {
      // First run. Just acknowledge all the extensions, silently, by
      // shortcutting the display of the UI and going straight to the
      // callback for pressing the Accept button.
      HandleExtensionAlertAccept();
    }
  }

  UpdateExternalExtensionAlert();

  if (!did_show_alert)
    extension_error_ui_.reset();
}

bool ExtensionService::PopulateExtensionErrorUI(
    ExtensionErrorUI* extension_error_ui) {
  bool needs_alert = false;

  // Extensions that are blacklisted.
  for (ExtensionSet::const_iterator it = blacklisted_extensions_.begin();
       it != blacklisted_extensions_.end(); ++it) {
    std::string id = (*it)->id();
    if (!extension_prefs_->IsBlacklistedExtensionAcknowledged(id)) {
      extension_error_ui->AddBlacklistedExtension(id);
      needs_alert = true;
    }
  }

  for (ExtensionSet::const_iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    const Extension* e = iter->get();

    // Extensions disabled by policy. Note: this no longer includes blacklisted
    // extensions, though we still show the same UI.
    if (!system_->management_policy()->UserMayLoad(e, NULL)) {
      if (!extension_prefs_->IsBlacklistedExtensionAcknowledged(e->id())) {
        extension_error_ui->AddBlacklistedExtension(e->id());
        needs_alert = true;
      }
    }
  }

  return needs_alert;
}

void ExtensionService::HandleExtensionAlertClosed() {
  const ExtensionIdSet* extension_ids =
      extension_error_ui_->get_blacklisted_extension_ids();
  for (ExtensionIdSet::const_iterator iter = extension_ids->begin();
       iter != extension_ids->end(); ++iter) {
    extension_prefs_->AcknowledgeBlacklistedExtension(*iter);
  }
  extension_error_ui_.reset();
}

void ExtensionService::HandleExtensionAlertAccept() {
  extension_error_ui_->Close();
}

void ExtensionService::AcknowledgeExternalExtension(const std::string& id) {
  extension_prefs_->AcknowledgeExternalExtension(id);
  UpdateExternalExtensionAlert();
}

bool ExtensionService::IsUnacknowledgedExternalExtension(
    const Extension* extension) {
  if (!FeatureSwitch::prompt_for_external_extensions()->IsEnabled())
    return false;

  return (Manifest::IsExternalLocation(extension->location()) &&
          !extension_prefs_->IsExternalExtensionAcknowledged(extension->id()) &&
          !(extension_prefs_->GetDisableReasons(extension->id()) &
                Extension::DISABLE_SIDELOAD_WIPEOUT));
}

void ExtensionService::HandleExtensionAlertDetails() {
  extension_error_ui_->ShowExtensions();
  // ShowExtensions may cause the error UI to close synchronously, e.g. if it
  // causes a navigation.
  if (extension_error_ui_)
    extension_error_ui_->Close();
}

void ExtensionService::UpdateExternalExtensionAlert() {
  if (!FeatureSwitch::prompt_for_external_extensions()->IsEnabled())
    return;

  const Extension* extension = NULL;
  for (ExtensionSet::const_iterator iter = disabled_extensions_.begin();
       iter != disabled_extensions_.end(); ++iter) {
    const Extension* e = iter->get();
    if (IsUnacknowledgedExternalExtension(e)) {
      extension = e;
      break;
    }
  }

  if (extension) {
    if (!extensions::HasExternalInstallError(this)) {
      if (extension_prefs_->IncrementAcknowledgePromptCount(extension->id()) >
              kMaxExtensionAcknowledgePromptCount) {
        // Stop prompting for this extension, and check if there's another
        // one that needs prompting.
        extension_prefs_->AcknowledgeExternalExtension(extension->id());
        UpdateExternalExtensionAlert();
        UMA_HISTOGRAM_ENUMERATION("Extensions.ExternalExtensionEvent",
                                  EXTERNAL_EXTENSION_IGNORED,
                                  EXTERNAL_EXTENSION_BUCKET_BOUNDARY);
        if (extensions::ManifestURL::UpdatesFromGallery(extension)) {
          UMA_HISTOGRAM_ENUMERATION(
              "Extensions.ExternalExtensionEventWebstore",
              EXTERNAL_EXTENSION_IGNORED,
              EXTERNAL_EXTENSION_BUCKET_BOUNDARY);
        } else {
          UMA_HISTOGRAM_ENUMERATION(
              "Extensions.ExternalExtensionEventNonWebstore",
              EXTERNAL_EXTENSION_IGNORED,
              EXTERNAL_EXTENSION_BUCKET_BOUNDARY);
        }
        return;
      }
      if (is_first_run_)
        extension_prefs_->SetExternalInstallFirstRun(extension->id());
      // first_run is true if the extension was installed during a first run
      // (even if it's post-first run now).
      bool first_run = extension_prefs_->IsExternalInstallFirstRun(
          extension->id());
      extensions::AddExternalInstallError(this, extension, first_run);
    }
  } else {
    extensions::RemoveExternalInstallError(this);
  }
}

void ExtensionService::UnloadExtension(
    const std::string& extension_id,
    extension_misc::UnloadedExtensionReason reason) {
  // Make sure the extension gets deleted after we return from this function.
  int include_mask = INCLUDE_EVERYTHING & ~INCLUDE_TERMINATED;
  scoped_refptr<const Extension> extension(
      GetExtensionById(extension_id, include_mask));

  // This method can be called via PostTask, so the extension may have been
  // unloaded by the time this runs.
  if (!extension.get()) {
    // In case the extension may have crashed/uninstalled. Allow the profile to
    // clean up its RequestContexts.
    system_->UnregisterExtensionWithRequestContexts(extension_id, reason);
    return;
  }

  // If uninstalling let RuntimeEventRouter know.
  if (reason == extension_misc::UNLOAD_REASON_UNINSTALL)
    extensions::RuntimeEventRouter::OnExtensionUninstalled(
        profile_, extension_id);

  // Keep information about the extension so that we can reload it later
  // even if it's not permanently installed.
  unloaded_extension_paths_[extension->id()] = extension->path();

  // Clean up if the extension is meant to be enabled after a reload.
  reloading_extensions_.erase(extension->id());

  // Clean up runtime data.
  extension_runtime_data_.erase(extension_id);

  if (disabled_extensions_.Contains(extension->id())) {
    disabled_extensions_.Remove(extension->id());
    // Make sure the profile cleans up its RequestContexts when an already
    // disabled extension is unloaded (since they are also tracking the disabled
    // extensions).
    system_->UnregisterExtensionWithRequestContexts(extension_id, reason);
  } else {
    // Remove the extension from our list.
    extensions_.Remove(extension->id());
    NotifyExtensionUnloaded(extension.get(), reason);
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_REMOVED,
      content::Source<Profile>(profile_),
      content::Details<const Extension>(extension.get()));
}

void ExtensionService::RemoveComponentExtension(
    const std::string& extension_id) {
  scoped_refptr<const Extension> extension(
      GetExtensionById(extension_id, false));
  UnloadExtension(extension_id, extension_misc::UNLOAD_REASON_UNINSTALL);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
      content::Source<Profile>(profile_),
      content::Details<const Extension>(extension.get()));
}

void ExtensionService::UnloadAllExtensions() {
  profile_->GetExtensionSpecialStoragePolicy()->RevokeRightsForAllExtensions();

  extensions_.Clear();
  disabled_extensions_.Clear();
  terminated_extensions_.Clear();
  extension_runtime_data_.clear();

  // TODO(erikkay) should there be a notification for this?  We can't use
  // EXTENSION_UNLOADED since that implies that the extension has been disabled
  // or uninstalled, and UnloadAll is just part of shutdown.
}

void ExtensionService::ReloadExtensions() {
  UnloadAllExtensions();
  component_loader_->LoadAll();
  extensions::InstalledLoader(this).LoadAllExtensions();
  // Don't call SetReadyAndNotifyListeners() since tests call this multiple
  // times.
}

void ExtensionService::GarbageCollectExtensions() {
#if defined(OS_CHROMEOS)
  if (disable_garbage_collection_)
    return;
#endif

  if (extension_prefs_->pref_service()->ReadOnly())
    return;

  if (pending_extension_manager()->HasPendingExtensions()) {
    // Don't garbage collect while there are pending installations, which may
    // be using the temporary installation directory. Try to garbage collect
    // again later.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ExtensionService::GarbageCollectExtensions, AsWeakPtr()),
        base::TimeDelta::FromSeconds(kGarbageCollectRetryDelay));
    return;
  }

  scoped_ptr<extensions::ExtensionPrefs::ExtensionsInfo> info(
      extension_prefs_->GetInstalledExtensionsInfo());

  std::multimap<std::string, base::FilePath> extension_paths;
  for (size_t i = 0; i < info->size(); ++i)
    extension_paths.insert(std::make_pair(info->at(i)->extension_id,
                                          info->at(i)->extension_path));

  info = extension_prefs_->GetAllDelayedInstallInfo();
  for (size_t i = 0; i < info->size(); ++i)
    extension_paths.insert(std::make_pair(info->at(i)->extension_id,
                                          info->at(i)->extension_path));

  if (!GetFileTaskRunner()->PostTask(
          FROM_HERE,
          base::Bind(
              &extension_file_util::GarbageCollectExtensions,
              install_directory_,
              extension_paths))) {
    NOTREACHED();
  }
}

void ExtensionService::SyncExtensionChangeIfNeeded(const Extension& extension) {
  if (extensions::sync_helper::IsSyncableApp(&extension)) {
    if (app_sync_bundle_.IsSyncing())
      app_sync_bundle_.SyncChangeIfNeeded(extension);
    else if (is_ready() && !flare_.is_null())
      flare_.Run(syncer::APPS);
  } else if (extensions::sync_helper::IsSyncableExtension(&extension)) {
    if (extension_sync_bundle_.IsSyncing())
      extension_sync_bundle_.SyncChangeIfNeeded(extension);
    else if (is_ready() && !flare_.is_null())
      flare_.Run(syncer::EXTENSIONS);
  }
}

void ExtensionService::SetReadyAndNotifyListeners() {
  ready_->Signal();
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSIONS_READY,
      content::Source<Profile>(profile_),
      content::NotificationService::NoDetails());
}

void ExtensionService::OnLoadedInstalledExtensions() {
  if (updater_)
    updater_->Start();

  OnBlacklistUpdated();
}

void ExtensionService::AddExtension(const Extension* extension) {
  // TODO(jstritar): We may be able to get rid of this branch by overriding the
  // default extension state to DISABLED when the --disable-extensions flag
  // is set (http://crbug.com/29067).
  if (!extensions_enabled() &&
      !extension->is_theme() &&
      extension->location() != Manifest::COMPONENT &&
      !Manifest::IsExternalLocation(extension->location())) {
    return;
  }

  bool is_extension_upgrade = false;
  bool is_extension_installed = false;
  const Extension* old = GetInstalledExtension(extension->id());
  if (old) {
    is_extension_installed = true;
    int version_compare_result =
        extension->version()->CompareTo(*(old->version()));
    is_extension_upgrade = version_compare_result > 0;
    // Other than for unpacked extensions, CrxInstaller should have guaranteed
    // that we aren't downgrading.
    if (!Manifest::IsUnpackedLocation(extension->location()))
      CHECK_GE(version_compare_result, 0);
  }
  SetBeingUpgraded(extension, is_extension_upgrade);

  // The extension is now loaded, remove its data from unloaded extension map.
  unloaded_extension_paths_.erase(extension->id());

  // If a terminated extension is loaded, remove it from the terminated list.
  UntrackTerminatedExtension(extension->id());

  // If the extension was disabled for a reload, then enable it.
  bool reloading = reloading_extensions_.erase(extension->id()) > 0;

  // Check if the extension's privileges have changed and mark the
  // extension disabled if necessary.
  CheckPermissionsIncrease(extension, is_extension_installed);

  if (is_extension_installed && !reloading) {
    // To upgrade an extension in place, unload the old one and then load the
    // new one.  ReloadExtension disables the extension, which is sufficient.
    UnloadExtension(extension->id(), extension_misc::UNLOAD_REASON_UPDATE);
  }

  if (extension_prefs_->IsExtensionBlacklisted(extension->id())) {
    // Only prefs is checked for the blacklist. We rely on callers to check the
    // blacklist before calling into here, e.g. CrxInstaller checks before
    // installation then threads through the install and pending install flow
    // of this class, and we check when loading installed extensions.
    blacklisted_extensions_.Insert(extension);
  } else if (!reloading &&
             extension_prefs_->IsExtensionDisabled(extension->id())) {
    disabled_extensions_.Insert(extension);
    SyncExtensionChangeIfNeeded(*extension);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_UPDATE_DISABLED,
        content::Source<Profile>(profile_),
        content::Details<const Extension>(extension));

    // Show the extension disabled error if a permissions increase was the
    // only reason it was disabled.
    if (extension_prefs_->GetDisableReasons(extension->id()) ==
        Extension::DISABLE_PERMISSIONS_INCREASE) {
      extensions::AddExtensionDisabledError(this, extension);
    }
  } else if (reloading) {
    // Replace the old extension with the new version.
    CHECK(!disabled_extensions_.Insert(extension));
    EnableExtension(extension->id());
  } else {
    // All apps that are displayed in the launcher are ordered by their ordinals
    // so we must ensure they have valid ordinals.
    if (extension->RequiresSortOrdinal()) {
      if (!extension->ShouldDisplayInNewTabPage()) {
        extension_prefs_->extension_sorting()->MarkExtensionAsHidden(
            extension->id());
      }
      extension_prefs_->extension_sorting()->EnsureValidOrdinals(
          extension->id(), syncer::StringOrdinal());
    }

    extensions_.Insert(extension);
    SyncExtensionChangeIfNeeded(*extension);
    NotifyExtensionLoaded(extension);
  }
  SetBeingUpgraded(extension, false);
}

void ExtensionService::AddComponentExtension(const Extension* extension) {
  const std::string old_version_string(
      extension_prefs_->GetVersionString(extension->id()));
  const Version old_version(old_version_string);

  if (!old_version.IsValid() || !old_version.Equals(*extension->version())) {
    VLOG(1) << "Component extension " << extension->name() << " ("
        << extension->id() << ") installing/upgrading from '"
        << old_version_string << "' to " << extension->version()->GetString();

    AddNewOrUpdatedExtension(extension,
                             Extension::ENABLED_COMPONENT,
                             extensions::Blacklist::NOT_BLACKLISTED,
                             syncer::StringOrdinal());
    return;
  }

  AddExtension(extension);
}

void ExtensionService::UpdateActivePermissions(const Extension* extension) {
  // If the extension has used the optional permissions API, it will have a
  // custom set of active permissions defined in the extension prefs. Here,
  // we update the extension's active permissions based on the prefs.
  scoped_refptr<PermissionSet> active_permissions =
      extension_prefs()->GetActivePermissions(extension->id());

  if (active_permissions.get()) {
    // We restrict the active permissions to be within the bounds defined in the
    // extension's manifest.
    //  a) active permissions must be a subset of optional + default permissions
    //  b) active permissions must contains all default permissions
    scoped_refptr<PermissionSet> total_permissions =
        PermissionSet::CreateUnion(
            extensions::PermissionsData::GetRequiredPermissions(extension),
            extensions::PermissionsData::GetOptionalPermissions(extension));

    // Make sure the active permissions contain no more than optional + default.
    scoped_refptr<PermissionSet> adjusted_active =
        PermissionSet::CreateIntersection(
            total_permissions.get(), active_permissions.get());

    // Make sure the active permissions contain the default permissions.
    adjusted_active = PermissionSet::CreateUnion(
        extensions::PermissionsData::GetRequiredPermissions(extension),
        adjusted_active.get());

    extensions::PermissionsUpdater perms_updater(profile());
    perms_updater.UpdateActivePermissions(extension, adjusted_active.get());
  }
}

void ExtensionService::CheckPermissionsIncrease(const Extension* extension,
                                                bool is_extension_installed) {
  UpdateActivePermissions(extension);

  // We keep track of all permissions the user has granted each extension.
  // This allows extensions to gracefully support backwards compatibility
  // by including unknown permissions in their manifests. When the user
  // installs the extension, only the recognized permissions are recorded.
  // When the unknown permissions become recognized (e.g., through browser
  // upgrade), we can prompt the user to accept these new permissions.
  // Extensions can also silently upgrade to less permissions, and then
  // silently upgrade to a version that adds these permissions back.
  //
  // For example, pretend that Chrome 10 includes a permission "omnibox"
  // for an API that adds suggestions to the omnibox. An extension can
  // maintain backwards compatibility while still having "omnibox" in the
  // manifest. If a user installs the extension on Chrome 9, the browser
  // will record the permissions it recognized, not including "omnibox."
  // When upgrading to Chrome 10, "omnibox" will be recognized and Chrome
  // will disable the extension and prompt the user to approve the increase
  // in privileges. The extension could then release a new version that
  // removes the "omnibox" permission. When the user upgrades, Chrome will
  // still remember that "omnibox" had been granted, so that if the
  // extension once again includes "omnibox" in an upgrade, the extension
  // can upgrade without requiring this user's approval.
  int disable_reasons = extension_prefs_->GetDisableReasons(extension->id());

  bool auto_grant_permission =
      (!is_extension_installed && extension->was_installed_by_default()) ||
      chrome::IsRunningInForcedAppMode();
  // Silently grant all active permissions to default apps only on install.
  // After install they should behave like other apps.
  // Silently grant all active permissions to apps install in kiosk mode on both
  // install and update.
  if (auto_grant_permission)
    GrantPermissions(extension);

  bool is_privilege_increase = false;
  // We only need to compare the granted permissions to the current permissions
  // if the extension is not allowed to silently increase its permissions.
  if (!extensions::PermissionsData::CanSilentlyIncreasePermissions(extension) &&
      !auto_grant_permission) {
    // Add all the recognized permissions if the granted permissions list
    // hasn't been initialized yet.
    scoped_refptr<PermissionSet> granted_permissions =
        extension_prefs_->GetGrantedPermissions(extension->id());
    CHECK(granted_permissions.get());

    // Here, we check if an extension's privileges have increased in a manner
    // that requires the user's approval. This could occur because the browser
    // upgraded and recognized additional privileges, or an extension upgrades
    // to a version that requires additional privileges.
    is_privilege_increase = granted_permissions->HasLessPrivilegesThan(
        extension->GetActivePermissions().get(), extension->GetType());
  }

  if (is_extension_installed) {
    // If the extension was already disabled, suppress any alerts for becoming
    // disabled on permissions increase.
    bool previously_disabled =
        extension_prefs_->IsExtensionDisabled(extension->id());
    // Legacy disabled extensions do not have a disable reason. Infer that if
    // there was no permission increase, it was likely disabled by the user.
    if (previously_disabled && disable_reasons == Extension::DISABLE_NONE &&
        !extension_prefs_->DidExtensionEscalatePermissions(extension->id())) {
      disable_reasons |= Extension::DISABLE_USER_ACTION;
    }
    // Extensions that came to us disabled from sync need a similar inference,
    // except based on the new version's permissions.
    if (previously_disabled &&
        disable_reasons == Extension::DISABLE_UNKNOWN_FROM_SYNC) {
      // Remove the DISABLE_UNKNOWN_FROM_SYNC reason.
      extension_prefs_->ClearDisableReasons(extension->id());
      if (!is_privilege_increase)
        disable_reasons |= Extension::DISABLE_USER_ACTION;
    }
    disable_reasons &= ~Extension::DISABLE_UNKNOWN_FROM_SYNC;
  }

  // Extension has changed permissions significantly. Disable it. A
  // notification should be sent by the caller.
  if (is_privilege_increase) {
    disable_reasons |= Extension::DISABLE_PERMISSIONS_INCREASE;
    if (!extension_prefs_->DidExtensionEscalatePermissions(extension->id())) {
      RecordPermissionMessagesHistogram(
          extension, "Extensions.Permissions_AutoDisable");
    }
    extension_prefs_->SetExtensionState(extension->id(), Extension::DISABLED);
    extension_prefs_->SetDidExtensionEscalatePermissions(extension, true);
  }
  if (disable_reasons != Extension::DISABLE_NONE) {
    extension_prefs_->AddDisableReason(
        extension->id(),
        static_cast<Extension::DisableReason>(disable_reasons));
  }
}

void ExtensionService::UpdateActiveExtensionsInCrashReporter() {
  std::set<std::string> extension_ids;
  for (ExtensionSet::const_iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    const Extension* extension = iter->get();
    if (!extension->is_theme() && extension->location() != Manifest::COMPONENT)
      extension_ids.insert(extension->id());
  }

  crash_keys::SetActiveExtensions(extension_ids);
}

ExtensionService::ImportStatus ExtensionService::SatisfyImports(
    const Extension* extension) {
  ImportStatus status = IMPORT_STATUS_OK;
  std::vector<std::string> pending;
  // TODO(elijahtaylor): Message the user if there is a failure that is
  // unrecoverable.
  if (SharedModuleInfo::ImportsModules(extension)) {
    const std::vector<SharedModuleInfo::ImportInfo>& imports =
        SharedModuleInfo::GetImports(extension);
    std::vector<SharedModuleInfo::ImportInfo>::const_iterator i;
    for (i = imports.begin(); i != imports.end(); ++i) {
      Version version_required(i->minimum_version);
      const Extension* imported_module =
          GetExtensionById(i->extension_id, true);
      if (!imported_module) {
        if (extension->from_webstore()) {
          status = IMPORT_STATUS_UNSATISFIED;
          pending.push_back(i->extension_id);
        } else {
          return IMPORT_STATUS_UNRECOVERABLE;
        }
      } else if (!SharedModuleInfo::IsSharedModule(imported_module)) {
        return IMPORT_STATUS_UNRECOVERABLE;
      } else if (version_required.IsValid() &&
                 imported_module->version()->CompareTo(version_required) < 0) {
        if (imported_module->from_webstore()) {
          status = IMPORT_STATUS_UNSATISFIED;
        } else {
          return IMPORT_STATUS_UNRECOVERABLE;
        }
      }
    }
  }
  if (status == IMPORT_STATUS_UNSATISFIED) {
    for (std::vector<std::string>::const_iterator iter = pending.begin();
         iter != pending.end();
         ++iter) {
      pending_extension_manager()->AddFromExtensionImport(
          *iter,
          extension_urls::GetWebstoreUpdateUrl(),
          IsSharedModule);
    }
    CheckForUpdatesSoon();
  }
  return status;
}

scoped_ptr<const ExtensionSet>
    ExtensionService::GetDependentExtensions(const Extension* extension) {
  scoped_ptr<ExtensionSet> dependents(new ExtensionSet());
  scoped_ptr<ExtensionSet> set_to_check(new ExtensionSet());
  if (SharedModuleInfo::IsSharedModule(extension)) {
    set_to_check->InsertAll(disabled_extensions_);
    set_to_check->InsertAll(delayed_installs_);
    set_to_check->InsertAll(extensions_);
    for (ExtensionSet::const_iterator iter = set_to_check->begin();
         iter != set_to_check->end(); ++iter) {
      if (SharedModuleInfo::ImportsExtensionById(iter->get(),
                                                 extension->id())) {
        dependents->Insert(*iter);
      }
    }
  }
  return dependents.PassAs<const ExtensionSet>();
}

void ExtensionService::PruneSharedModulesOnUninstall(
    const Extension* extension) {
  if (SharedModuleInfo::ImportsModules(extension)) {
    const std::vector<SharedModuleInfo::ImportInfo>& imports =
        SharedModuleInfo::GetImports(extension);
    std::vector<SharedModuleInfo::ImportInfo>::const_iterator i;
    for (i = imports.begin(); i != imports.end(); ++i) {
      const Extension* imported_module =
          GetExtensionById(i->extension_id, true);
      if (imported_module && imported_module->from_webstore()) {
        scoped_ptr<const ExtensionSet> dependents =
            GetDependentExtensions(imported_module);
        if (dependents->size() == 0) {
          UninstallExtension(i->extension_id, true, NULL);
        }
      }
    }
  }
}

void ExtensionService::OnExtensionInstalled(
    const Extension* extension,
    const syncer::StringOrdinal& page_ordinal,
    bool has_requirement_errors,
    extensions::Blacklist::BlacklistState blacklist_state,
    bool wait_for_idle) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const std::string& id = extension->id();
  bool initial_enable = ShouldEnableOnInstall(extension);
  const extensions::PendingExtensionInfo* pending_extension_info = NULL;
  if ((pending_extension_info = pending_extension_manager()->GetById(id))) {
    if (!pending_extension_info->ShouldAllowInstall(extension)) {
      pending_extension_manager()->Remove(id);

      LOG(WARNING) << "ShouldAllowInstall() returned false for "
                   << id << " of type " << extension->GetType()
                   << " and update URL "
                   << extensions::ManifestURL::GetUpdateURL(extension).spec()
                   << "; not installing";

      // Delete the extension directory since we're not going to
      // load it.
      if (!GetFileTaskRunner()->PostTask(
              FROM_HERE,
              base::Bind(&extension_file_util::DeleteFile,
                         extension->path(), true))) {
        NOTREACHED();
      }
      return;
    }

    pending_extension_manager()->Remove(id);
  } else {
    // We explicitly want to re-enable an uninstalled external
    // extension; if we're here, that means the user is manually
    // installing the extension.
    if (IsExternalExtensionUninstalled(id)) {
      initial_enable = true;
    }
  }

  // Unsupported requirements overrides the management policy.
  if (has_requirement_errors) {
    initial_enable = false;
    extension_prefs_->AddDisableReason(
        id, Extension::DISABLE_UNSUPPORTED_REQUIREMENT);
  // If the extension was disabled because of unsupported requirements but
  // now supports all requirements after an update and there are not other
  // disable reasons, enable it.
  } else if (extension_prefs_->GetDisableReasons(id) ==
      Extension::DISABLE_UNSUPPORTED_REQUIREMENT) {
    initial_enable = true;
    extension_prefs_->ClearDisableReasons(id);
  }

  if (blacklist_state == extensions::Blacklist::BLACKLISTED) {
    // Installation of a blacklisted extension can happen from sync, policy,
    // etc, where to maintain consistency we need to install it, just never
    // load it (see AddExtension). Usually it should be the job of callers to
    // incercept blacklisted extension earlier (e.g. CrxInstaller, before even
    // showing the install dialogue).
    extension_prefs()->AcknowledgeBlacklistedExtension(id);
    UMA_HISTOGRAM_ENUMERATION("ExtensionBlacklist.SilentInstall",
                              extension->location(),
                              Manifest::NUM_LOCATIONS);
  }

  if (!GetInstalledExtension(extension->id())) {
    UMA_HISTOGRAM_ENUMERATION("Extensions.InstallType",
                              extension->GetType(), 100);
    UMA_HISTOGRAM_ENUMERATION("Extensions.InstallSource",
                              extension->location(), Manifest::NUM_LOCATIONS);
    RecordPermissionMessagesHistogram(
        extension, "Extensions.Permissions_Install");
  } else {
    UMA_HISTOGRAM_ENUMERATION("Extensions.UpdateType",
                              extension->GetType(), 100);
    UMA_HISTOGRAM_ENUMERATION("Extensions.UpdateSource",
                              extension->location(), Manifest::NUM_LOCATIONS);
  }

  // Certain extension locations are specific enough that we can
  // auto-acknowledge any extension that came from one of them.
  if (extension->location() == Manifest::EXTERNAL_POLICY_DOWNLOAD)
    AcknowledgeExternalExtension(extension->id());
  const Extension::State initial_state =
      initial_enable ? Extension::ENABLED : Extension::DISABLED;
  if (ShouldDelayExtensionUpdate(id, wait_for_idle)) {
    extension_prefs_->SetDelayedInstallInfo(
        extension,
        initial_state,
        blacklist_state,
        extensions::ExtensionPrefs::DELAY_REASON_WAIT_FOR_IDLE,
        page_ordinal);

    // Transfer ownership of |extension|.
    delayed_installs_.Insert(extension);

    // Notify extension of available update.
    extensions::RuntimeEventRouter::DispatchOnUpdateAvailableEvent(
        profile_, id, extension->manifest()->value());

    // Notify observers that app update is available.
    FOR_EACH_OBSERVER(extensions::UpdateObserver, update_observers_,
                      OnAppUpdateAvailable(extension->id()));
    return;
  }

  ImportStatus status = SatisfyImports(extension);
  if (installs_delayed_for_gc()) {
    extension_prefs_->SetDelayedInstallInfo(
        extension,
        initial_state,
        blacklist_state,
        extensions::ExtensionPrefs::DELAY_REASON_GC,
        page_ordinal);
    delayed_installs_.Insert(extension);
  } else if (status != IMPORT_STATUS_OK) {
    if (status == IMPORT_STATUS_UNSATISFIED) {
      extension_prefs_->SetDelayedInstallInfo(
          extension,
          initial_state,
          blacklist_state,
          extensions::ExtensionPrefs::DELAY_REASON_WAIT_FOR_IMPORTS,
          page_ordinal);
      delayed_installs_.Insert(extension);
    }
  } else {
    AddNewOrUpdatedExtension(extension,
                             initial_state,
                             blacklist_state,
                             page_ordinal);
  }
}

void ExtensionService::AddNewOrUpdatedExtension(
    const Extension* extension,
    Extension::State initial_state,
    extensions::Blacklist::BlacklistState blacklist_state,
    const syncer::StringOrdinal& page_ordinal) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  extension_prefs_->OnExtensionInstalled(extension,
                                         initial_state,
                                         blacklist_state,
                                         page_ordinal);
  delayed_installs_.Remove(extension->id());
  FinishInstallation(extension);
}

void ExtensionService::MaybeFinishDelayedInstallation(
    const std::string& extension_id) {
  // Check if the extension already got installed.
  if (!delayed_installs_.Contains(extension_id))
    return;
  extensions::ExtensionPrefs::DelayReason reason =
      extension_prefs_->GetDelayedInstallReason(extension_id);

  // Check if the extension is idle. DELAY_REASON_NONE is used for older
  // preferences files that will not have set this field but it was previously
  // only used for idle updates.
  if ((reason == extensions::ExtensionPrefs::DELAY_REASON_WAIT_FOR_IDLE ||
       reason == extensions::ExtensionPrefs::DELAY_REASON_NONE) &&
       is_ready() && !IsExtensionIdle(extension_id))
    return;

  const Extension* extension = delayed_installs_.GetByID(extension_id);
  if (reason == extensions::ExtensionPrefs::DELAY_REASON_WAIT_FOR_IMPORTS) {
    ImportStatus status = SatisfyImports(extension);
    if (status != IMPORT_STATUS_OK) {
      if (status == IMPORT_STATUS_UNRECOVERABLE) {
        delayed_installs_.Remove(extension_id);
        // Make sure no version of the extension is actually installed, (i.e.,
        // that this delayed install was not an update).
        CHECK(!extension_prefs_->GetInstalledExtensionInfo(extension_id).get());
        extension_prefs_->DeleteExtensionPrefs(extension_id);
      }
      return;
    }
  }

  FinishDelayedInstallation(extension_id);
}

void ExtensionService::FinishDelayedInstallation(
    const std::string& extension_id) {
  scoped_refptr<const Extension> extension(
      GetPendingExtensionUpdate(extension_id));
  CHECK(extension.get());
  delayed_installs_.Remove(extension_id);

  if (!extension_prefs_->FinishDelayedInstallInfo(extension_id))
    NOTREACHED();

  FinishInstallation(extension.get());
}

void ExtensionService::FinishInstallation(const Extension* extension) {
  const extensions::Extension* existing_extension =
      GetInstalledExtension(extension->id());
  bool is_update = false;
  std::string old_name;
  if (existing_extension) {
    is_update = true;
    old_name = existing_extension->name();
  }
  extensions::InstalledExtensionInfo details(extension, is_update, old_name);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_INSTALLED,
      content::Source<Profile>(profile_),
      content::Details<const extensions::InstalledExtensionInfo>(&details));

  bool unacknowledged_external = IsUnacknowledgedExternalExtension(extension);

  // Unpacked extensions default to allowing file access, but if that has been
  // overridden, don't reset the value.
  if (Manifest::ShouldAlwaysAllowFileAccess(extension->location()) &&
      !extension_prefs_->HasAllowFileAccessSetting(extension->id())) {
    extension_prefs_->SetAllowFileAccess(extension->id(), true);
  }

  AddExtension(extension);

  // If this is a new external extension that was disabled, alert the user
  // so he can reenable it. We do this last so that it has already been
  // added to our list of extensions.
  if (unacknowledged_external && !is_update) {
    UpdateExternalExtensionAlert();
    UMA_HISTOGRAM_ENUMERATION("Extensions.ExternalExtensionEvent",
                              EXTERNAL_EXTENSION_INSTALLED,
                              EXTERNAL_EXTENSION_BUCKET_BOUNDARY);
    if (extensions::ManifestURL::UpdatesFromGallery(extension)) {
      UMA_HISTOGRAM_ENUMERATION("Extensions.ExternalExtensionEventWebstore",
                                EXTERNAL_EXTENSION_INSTALLED,
                                EXTERNAL_EXTENSION_BUCKET_BOUNDARY);
    } else {
      UMA_HISTOGRAM_ENUMERATION("Extensions.ExternalExtensionEventNonWebstore",
                                EXTERNAL_EXTENSION_INSTALLED,
                                EXTERNAL_EXTENSION_BUCKET_BOUNDARY);
    }
  }

  // Check extensions that may have been delayed only because this shared module
  // was not available.
  if (SharedModuleInfo::IsSharedModule(extension)) {
    MaybeFinishDelayedInstallations();
  }
}

const Extension* ExtensionService::GetPendingExtensionUpdate(
    const std::string& id) const {
  return delayed_installs_.GetByID(id);
}

void ExtensionService::TrackTerminatedExtension(const Extension* extension) {
  if (!terminated_extensions_.Contains(extension->id()))
    terminated_extensions_.Insert(make_scoped_refptr(extension));

  UnloadExtension(extension->id(), extension_misc::UNLOAD_REASON_TERMINATE);
}

void ExtensionService::UntrackTerminatedExtension(const std::string& id) {
  std::string lowercase_id = StringToLowerASCII(id);
  const Extension* extension = terminated_extensions_.GetByID(lowercase_id);
  terminated_extensions_.Remove(lowercase_id);
  if (extension) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_REMOVED,
        content::Source<Profile>(profile_),
        content::Details<const Extension>(extension));
  }
}

const Extension* ExtensionService::GetTerminatedExtension(
    const std::string& id) const {
  return GetExtensionById(id, INCLUDE_TERMINATED);
}

const Extension* ExtensionService::GetInstalledExtension(
    const std::string& id) const {
  int include_mask = INCLUDE_ENABLED |
                     INCLUDE_DISABLED |
                     INCLUDE_TERMINATED |
                     INCLUDE_BLACKLISTED;
  return GetExtensionById(id, include_mask);
}

bool ExtensionService::ExtensionBindingsAllowed(const GURL& url) {
  // Allow bindings for all packaged extensions and component hosted apps.
  const Extension* extension = extensions_.GetExtensionOrAppByURL(url);
  return extension && (!extension->is_hosted_app() ||
                       extension->location() == Manifest::COMPONENT);
}

bool ExtensionService::ShouldBlockUrlInBrowserTab(GURL* url) {
  const Extension* extension = extensions_.GetExtensionOrAppByURL(*url);
  if (extension && extension->is_platform_app()) {
    *url = GURL(chrome::kExtensionInvalidRequestURL);
    return true;
  }

  return false;
}

bool ExtensionService::OnExternalExtensionFileFound(
         const std::string& id,
         const Version* version,
         const base::FilePath& path,
         Manifest::Location location,
         int creation_flags,
         bool mark_acknowledged) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(Extension::IdIsValid(id));
  if (extension_prefs_->IsExternalExtensionUninstalled(id))
    return false;

  // Before even bothering to unpack, check and see if we already have this
  // version. This is important because these extensions are going to get
  // installed on every startup.
  const Extension* existing = GetExtensionById(id, true);

  if (existing) {
    // The default apps will have the location set as INTERNAL. Since older
    // default apps are installed as EXTERNAL, we override them. However, if the
    // app is already installed as internal, then do the version check.
    // TODO(grv) : Remove after Q1-2013.
    bool is_default_apps_migration =
        (location == Manifest::INTERNAL &&
         Manifest::IsExternalLocation(existing->location()));

    if (!is_default_apps_migration) {
      DCHECK(version);

      switch (existing->version()->CompareTo(*version)) {
        case -1:  // existing version is older, we should upgrade
          break;
        case 0:  // existing version is same, do nothing
          return false;
        case 1:  // existing version is newer, uh-oh
          LOG(WARNING) << "Found external version of extension " << id
                       << "that is older than current version. Current version "
                       << "is: " << existing->VersionString() << ". New "
                       << "version is: " << version->GetString()
                       << ". Keeping current version.";
          return false;
      }
    }
  }

  // If the extension is already pending, don't start an install.
  if (!pending_extension_manager()->AddFromExternalFile(
          id, location, *version)) {
    return false;
  }

  // no client (silent install)
  scoped_refptr<CrxInstaller> installer(CrxInstaller::CreateSilent(this));
  installer->set_install_source(location);
  installer->set_expected_id(id);
  installer->set_expected_version(*version);
  installer->set_install_cause(extension_misc::INSTALL_CAUSE_EXTERNAL_FILE);
  installer->set_creation_flags(creation_flags);
#if defined(OS_CHROMEOS)
  extensions::InstallLimiter::Get(profile_)->Add(installer, path);
#else
  installer->InstallCrx(path);
#endif

  // Depending on the source, a new external extension might not need a user
  // notification on installation. For such extensions, mark them acknowledged
  // now to suppress the notification.
  if (mark_acknowledged)
    AcknowledgeExternalExtension(id);

  return true;
}

void ExtensionService::ReportExtensionLoadError(
    const base::FilePath& extension_path,
    const std::string &error,
    bool be_noisy) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_LOAD_ERROR,
      content::Source<Profile>(profile_),
      content::Details<const std::string>(&error));

  std::string path_str = UTF16ToUTF8(extension_path.LossyDisplayName());
  string16 message = UTF8ToUTF16(base::StringPrintf(
      "Could not load extension from '%s'. %s",
      path_str.c_str(), error.c_str()));
  ExtensionErrorReporter::GetInstance()->ReportError(message, be_noisy);
}

void ExtensionService::DidCreateRenderViewForBackgroundPage(
    extensions::ExtensionHost* host) {
  OrphanedDevTools::iterator iter =
      orphaned_dev_tools_.find(host->extension_id());
  if (iter == orphaned_dev_tools_.end())
    return;

  iter->second->ConnectRenderViewHost(host->render_view_host());
  orphaned_dev_tools_.erase(iter);
}

void ExtensionService::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_APP_TERMINATING:
      // Shutdown has started. Don't start any more extension installs.
      // (We cannot use ExtensionService::Shutdown() for this because it
      // happens too late in browser teardown.)
      browser_terminating_ = true;
      break;
    case chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED: {
      if (profile_ !=
          content::Source<Profile>(source).ptr()->GetOriginalProfile()) {
        break;
      }

      extensions::ExtensionHost* host =
          content::Details<extensions::ExtensionHost>(details).ptr();

      // Mark the extension as terminated and Unload it. We want it to
      // be in a consistent state: either fully working or not loaded
      // at all, but never half-crashed.  We do it in a PostTask so
      // that other handlers of this notification will still have
      // access to the Extension and ExtensionHost.
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(
              &ExtensionService::TrackTerminatedExtension,
              AsWeakPtr(),
              host->extension()));
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_CREATED: {
      content::RenderProcessHost* process =
          content::Source<content::RenderProcessHost>(source).ptr();
      Profile* host_profile =
          Profile::FromBrowserContext(process->GetBrowserContext());
      if (!profile_->IsSameProfile(host_profile->GetOriginalProfile()))
          break;

      // Extensions need to know the channel for API restrictions.
      process->Send(new ExtensionMsg_SetChannel(
          extensions::GetCurrentChannel()));

      // Platform apps need to know the system font.
      scoped_ptr<base::DictionaryValue> fonts(new base::DictionaryValue);
      webui::SetFontAndTextDirection(fonts.get());
      std::string font_family, font_size;
      fonts->GetString("fontfamily", &font_family);
      fonts->GetString("fontsize", &font_size);
      process->Send(new ExtensionMsg_SetSystemFont(
          font_family, font_size));

      // Valid extension function names, used to setup bindings in renderer.
      std::vector<std::string> function_names;
      ExtensionFunctionDispatcher::GetAllFunctionNames(&function_names);
      process->Send(new ExtensionMsg_SetFunctionNames(function_names));

      // Scripting whitelist. This is modified by tests and must be communicated
      // to renderers.
      process->Send(new ExtensionMsg_SetScriptingWhitelist(
          *Extension::GetScriptingWhitelist()));

      // Loaded extensions.
      std::vector<ExtensionMsg_Loaded_Params> loaded_extensions;
      for (ExtensionSet::const_iterator iter = extensions_.begin();
           iter != extensions_.end(); ++iter) {
        // Renderers don't need to know about themes.
        if (!(*iter)->is_theme())
          loaded_extensions.push_back(ExtensionMsg_Loaded_Params(iter->get()));
      }
      process->Send(new ExtensionMsg_Loaded(loaded_extensions));
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED: {
      content::RenderProcessHost* process =
          content::Source<content::RenderProcessHost>(source).ptr();
      Profile* host_profile =
          Profile::FromBrowserContext(process->GetBrowserContext());
      if (!profile_->IsSameProfile(host_profile->GetOriginalProfile()))
          break;

      process_map_.RemoveAllFromProcess(process->GetID());
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&ExtensionInfoMap::UnregisterAllExtensionsInProcess,
                     system_->info_map(),
                     process->GetID()));
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED: {
      extensions::ExtensionHost* host =
          content::Details<extensions::ExtensionHost>(details).ptr();
      std::string extension_id = host->extension_id();
      if (delayed_installs_.Contains(extension_id)) {
        // We were waiting for this extension to become idle, it now might have,
        // so maybe finish installation.
        base::MessageLoop::current()->PostDelayedTask(
            FROM_HERE,
            base::Bind(&ExtensionService::MaybeFinishDelayedInstallation,
                       AsWeakPtr(), extension_id),
            base::TimeDelta::FromSeconds(kUpdateIdleDelay));
      }
      break;
    }
    case chrome::NOTIFICATION_UPGRADE_RECOMMENDED: {
      // Notify extensions that chrome update is available.
      extensions::RuntimeEventRouter::DispatchOnBrowserUpdateAvailableEvent(
          profile_);

      // Notify observers that chrome update is available.
      FOR_EACH_OBSERVER(extensions::UpdateObserver, update_observers_,
                        OnChromeUpdateAvailable());
      break;
    }

    default:
      NOTREACHED() << "Unexpected notification type.";
  }
}

void ExtensionService::OnExtensionInstallPrefChanged() {
  IdentifyAlertableExtensions();
  CheckManagementPolicy();
}

bool ExtensionService::HasApps() const {
  return !GetAppIds().empty();
}

ExtensionIdSet ExtensionService::GetAppIds() const {
  ExtensionIdSet result;
  for (ExtensionSet::const_iterator it = extensions_.begin();
       it != extensions_.end(); ++it) {
    if ((*it)->is_app() && (*it)->location() != Manifest::COMPONENT)
      result.insert((*it)->id());
  }

  return result;
}

bool ExtensionService::IsBackgroundPageReady(const Extension* extension) const {
  if (!extensions::BackgroundInfo::HasPersistentBackgroundPage(extension))
    return true;
  ExtensionRuntimeDataMap::const_iterator it =
      extension_runtime_data_.find(extension->id());
  return it == extension_runtime_data_.end() ? false :
                                               it->second.background_page_ready;
}

void ExtensionService::SetBackgroundPageReady(const Extension* extension) {
  DCHECK(extensions::BackgroundInfo::HasBackgroundPage(extension));
  extension_runtime_data_[extension->id()].background_page_ready = true;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
      content::Source<const Extension>(extension),
      content::NotificationService::NoDetails());
}

bool ExtensionService::IsBeingUpgraded(const Extension* extension) const {
  ExtensionRuntimeDataMap::const_iterator it =
      extension_runtime_data_.find(extension->id());
  return it == extension_runtime_data_.end() ? false :
                                               it->second.being_upgraded;
}

void ExtensionService::SetBeingUpgraded(const Extension* extension,
                                        bool value) {
  extension_runtime_data_[extension->id()].being_upgraded = value;
}

bool ExtensionService::IsBeingReloaded(
    const std::string& extension_id) const {
  return ContainsKey(extensions_being_reloaded_, extension_id);
}

void ExtensionService::SetBeingReloaded(const std::string& extension_id,
                                        bool isBeingReloaded) {
  if (isBeingReloaded)
    extensions_being_reloaded_.insert(extension_id);
  else
    extensions_being_reloaded_.erase(extension_id);
}

bool ExtensionService::HasUsedWebRequest(const Extension* extension) const {
  ExtensionRuntimeDataMap::const_iterator it =
      extension_runtime_data_.find(extension->id());
  return it == extension_runtime_data_.end() ? false :
                                               it->second.has_used_webrequest;
}

void ExtensionService::SetHasUsedWebRequest(const Extension* extension,
                                            bool value) {
  extension_runtime_data_[extension->id()].has_used_webrequest = value;
}

bool ExtensionService::ShouldEnableOnInstall(const Extension* extension) {
  // Extensions installed by policy can't be disabled. So even if a previous
  // installation disabled the extension, make sure it is now enabled.
  if (system_->management_policy()->MustRemainEnabled(extension, NULL))
    return true;

  if (extension_prefs_->IsExtensionDisabled(extension->id()))
    return false;

  if (FeatureSwitch::prompt_for_external_extensions()->IsEnabled()) {
    // External extensions are initially disabled. We prompt the user before
    // enabling them. Hosted apps are excepted because they are not dangerous
    // (they need to be launched by the user anyway).
    if (extension->GetType() != Manifest::TYPE_HOSTED_APP &&
        Manifest::IsExternalLocation(extension->location()) &&
        !extension_prefs_->IsExternalExtensionAcknowledged(extension->id())) {
      return false;
    }
  }

  return true;
}

bool ExtensionService::IsExtensionIdle(const std::string& extension_id) const {
  ExtensionProcessManager* process_manager = system_->process_manager();
  DCHECK(process_manager);
  extensions::ExtensionHost* host =
      process_manager->GetBackgroundHostForExtension(extension_id);
  if (host)
    return false;
  return process_manager->GetRenderViewHostsForExtension(extension_id).empty();
}

bool ExtensionService::ShouldDelayExtensionUpdate(
    const std::string& extension_id,
    bool wait_for_idle) const {
  const char kOnUpdateAvailableEvent[] = "runtime.onUpdateAvailable";

  // If delayed updates are globally disabled, or just for this extension,
  // don't delay.
  if (!install_updates_when_idle_ || !wait_for_idle)
    return false;

  const Extension* old = GetInstalledExtension(extension_id);
  // If there is no old extension, this is not an update, so don't delay.
  if (!old)
    return false;

  if (extensions::BackgroundInfo::HasPersistentBackgroundPage(old)) {
    // Delay installation if the extension listens for the onUpdateAvailable
    // event.
    return system_->event_router()->ExtensionHasEventListener(
        extension_id, kOnUpdateAvailableEvent);
  } else {
    // Delay installation if the extension is not idle.
    return !IsExtensionIdle(extension_id);
  }
}

void ExtensionService::GarbageCollectIsolatedStorage() {
  scoped_ptr<base::hash_set<base::FilePath> > active_paths(
      new base::hash_set<base::FilePath>());
  for (ExtensionSet::const_iterator it = extensions_.begin();
       it != extensions_.end(); ++it) {
    if (extensions::AppIsolationInfo::HasIsolatedStorage(it->get())) {
      active_paths->insert(BrowserContext::GetStoragePartitionForSite(
          profile_, GetSiteForExtensionId((*it)->id()))->GetPath());
    }
  }

  DCHECK(!installs_delayed_for_gc());
  set_installs_delayed_for_gc(true);
  BrowserContext::GarbageCollectStoragePartitions(
      profile_, active_paths.Pass(),
      base::Bind(&ExtensionService::OnGarbageCollectIsolatedStorageFinished,
                 AsWeakPtr()));
}

void ExtensionService::OnGarbageCollectIsolatedStorageFinished() {
  set_installs_delayed_for_gc(false);
  MaybeFinishDelayedInstallations();
}

void ExtensionService::MaybeFinishDelayedInstallations() {
  std::vector<std::string> to_be_installed;
  for (ExtensionSet::const_iterator it = delayed_installs_.begin();
       it != delayed_installs_.end();
       ++it) {
    to_be_installed.push_back((*it)->id());
  }
  for (std::vector<std::string>::const_iterator it = to_be_installed.begin();
       it != to_be_installed.end();
       ++it) {
    MaybeFinishDelayedInstallation(*it);
  }
}

void ExtensionService::OnNeedsToGarbageCollectIsolatedStorage() {
  extension_prefs_->SetNeedsStorageGarbageCollection(true);
}

void ExtensionService::OnBlacklistUpdated() {
  blacklist_->GetBlacklistedIDs(
      GenerateInstalledExtensionsSet()->GetIDs(),
      base::Bind(&ExtensionService::ManageBlacklist, AsWeakPtr()));
}

void ExtensionService::ManageBlacklist(const std::set<std::string>& updated) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::set<std::string> before = blacklisted_extensions_.GetIDs();
  std::set<std::string> no_longer_blacklisted =
      base::STLSetDifference<std::set<std::string> >(before, updated);
  std::set<std::string> not_yet_blacklisted =
      base::STLSetDifference<std::set<std::string> >(updated, before);

  for (std::set<std::string>::iterator it = no_longer_blacklisted.begin();
       it != no_longer_blacklisted.end(); ++it) {
    scoped_refptr<const Extension> extension =
        blacklisted_extensions_.GetByID(*it);
    if (!extension.get()) {
      NOTREACHED() << "Extension " << *it << " no longer blacklisted, "
                   << "but it was never blacklisted.";
      continue;
    }
    blacklisted_extensions_.Remove(*it);
    extension_prefs_->SetExtensionBlacklisted(extension->id(), false);
    AddExtension(extension.get());
    UMA_HISTOGRAM_ENUMERATION("ExtensionBlacklist.UnblacklistInstalled",
                              extension->location(),
                              Manifest::NUM_LOCATIONS);
  }

  for (std::set<std::string>::iterator it = not_yet_blacklisted.begin();
       it != not_yet_blacklisted.end(); ++it) {
    scoped_refptr<const Extension> extension = GetInstalledExtension(*it);
    if (!extension.get()) {
      NOTREACHED() << "Extension " << *it << " needs to be "
                   << "blacklisted, but it's not installed.";
      continue;
    }
    blacklisted_extensions_.Insert(extension);
    extension_prefs_->SetExtensionBlacklisted(extension->id(), true);
    UnloadExtension(*it, extension_misc::UNLOAD_REASON_BLACKLIST);
    UMA_HISTOGRAM_ENUMERATION("ExtensionBlacklist.BlacklistInstalled",
                              extension->location(), Manifest::NUM_LOCATIONS);
  }

  IdentifyAlertableExtensions();
}

void ExtensionService::AddUpdateObserver(extensions::UpdateObserver* observer) {
  update_observers_.AddObserver(observer);
}

void ExtensionService::RemoveUpdateObserver(
    extensions::UpdateObserver* observer) {
  update_observers_.RemoveObserver(observer);
}
