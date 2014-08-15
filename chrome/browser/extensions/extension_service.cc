// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_service.h"

#include <algorithm>
#include <iterator>
#include <set>

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/data_deleter.h"
#include "chrome/browser/extensions/extension_assets_manager.h"
#include "chrome/browser/extensions/extension_disabled_ui.h"
#include "chrome/browser/extensions/extension_error_controller.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/external_install_manager.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/installed_loader.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/extension_cache.h"
#include "chrome/browser/extensions/updater/extension_downloader.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_identity_provider.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/ntp/thumbnail_source.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/startup_metric_utils/startup_metric_utils.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/browser/update_observer.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/one_shot_event.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permissions_data.h"

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
using extensions::ExtensionDownloader;
using extensions::ExtensionDownloaderDelegate;
using extensions::ExtensionIdSet;
using extensions::ExtensionInfo;
using extensions::ExtensionRegistry;
using extensions::ExtensionSet;
using extensions::FeatureSwitch;
using extensions::InstallVerifier;
using extensions::ManagementPolicy;
using extensions::Manifest;
using extensions::PermissionMessage;
using extensions::PermissionMessages;
using extensions::PermissionSet;
using extensions::SharedModuleInfo;
using extensions::SharedModuleService;
using extensions::UnloadedExtensionInfo;

namespace errors = extensions::manifest_errors;

namespace {

// Wait this many seconds after an extensions becomes idle before updating it.
const int kUpdateIdleDelay = 5;

#if defined(ENABLE_EXTENSIONS)
scoped_ptr<IdentityProvider> CreateWebstoreIdentityProvider(Profile* profile) {
  return make_scoped_ptr<IdentityProvider>(new ProfileIdentityProvider(
      SigninManagerFactory::GetForProfile(profile),
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      LoginUIServiceFactory::GetForProfile(profile)));
}
#endif  // defined(ENABLE_EXTENSIONS)

}  // namespace

// ExtensionService.

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
  UninstallExtension(id,
                     extensions::UNINSTALL_REASON_ORPHANED_EXTERNAL_EXTENSION,
                     base::Bind(&base::DoNothing),
                     NULL);
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

void ExtensionService::BlacklistExtensionForTest(
    const std::string& extension_id) {
  ExtensionIdSet blocked;
  ExtensionIdSet unchanged;
  blocked.insert(extension_id);
  UpdateBlockedExtensions(blocked, unchanged);
}

bool ExtensionService::OnExternalExtensionUpdateUrlFound(
    const std::string& id,
    const std::string& install_parameter,
    const GURL& update_url,
    Manifest::Location location,
    int creation_flags,
    bool mark_acknowledged) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(Extension::IdIsValid(id));

  if (Manifest::IsExternalLocation(location)) {
    // All extensions that are not user specific can be cached.
    extensions::ExtensionCache::GetInstance()->AllowCaching(id);
  }

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
          id,
          install_parameter,
          update_url,
          location,
          creation_flags,
          mark_acknowledged)) {
    return false;
  }

  update_once_all_providers_are_ready_ = true;
  return true;
}

// static
// This function is used to uninstall an extension via sync.  The LOG statements
// within this function are used to inform the user if the uninstall cannot be
// done.
bool ExtensionService::UninstallExtensionHelper(
    ExtensionService* extensions_service,
    const std::string& extension_id,
    extensions::UninstallReason reason) {
  // We can't call UninstallExtension with an invalid extension ID.
  if (!extensions_service->GetInstalledExtension(extension_id)) {
    LOG(WARNING) << "Attempted uninstallation of non-existent extension with "
                 << "id: " << extension_id;
    return false;
  }

  // The following call to UninstallExtension will not allow an uninstall of a
  // policy-controlled extension.
  base::string16 error;
  if (!extensions_service->UninstallExtension(
          extension_id, reason, base::Bind(&base::DoNothing), &error)) {
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
      extension_sync_service_(NULL),
      registry_(extensions::ExtensionRegistry::Get(profile)),
      pending_extension_manager_(profile),
      install_directory_(install_directory),
      extensions_enabled_(extensions_enabled),
      show_extensions_prompts_(true),
      install_updates_when_idle_(true),
      ready_(ready),
      update_once_all_providers_are_ready_(false),
      browser_terminating_(false),
      installs_delayed_for_gc_(false),
      is_first_run_(false),
      shared_module_service_(new extensions::SharedModuleService(profile_)) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Figure out if extension installation should be enabled.
  if (extensions::ExtensionsBrowserClient::Get()->AreExtensionsDisabled(
          *command_line, profile))
    extensions_enabled_ = false;

  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_UPGRADE_RECOMMENDED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_DESTRUCTION_STARTED,
                 content::Source<Profile>(profile_));
  pref_change_registrar_.Init(profile->GetPrefs());
  base::Closure callback =
      base::Bind(&ExtensionService::OnExtensionInstallPrefChanged,
                 base::Unretained(this));
  pref_change_registrar_.Add(extensions::pref_names::kInstallAllowList,
                             callback);
  pref_change_registrar_.Add(extensions::pref_names::kInstallDenyList,
                             callback);
  pref_change_registrar_.Add(extensions::pref_names::kAllowedTypes, callback);

  // Set up the ExtensionUpdater
  if (autoupdate_enabled) {
    int update_frequency = extensions::kDefaultUpdateFrequencySeconds;
    if (command_line->HasSwitch(switches::kExtensionsUpdateFrequency)) {
      base::StringToInt(command_line->GetSwitchValueASCII(
          switches::kExtensionsUpdateFrequency),
          &update_frequency);
    }
    updater_.reset(new extensions::ExtensionUpdater(
        this,
        extension_prefs,
        profile->GetPrefs(),
        profile,
        update_frequency,
        extensions::ExtensionCache::GetInstance(),
        base::Bind(&ExtensionService::CreateExtensionDownloader,
                   base::Unretained(this))));
  }

  component_loader_.reset(
      new extensions::ComponentLoader(this,
                                      profile->GetPrefs(),
                                      g_browser_process->local_state(),
                                      profile));

  if (extensions_enabled_) {
    extensions::ExternalProviderImpl::CreateExternalProviders(
        this, profile_, &external_extension_providers_);
  }

  // Set this as the ExtensionService for app sorting to ensure it causes syncs
  // if required.
  is_first_run_ = !extension_prefs_->SetAlertSystemFirstRun();

  error_controller_.reset(
      new extensions::ExtensionErrorController(profile_, is_first_run_));
  external_install_manager_.reset(
      new extensions::ExternalInstallManager(profile_, is_first_run_));

  extension_action_storage_manager_.reset(
      new extensions::ExtensionActionStorageManager(profile_));

  // How long is the path to the Extensions directory?
  UMA_HISTOGRAM_CUSTOM_COUNTS("Extensions.ExtensionRootPathLength",
                              install_directory_.value().length(), 0, 500, 100);
}

const ExtensionSet* ExtensionService::extensions() const {
  return &registry_->enabled_extensions();
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

void ExtensionService::Shutdown() {
  system_->management_policy()->UnregisterProvider(
      shared_module_policy_provider_.get());
}

const Extension* ExtensionService::GetExtensionById(
    const std::string& id, bool include_disabled) const {
  int include_mask = ExtensionRegistry::ENABLED;
  if (include_disabled) {
    // Include blacklisted extensions here because there are hundreds of
    // callers of this function, and many might assume that this includes those
    // that have been disabled due to blacklisting.
    include_mask |= ExtensionRegistry::DISABLED |
                    ExtensionRegistry::BLACKLISTED;
  }
  return registry_->GetExtensionById(id, include_mask);
}

void ExtensionService::Init() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::Time begin_time = base::Time::Now();

  DCHECK(!is_ready());  // Can't redo init.
  DCHECK_EQ(registry_->enabled_extensions().size(), 0u);

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
    const ExtensionSet& disabled_extensions = registry_->disabled_extensions();
    for (ExtensionSet::const_iterator iter = disabled_extensions.begin();
        iter != disabled_extensions.end(); ++iter) {
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

    LoadGreylistFromPrefs();
  }

  UMA_HISTOGRAM_TIMES("Extensions.ExtensionServiceInitTime",
                      base::Time::Now() - begin_time);
}

void ExtensionService::LoadGreylistFromPrefs() {
  scoped_ptr<ExtensionSet> all_extensions =
      registry_->GenerateInstalledExtensionsSet();

  for (ExtensionSet::const_iterator it = all_extensions->begin();
       it != all_extensions->end(); ++it) {
    extensions::BlacklistState state =
        extension_prefs_->GetExtensionBlacklistState((*it)->id());
    if (state == extensions::BLACKLISTED_SECURITY_VULNERABILITY ||
        state == extensions::BLACKLISTED_POTENTIALLY_UNWANTED ||
        state == extensions::BLACKLISTED_CWS_POLICY_VIOLATION)
      greylist_.Insert(*it);
  }
}

bool ExtensionService::UpdateExtension(const std::string& id,
                                       const base::FilePath& extension_path,
                                       bool file_ownership_passed,
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
                &extensions::file_util::DeleteFile, extension_path, false)))
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
  int creation_flags = Extension::NO_FLAGS;
  if (pending_extension_info) {
    installer->set_install_source(pending_extension_info->install_source());
    if (pending_extension_info->install_silently())
      installer->set_allow_silent_install(true);
    if (pending_extension_info->remote_install())
      installer->set_grant_permissions(false);
    creation_flags = pending_extension_info->creation_flags();
    if (pending_extension_info->mark_acknowledged())
      external_install_manager_->AcknowledgeExternalExtension(id);
  } else if (extension) {
    installer->set_install_source(extension->location());
  }
  // If the extension was installed from or has migrated to the webstore, or
  // its auto-update URL is from the webstore, treat it as a webstore install.
  // Note that we ignore some older extensions with blank auto-update URLs
  // because we are mostly concerned with restrictions on NaCl extensions,
  // which are newer.
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

  if (extension && extension->was_installed_by_oem())
    creation_flags |= Extension::WAS_INSTALLED_BY_OEM;

  if (extension && extension->was_installed_by_custodian())
    creation_flags |= Extension::WAS_INSTALLED_BY_CUSTODIAN;

  if (extension) {
    installer->set_is_ephemeral(extension_prefs_->IsEphemeralApp(id));
    installer->set_do_not_sync(extension_prefs_->DoNotSync(id));
  }

  installer->set_creation_flags(creation_flags);

  installer->set_delete_source(file_ownership_passed);
  installer->set_install_cause(extension_misc::INSTALL_CAUSE_UPDATE);
  installer->InstallCrx(extension_path);

  if (out_crx_installer)
    *out_crx_installer = installer.get();

  return true;
}

scoped_ptr<ExtensionDownloader> ExtensionService::CreateExtensionDownloader(
    ExtensionDownloaderDelegate* delegate) {
  scoped_ptr<ExtensionDownloader> downloader;
#if defined(ENABLE_EXTENSIONS)
  scoped_ptr<IdentityProvider> identity_provider =
      CreateWebstoreIdentityProvider(profile_);
  downloader.reset(new ExtensionDownloader(
      delegate,
      profile_->GetRequestContext()));
  downloader->SetWebstoreIdentityProvider(identity_provider.Pass());
#endif
  return downloader.Pass();
}

void ExtensionService::ReloadExtensionImpl(
    // "transient" because the process of reloading may cause the reference
    // to become invalid. Instead, use |extension_id|, a copy.
    const std::string& transient_extension_id,
    bool be_noisy) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If the extension is already reloading, don't reload again.
  if (extension_prefs_->GetDisableReasons(transient_extension_id) &
      Extension::DISABLE_RELOAD) {
    return;
  }

  // Ignore attempts to reload a blacklisted extension. Sometimes this can
  // happen in a convoluted reload sequence triggered by the termination of a
  // blacklisted extension and a naive attempt to reload it. For an example see
  // http://crbug.com/373842.
  if (registry_->blacklisted_extensions().Contains(transient_extension_id))
    return;

  base::FilePath path;

  std::string extension_id = transient_extension_id;
  const Extension* transient_current_extension =
      GetExtensionById(extension_id, false);

  // Disable the extension if it's loaded. It might not be loaded if it crashed.
  if (transient_current_extension) {
    // If the extension has an inspector open for its background page, detach
    // the inspector and hang onto a cookie for it, so that we can reattach
    // later.
    // TODO(yoz): this is not incognito-safe!
    extensions::ProcessManager* manager = system_->process_manager();
    extensions::ExtensionHost* host =
        manager->GetBackgroundHostForExtension(extension_id);
    if (host && DevToolsAgentHost::HasFor(host->host_contents())) {
      // Look for an open inspector for the background page.
      scoped_refptr<DevToolsAgentHost> agent_host =
          DevToolsAgentHost::GetOrCreateFor(host->host_contents());
      agent_host->DisconnectWebContents();
      orphaned_dev_tools_[extension_id] = agent_host;
    }

    path = transient_current_extension->path();
    // BeingUpgraded is set back to false when the extension is added.
    system_->runtime_data()->SetBeingUpgraded(transient_current_extension,
                                              true);
    DisableExtension(extension_id, Extension::DISABLE_RELOAD);
    reloading_extensions_.insert(extension_id);
  } else {
    std::map<std::string, base::FilePath>::const_iterator iter =
        unloaded_extension_paths_.find(extension_id);
    if (iter == unloaded_extension_paths_.end()) {
      return;
    }
    path = unloaded_extension_paths_[extension_id];
  }

  transient_current_extension = NULL;

  if (delayed_installs_.Contains(extension_id)) {
    FinishDelayedInstallation(extension_id);
    return;
  }

  // If we're reloading a component extension, use the component extension
  // loader's reloader.
  if (component_loader_->Exists(extension_id)) {
    component_loader_->Reload(extension_id);
    return;
  }

  // Check the installed extensions to see if what we're reloading was already
  // installed.
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
    scoped_refptr<extensions::UnpackedInstaller> unpacked_installer =
        extensions::UnpackedInstaller::Create(this);
    unpacked_installer->set_be_noisy_on_failure(be_noisy);
    unpacked_installer->Load(path);
  }
}

void ExtensionService::ReloadExtension(const std::string& extension_id) {
  ReloadExtensionImpl(extension_id, true); // be_noisy
}

void ExtensionService::ReloadExtensionWithQuietFailure(
    const std::string& extension_id) {
  ReloadExtensionImpl(extension_id, false); // be_noisy
}

bool ExtensionService::UninstallExtension(
    // "transient" because the process of uninstalling may cause the reference
    // to become invalid. Instead, use |extenson->id()|.
    const std::string& transient_extension_id,
    extensions::UninstallReason reason,
    const base::Closure& deletion_done_callback,
    base::string16* error) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_refptr<const Extension> extension =
      GetInstalledExtension(transient_extension_id);

  // Callers should not send us nonexistent extensions.
  CHECK(extension.get());

  // Policy change which triggers an uninstall will always set
  // |external_uninstall| to true so this is the only way to uninstall
  // managed extensions.
  // Shared modules being uninstalled will also set |external_uninstall| to true
  // so that we can guarantee users don't uninstall a shared module.
  // (crbug.com/273300)
  // TODO(rdevlin.cronin): This is probably not right. We should do something
  // else, like include an enum IS_INTERNAL_UNINSTALL or IS_USER_UNINSTALL so
  // we don't do this.
  bool external_uninstall =
      (reason == extensions::UNINSTALL_REASON_INTERNAL_MANAGEMENT) ||
      (reason == extensions::UNINSTALL_REASON_ORPHANED_EXTERNAL_EXTENSION) ||
      (reason == extensions::UNINSTALL_REASON_ORPHANED_SHARED_MODULE) ||
      (reason == extensions::UNINSTALL_REASON_SYNC &&
           extension->was_installed_by_custodian());
  if (!external_uninstall &&
      !system_->management_policy()->UserMayModifySettings(
        extension.get(), error)) {
    content::NotificationService::current()->Notify(
        extensions::NOTIFICATION_EXTENSION_UNINSTALL_NOT_ALLOWED,
        content::Source<Profile>(profile_),
        content::Details<const Extension>(extension.get()));
    return false;
  }

  syncer::SyncChange sync_change;
  if (extension_sync_service_) {
     sync_change = extension_sync_service_->PrepareToSyncUninstallExtension(
        extension.get(), is_ready());
  }

  system_->install_verifier()->Remove(extension->id());

  UMA_HISTOGRAM_ENUMERATION("Extensions.UninstallType",
                            extension->GetType(), 100);
  RecordPermissionMessagesHistogram(extension.get(),
                                    "Extensions.Permissions_Uninstall2");

  // Unload before doing more cleanup to ensure that nothing is hanging on to
  // any of these resources.
  UnloadExtension(extension->id(), UnloadedExtensionInfo::REASON_UNINSTALL);

  // Tell the backend to start deleting installed extensions on the file thread.
  if (!Manifest::IsUnpackedLocation(extension->location())) {
    if (!GetFileTaskRunner()->PostTask(
            FROM_HERE,
            base::Bind(&ExtensionService::UninstallExtensionOnFileThread,
                       extension->id(),
                       profile_,
                       install_directory_,
                       extension->path())))
      NOTREACHED();
  }

  extensions::DataDeleter::StartDeleting(
      profile_, extension.get(), deletion_done_callback);

  UntrackTerminatedExtension(extension->id());

  // Notify interested parties that we've uninstalled this extension.
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_UNINSTALLED_DEPRECATED,
      content::Source<Profile>(profile_),
      content::Details<const Extension>(extension.get()));
  ExtensionRegistry::Get(profile_)
      ->TriggerOnUninstalled(extension.get(), reason);

  if (extension_sync_service_) {
    extension_sync_service_->ProcessSyncUninstallExtension(extension->id(),
                                                           sync_change);
  }

  delayed_installs_.Remove(extension->id());

  extension_prefs_->OnExtensionUninstalled(
      extension->id(), extension->location(), external_uninstall);

  // Track the uninstallation.
  UMA_HISTOGRAM_ENUMERATION("Extensions.ExtensionUninstalled", 1, 2);

  return true;
}

// static
void ExtensionService::UninstallExtensionOnFileThread(
    const std::string& id,
    Profile* profile,
    const base::FilePath& install_dir,
    const base::FilePath& extension_path) {
  extensions::ExtensionAssetsManager* assets_manager =
      extensions::ExtensionAssetsManager::GetInstance();
  assets_manager->UninstallExtension(id, profile, install_dir, extension_path);
}

bool ExtensionService::IsExtensionEnabled(
    const std::string& extension_id) const {
  if (registry_->enabled_extensions().Contains(extension_id) ||
      registry_->terminated_extensions().Contains(extension_id)) {
    return true;
  }

  if (registry_->disabled_extensions().Contains(extension_id) ||
      registry_->blacklisted_extensions().Contains(extension_id)) {
    return false;
  }

  // If the extension hasn't been loaded yet, check the prefs for it. Assume
  // enabled unless otherwise noted.
  return !extension_prefs_->IsExtensionDisabled(extension_id) &&
         !extension_prefs_->IsExtensionBlacklisted(extension_id) &&
         !extension_prefs_->IsExternalExtensionUninstalled(extension_id);
}

void ExtensionService::EnableExtension(const std::string& extension_id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (IsExtensionEnabled(extension_id))
    return;
  const Extension* extension =
      registry_->disabled_extensions().GetByID(extension_id);

  ManagementPolicy* policy = system_->management_policy();
  if (extension && policy->MustRemainDisabled(extension, NULL, NULL)) {
    UMA_HISTOGRAM_COUNTS_100("Extensions.EnableDeniedByPolicy", 1);
    return;
  }

  extension_prefs_->SetExtensionState(extension_id, Extension::ENABLED);
  extension_prefs_->ClearDisableReasons(extension_id);

  // This can happen if sync enables an extension that is not
  // installed yet.
  if (!extension)
    return;

  // Move it over to the enabled list.
  registry_->AddEnabled(make_scoped_refptr(extension));
  registry_->RemoveDisabled(extension->id());

  NotifyExtensionLoaded(extension);

  // Notify listeners that the extension was enabled.
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_ENABLED,
      content::Source<Profile>(profile_),
      content::Details<const Extension>(extension));

  if (extension_sync_service_)
    extension_sync_service_->SyncEnableExtension(*extension);
}

void ExtensionService::DisableExtension(
    const std::string& extension_id,
    Extension::DisableReason disable_reason) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // The extension may have been disabled already. Just add a disable reason.
  if (!IsExtensionEnabled(extension_id)) {
    extension_prefs_->AddDisableReason(extension_id, disable_reason);
    return;
  }

  const Extension* extension = GetInstalledExtension(extension_id);
  // |extension| can be NULL if sync disables an extension that is not
  // installed yet.
  // EXTERNAL_COMPONENT extensions are not generally modifiable by users, but
  // can be uninstalled by the browser if the user sets extension-specific
  // preferences.
  if (extension &&
      disable_reason != Extension::DISABLE_RELOAD &&
      !system_->management_policy()->UserMayModifySettings(extension, NULL) &&
      extension->location() != Manifest::EXTERNAL_COMPONENT) {
    return;
  }

  extension_prefs_->SetExtensionState(extension_id, Extension::DISABLED);
  extension_prefs_->AddDisableReason(extension_id, disable_reason);

  int include_mask =
      ExtensionRegistry::EVERYTHING & ~ExtensionRegistry::DISABLED;
  extension = registry_->GetExtensionById(extension_id, include_mask);
  if (!extension)
    return;

  // The extension is either enabled or terminated.
  DCHECK(registry_->enabled_extensions().Contains(extension->id()) ||
         registry_->terminated_extensions().Contains(extension->id()));

  // Move it over to the disabled list. Don't send a second unload notification
  // for terminated extensions being disabled.
  registry_->AddDisabled(make_scoped_refptr(extension));
  if (registry_->enabled_extensions().Contains(extension->id())) {
    registry_->RemoveEnabled(extension->id());
    NotifyExtensionUnloaded(extension, UnloadedExtensionInfo::REASON_DISABLE);
  } else {
    registry_->RemoveTerminated(extension->id());
  }

  if (extension_sync_service_)
    extension_sync_service_->SyncDisableExtension(*extension);
}

void ExtensionService::DisableUserExtensions(
    const std::vector<std::string>& except_ids) {
  extensions::ManagementPolicy* management_policy =
      system_->management_policy();
  extensions::ExtensionList to_disable;

  const ExtensionSet& enabled_set = registry_->enabled_extensions();
  for (ExtensionSet::const_iterator extension = enabled_set.begin();
      extension != enabled_set.end(); ++extension) {
    if (management_policy->UserMayModifySettings(extension->get(), NULL))
      to_disable.push_back(*extension);
  }
  const ExtensionSet& terminated_set = registry_->terminated_extensions();
  for (ExtensionSet::const_iterator extension = terminated_set.begin();
      extension != terminated_set.end(); ++extension) {
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
  RecordPermissionMessagesHistogram(extension,
                                    "Extensions.Permissions_ReEnable2");
  extension_prefs_->SetDidExtensionEscalatePermissions(extension, false);
  EnableExtension(extension->id());
}

void ExtensionService::GrantPermissions(const Extension* extension) {
  CHECK(extension);
  extensions::PermissionsUpdater(profile()).GrantActivePermissions(extension);
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
      extension->permissions_data()->GetPermissionMessages();
  if (permissions.empty()) {
    counter->Add(PermissionMessage::kNone);
  } else {
    for (PermissionMessages::iterator it = permissions.begin();
         it != permissions.end(); ++it)
      counter->Add(it->id());
  }
}

void ExtensionService::NotifyExtensionLoaded(const Extension* extension) {
  // The URLRequestContexts need to be first to know that the extension
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
  // ExtensionRegistryObserver::OnLoaded or
  // NOTIFICATION_EXTENSION_LOADED_DEPRECATED, the
  // renderer is guaranteed to know about it.
  registry_->TriggerOnLoaded(extension);

  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
      content::Source<Profile>(profile_),
      content::Details<const Extension>(extension));

  // TODO(kalman): Convert ExtensionSpecialStoragePolicy to a
  // BrowserContextKeyedService and use ExtensionRegistryObserver.
  profile_->GetExtensionSpecialStoragePolicy()->
      GrantRightsForExtension(extension);

  // TODO(kalman): This is broken. The crash reporter is process-wide so doesn't
  // work properly multi-profile. Besides which, it should be using
  // ExtensionRegistryObserver. See http://crbug.com/355029.
  UpdateActiveExtensionsInCrashReporter();

  const extensions::PermissionsData* permissions_data =
      extension->permissions_data();

  // If the extension has permission to load chrome://favicon/ resources we need
  // to make sure that the FaviconSource is registered with the
  // ChromeURLDataManager.
  if (permissions_data->HasHostPermission(GURL(chrome::kChromeUIFaviconURL))) {
    FaviconSource* favicon_source = new FaviconSource(profile_,
                                                      FaviconSource::FAVICON);
    content::URLDataSource::Add(profile_, favicon_source);
  }

  // Same for chrome://theme/ resources.
  if (permissions_data->HasHostPermission(GURL(chrome::kChromeUIThemeURL))) {
    ThemeSource* theme_source = new ThemeSource(profile_);
    content::URLDataSource::Add(profile_, theme_source);
  }

  // Same for chrome://thumb/ resources.
  if (permissions_data->HasHostPermission(
          GURL(chrome::kChromeUIThumbnailURL))) {
    ThumbnailSource* thumbnail_source = new ThumbnailSource(profile_, false);
    content::URLDataSource::Add(profile_, thumbnail_source);
  }
}

void ExtensionService::NotifyExtensionUnloaded(
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  UnloadedExtensionInfo details(extension, reason);

  registry_->TriggerOnUnloaded(extension, reason);

  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
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

  // TODO(kalman): Convert ExtensionSpecialStoragePolicy to a
  // BrowserContextKeyedService and use ExtensionRegistryObserver.
  profile_->GetExtensionSpecialStoragePolicy()->
      RevokeRightsForExtension(extension);

#if defined(OS_CHROMEOS)
  // Revoke external file access for the extension from its file system context.
  // It is safe to access the extension's storage partition at this point. The
  // storage partition may get destroyed only after the extension gets unloaded.
  GURL site =
      extensions::util::GetSiteForExtensionId(extension->id(), profile_);
  fileapi::FileSystemContext* filesystem_context =
      BrowserContext::GetStoragePartitionForSite(profile_, site)->
          GetFileSystemContext();
  if (filesystem_context && filesystem_context->external_backend()) {
    filesystem_context->external_backend()->
        RevokeAccessForExtension(extension->id());
  }
#endif

  // TODO(kalman): This is broken. The crash reporter is process-wide so doesn't
  // work properly multi-profile. Besides which, it should be using
  // ExtensionRegistryObserver::OnExtensionLoaded. See http://crbug.com/355029.
  UpdateActiveExtensionsInCrashReporter();
}

content::BrowserContext* ExtensionService::GetBrowserContext() const {
  // Implemented in the .cc file to avoid adding a profile.h dependency to
  // extension_service.h.
  return profile_;
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

void ExtensionService::CheckManagementPolicy() {
  std::vector<std::string> to_unload;
  std::map<std::string, Extension::DisableReason> to_disable;

  // Loop through the extensions list, finding extensions we need to unload or
  // disable.
  const ExtensionSet& extensions = registry_->enabled_extensions();
  for (ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end(); ++iter) {
    const Extension* extension = (iter->get());
    if (!system_->management_policy()->UserMayLoad(extension, NULL))
      to_unload.push_back(extension->id());
    Extension::DisableReason disable_reason = Extension::DISABLE_NONE;
    if (system_->management_policy()->MustRemainDisabled(
            extension, &disable_reason, NULL))
      to_disable[extension->id()] = disable_reason;
  }

  for (size_t i = 0; i < to_unload.size(); ++i)
    UnloadExtension(to_unload[i], UnloadedExtensionInfo::REASON_DISABLE);

  for (std::map<std::string, Extension::DisableReason>::const_iterator i =
           to_disable.begin(); i != to_disable.end(); ++i)
    DisableExtension(i->first, i->second);
}

void ExtensionService::CheckForUpdatesSoon() {
  // This can legitimately happen in unit tests.
  if (!updater_.get())
    return;

  if (AreAllExternalProvidersReady()) {
    updater_->CheckSoon();
  } else {
    // Sync can start updating before all the external providers are ready
    // during startup. Start the update as soon as those providers are ready,
    // but not before.
    update_once_all_providers_are_ready_ = true;
  }
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
  if (external_extension_providers_.empty())
    OnAllExternalProvidersReady();
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
    extensions::ExtensionUpdater::CheckParams params;
    params.callback = external_updates_finished_callback_;
    updater()->CheckNow(params);
  }

  // Uninstall all the unclaimed extensions.
  scoped_ptr<extensions::ExtensionPrefs::ExtensionsInfo> extensions_info(
      extension_prefs_->GetInstalledExtensionsInfo());
  for (size_t i = 0; i < extensions_info->size(); ++i) {
    ExtensionInfo* info = extensions_info->at(i).get();
    if (Manifest::IsExternalLocation(info->extension_location))
      CheckExternalUninstall(info->extension_id);
  }

  error_controller_->ShowErrorIfNeeded();

  external_install_manager_->UpdateExternalExtensionAlert();
}

void ExtensionService::UnloadExtension(
    const std::string& extension_id,
    UnloadedExtensionInfo::Reason reason) {
  // Make sure the extension gets deleted after we return from this function.
  int include_mask =
      ExtensionRegistry::EVERYTHING & ~ExtensionRegistry::TERMINATED;
  scoped_refptr<const Extension> extension(
      registry_->GetExtensionById(extension_id, include_mask));

  // This method can be called via PostTask, so the extension may have been
  // unloaded by the time this runs.
  if (!extension.get()) {
    // In case the extension may have crashed/uninstalled. Allow the profile to
    // clean up its RequestContexts.
    system_->UnregisterExtensionWithRequestContexts(extension_id, reason);
    return;
  }

  // Keep information about the extension so that we can reload it later
  // even if it's not permanently installed.
  unloaded_extension_paths_[extension->id()] = extension->path();

  // Clean up if the extension is meant to be enabled after a reload.
  reloading_extensions_.erase(extension->id());

  if (registry_->disabled_extensions().Contains(extension->id())) {
    registry_->RemoveDisabled(extension->id());
    // Make sure the profile cleans up its RequestContexts when an already
    // disabled extension is unloaded (since they are also tracking the disabled
    // extensions).
    system_->UnregisterExtensionWithRequestContexts(extension_id, reason);
    // Don't send the unloaded notification. It was sent when the extension
    // was disabled.
  } else {
    // Remove the extension from the enabled list.
    registry_->RemoveEnabled(extension->id());
    NotifyExtensionUnloaded(extension.get(), reason);
  }

  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_REMOVED,
      content::Source<Profile>(profile_),
      content::Details<const Extension>(extension.get()));
}

void ExtensionService::RemoveComponentExtension(
    const std::string& extension_id) {
  scoped_refptr<const Extension> extension(
      GetExtensionById(extension_id, false));
  UnloadExtension(extension_id, UnloadedExtensionInfo::REASON_UNINSTALL);
  if (extension.get()) {
    content::NotificationService::current()->Notify(
        extensions::NOTIFICATION_EXTENSION_UNINSTALLED_DEPRECATED,
        content::Source<Profile>(profile_),
        content::Details<const Extension>(extension.get()));
    ExtensionRegistry::Get(profile_)->TriggerOnUninstalled(
        extension.get(), extensions::UNINSTALL_REASON_INTERNAL_MANAGEMENT);
  }
}

void ExtensionService::UnloadAllExtensionsForTest() {
  UnloadAllExtensionsInternal();
}

void ExtensionService::ReloadExtensionsForTest() {
  // Calling UnloadAllExtensionsForTest here triggers a false-positive presubmit
  // warning about calling test code in production.
  UnloadAllExtensionsInternal();
  component_loader_->LoadAll();
  extensions::InstalledLoader(this).LoadAllExtensions();
  // Don't call SetReadyAndNotifyListeners() since tests call this multiple
  // times.
}

void ExtensionService::SetReadyAndNotifyListeners() {
  ready_->Signal();
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED,
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
  system_->runtime_data()->SetBeingUpgraded(extension, is_extension_upgrade);

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
    UnloadExtension(extension->id(), UnloadedExtensionInfo::REASON_UPDATE);
  }

  if (extension_prefs_->IsExtensionBlacklisted(extension->id())) {
    // Only prefs is checked for the blacklist. We rely on callers to check the
    // blacklist before calling into here, e.g. CrxInstaller checks before
    // installation then threads through the install and pending install flow
    // of this class, and we check when loading installed extensions.
    registry_->AddBlacklisted(extension);
  } else if (!reloading &&
             extension_prefs_->IsExtensionDisabled(extension->id())) {
    registry_->AddDisabled(extension);
    if (extension_sync_service_)
      extension_sync_service_->SyncExtensionChangeIfNeeded(*extension);
    content::NotificationService::current()->Notify(
        extensions::NOTIFICATION_EXTENSION_UPDATE_DISABLED,
        content::Source<Profile>(profile_),
        content::Details<const Extension>(extension));

    // Show the extension disabled error if a permissions increase or a remote
    // installation is the reason it was disabled, and no other reasons exist.
    int reasons = extension_prefs_->GetDisableReasons(extension->id());
    const int kReasonMask = Extension::DISABLE_PERMISSIONS_INCREASE |
                            Extension::DISABLE_REMOTE_INSTALL;
    if (reasons & kReasonMask && !(reasons & ~kReasonMask)) {
      extensions::AddExtensionDisabledError(
          this,
          extension,
          extension_prefs_->HasDisableReason(
              extension->id(), Extension::DISABLE_REMOTE_INSTALL));
    }
  } else if (reloading) {
    // Replace the old extension with the new version.
    CHECK(!registry_->AddDisabled(extension));
    EnableExtension(extension->id());
  } else {
    // All apps that are displayed in the launcher are ordered by their ordinals
    // so we must ensure they have valid ordinals.
    if (extension->RequiresSortOrdinal()) {
      extension_prefs_->app_sorting()->SetExtensionVisible(
          extension->id(),
          extension->ShouldDisplayInNewTabPage() &&
              !extension_prefs_->IsEphemeralApp(extension->id()));
      if (!extension_prefs_->IsEphemeralApp(extension->id())) {
        extension_prefs_->app_sorting()->EnsureValidOrdinals(
            extension->id(), syncer::StringOrdinal());
      }
    }

    registry_->AddEnabled(extension);
    if (extension_sync_service_)
      extension_sync_service_->SyncExtensionChangeIfNeeded(*extension);
    NotifyExtensionLoaded(extension);
  }
  system_->runtime_data()->SetBeingUpgraded(extension, false);
}

void ExtensionService::AddComponentExtension(const Extension* extension) {
  const std::string old_version_string(
      extension_prefs_->GetVersionString(extension->id()));
  const Version old_version(old_version_string);

  VLOG(1) << "AddComponentExtension " << extension->name();
  if (!old_version.IsValid() || !old_version.Equals(*extension->version())) {
    VLOG(1) << "Component extension " << extension->name() << " ("
        << extension->id() << ") installing/upgrading from '"
        << old_version_string << "' to " << extension->version()->GetString();

    AddNewOrUpdatedExtension(extension,
                             Extension::ENABLED,
                             extensions::kInstallFlagNone,
                             syncer::StringOrdinal(),
                             std::string());
    return;
  }

  AddExtension(extension);
}

void ExtensionService::CheckPermissionsIncrease(const Extension* extension,
                                                bool is_extension_installed) {
  extensions::PermissionsUpdater(profile_).InitializePermissions(extension);

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
      extensions::ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode();
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
    is_privilege_increase =
        extensions::PermissionMessageProvider::Get()->IsPrivilegeIncrease(
            granted_permissions,
            extension->permissions_data()->active_permissions().get(),
            extension->GetType());
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
  // notification should be sent by the caller. If the extension is already
  // disabled because it was installed remotely, don't add another disable
  // reason, but instead always set the "did escalate permissions" flag, to
  // ensure enabling it will always show a warning.
  if (disable_reasons == Extension::DISABLE_REMOTE_INSTALL) {
    extension_prefs_->SetDidExtensionEscalatePermissions(extension, true);
  } else if (is_privilege_increase) {
    disable_reasons |= Extension::DISABLE_PERMISSIONS_INCREASE;
    if (!extension_prefs_->DidExtensionEscalatePermissions(extension->id())) {
      RecordPermissionMessagesHistogram(extension,
                                        "Extensions.Permissions_AutoDisable2");
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
  const ExtensionSet& extensions = registry_->enabled_extensions();
  for (ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end(); ++iter) {
    const Extension* extension = iter->get();
    if (!extension->is_theme() && extension->location() != Manifest::COMPONENT)
      extension_ids.insert(extension->id());
  }

  // TODO(kalman): This is broken. ExtensionService is per-profile.
  // crash_keys::SetActiveExtensions is per-process. See
  // http://crbug.com/355029.
  crash_keys::SetActiveExtensions(extension_ids);
}

void ExtensionService::OnExtensionInstalled(
    const Extension* extension,
    const syncer::StringOrdinal& page_ordinal,
    int install_flags) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const std::string& id = extension->id();
  bool initial_enable = ShouldEnableOnInstall(extension);
  std::string install_parameter;
  const extensions::PendingExtensionInfo* pending_extension_info =
      pending_extension_manager()->GetById(id);
  if (pending_extension_info) {
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
              base::Bind(&extensions::file_util::DeleteFile,
                         extension->path(),
                         true))) {
        NOTREACHED();
      }
      return;
    }

    install_parameter = pending_extension_info->install_parameter();
    pending_extension_manager()->Remove(id);
  } else {
    // We explicitly want to re-enable an uninstalled external
    // extension; if we're here, that means the user is manually
    // installing the extension.
    if (extension_prefs_->IsExternalExtensionUninstalled(id)) {
      initial_enable = true;
    }
  }

  // Unsupported requirements overrides the management policy.
  if (install_flags & extensions::kInstallFlagHasRequirementErrors) {
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

  if (install_flags & extensions::kInstallFlagIsBlacklistedForMalware) {
    // Installation of a blacklisted extension can happen from sync, policy,
    // etc, where to maintain consistency we need to install it, just never
    // load it (see AddExtension). Usually it should be the job of callers to
    // incercept blacklisted extension earlier (e.g. CrxInstaller, before even
    // showing the install dialogue).
    extension_prefs_->AcknowledgeBlacklistedExtension(id);
    UMA_HISTOGRAM_ENUMERATION("ExtensionBlacklist.SilentInstall",
                              extension->location(),
                              Manifest::NUM_LOCATIONS);
  }

  if (!GetInstalledExtension(extension->id())) {
    UMA_HISTOGRAM_ENUMERATION("Extensions.InstallType",
                              extension->GetType(), 100);
    UMA_HISTOGRAM_ENUMERATION("Extensions.InstallSource",
                              extension->location(), Manifest::NUM_LOCATIONS);
    RecordPermissionMessagesHistogram(extension,
                                      "Extensions.Permissions_Install2");
  } else {
    UMA_HISTOGRAM_ENUMERATION("Extensions.UpdateType",
                              extension->GetType(), 100);
    UMA_HISTOGRAM_ENUMERATION("Extensions.UpdateSource",
                              extension->location(), Manifest::NUM_LOCATIONS);

    // A fully installed app cannot be demoted to an ephemeral app.
    if ((install_flags & extensions::kInstallFlagIsEphemeral) &&
        !extension_prefs_->IsEphemeralApp(id)) {
      install_flags &= ~static_cast<int>(extensions::kInstallFlagIsEphemeral);
    }
  }

  const Extension::State initial_state =
      initial_enable ? Extension::ENABLED : Extension::DISABLED;
  if (ShouldDelayExtensionUpdate(
          id,
          !!(install_flags & extensions::kInstallFlagInstallImmediately))) {
    extension_prefs_->SetDelayedInstallInfo(
        extension,
        initial_state,
        install_flags,
        extensions::ExtensionPrefs::DELAY_REASON_WAIT_FOR_IDLE,
        page_ordinal,
        install_parameter);

    // Transfer ownership of |extension|.
    delayed_installs_.Insert(extension);

    // Notify observers that app update is available.
    FOR_EACH_OBSERVER(extensions::UpdateObserver, update_observers_,
                      OnAppUpdateAvailable(extension));
    return;
  }

  extensions::SharedModuleService::ImportStatus status =
      shared_module_service_->SatisfyImports(extension);
  if (installs_delayed_for_gc_) {
    extension_prefs_->SetDelayedInstallInfo(
        extension,
        initial_state,
        install_flags,
        extensions::ExtensionPrefs::DELAY_REASON_GC,
        page_ordinal,
        install_parameter);
    delayed_installs_.Insert(extension);
  } else if (status != SharedModuleService::IMPORT_STATUS_OK) {
    if (status == SharedModuleService::IMPORT_STATUS_UNSATISFIED) {
      extension_prefs_->SetDelayedInstallInfo(
          extension,
          initial_state,
          install_flags,
          extensions::ExtensionPrefs::DELAY_REASON_WAIT_FOR_IMPORTS,
          page_ordinal,
          install_parameter);
      delayed_installs_.Insert(extension);
    }
  } else {
    AddNewOrUpdatedExtension(extension,
                             initial_state,
                             install_flags,
                             page_ordinal,
                             install_parameter);
  }
}

void ExtensionService::AddNewOrUpdatedExtension(
    const Extension* extension,
    Extension::State initial_state,
    int install_flags,
    const syncer::StringOrdinal& page_ordinal,
    const std::string& install_parameter) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  bool was_ephemeral = extension_prefs_->IsEphemeralApp(extension->id());
  extension_prefs_->OnExtensionInstalled(
      extension, initial_state, page_ordinal, install_flags, install_parameter);
  delayed_installs_.Remove(extension->id());
  if (InstallVerifier::NeedsVerification(*extension))
    system_->install_verifier()->VerifyExtension(extension->id());
  FinishInstallation(extension, was_ephemeral);
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
       is_ready() && !extensions::util::IsExtensionIdle(extension_id, profile_))
    return;

  const Extension* extension = delayed_installs_.GetByID(extension_id);
  if (reason == extensions::ExtensionPrefs::DELAY_REASON_WAIT_FOR_IMPORTS) {
    extensions::SharedModuleService::ImportStatus status =
        shared_module_service_->SatisfyImports(extension);
    if (status != SharedModuleService::IMPORT_STATUS_OK) {
      if (status == SharedModuleService::IMPORT_STATUS_UNRECOVERABLE) {
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

  bool was_ephemeral = extension_prefs_->IsEphemeralApp(extension->id());
  if (!extension_prefs_->FinishDelayedInstallInfo(extension_id))
    NOTREACHED();

  FinishInstallation(extension.get(), was_ephemeral);
}

void ExtensionService::FinishInstallation(
    const Extension* extension, bool was_ephemeral) {
  const extensions::Extension* existing_extension =
      GetInstalledExtension(extension->id());
  bool is_update = false;
  std::string old_name;
  if (existing_extension) {
    is_update = true;
    old_name = existing_extension->name();
  }
  bool from_ephemeral =
      was_ephemeral && !extension_prefs_->IsEphemeralApp(extension->id());
  extensions::InstalledExtensionInfo details(
      extension, is_update, from_ephemeral, old_name);
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED,
      content::Source<Profile>(profile_),
      content::Details<const extensions::InstalledExtensionInfo>(&details));

  registry_->TriggerOnWillBeInstalled(
      extension, is_update, from_ephemeral, old_name);

  // Unpacked extensions default to allowing file access, but if that has been
  // overridden, don't reset the value.
  if (Manifest::ShouldAlwaysAllowFileAccess(extension->location()) &&
      !extension_prefs_->HasAllowFileAccessSetting(extension->id())) {
    extension_prefs_->SetAllowFileAccess(extension->id(), true);
  }

  AddExtension(extension);

  // Notify observers that need to know when an installation is complete.
  registry_->TriggerOnInstalled(extension, is_update);

  // Check extensions that may have been delayed only because this shared module
  // was not available.
  if (SharedModuleInfo::IsSharedModule(extension))
    MaybeFinishDelayedInstallations();
}

void ExtensionService::PromoteEphemeralApp(
    const extensions::Extension* extension, bool is_from_sync) {
  DCHECK(GetInstalledExtension(extension->id()) &&
         extension_prefs_->IsEphemeralApp(extension->id()));

  if (extension->RequiresSortOrdinal()) {
    extension_prefs_->app_sorting()->SetExtensionVisible(
        extension->id(), extension->ShouldDisplayInNewTabPage());

    if (!is_from_sync) {
      // Reset the sort ordinals of the app to ensure it is added to the default
      // position, like newly installed apps would.
      extension_prefs_->app_sorting()->ClearOrdinals(extension->id());
    }

    extension_prefs_->app_sorting()->EnsureValidOrdinals(
        extension->id(), syncer::StringOrdinal());
  }

  // Remove the ephemeral flags from the preferences.
  extension_prefs_->OnEphemeralAppPromoted(extension->id());

  // Fire install-related events to allow observers to handle the promotion
  // of the ephemeral app.
  extensions::InstalledExtensionInfo details(
      extension,
      true /* is update */,
      true /* from ephemeral */,
      extension->name() /* old name */);
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED,
      content::Source<Profile>(profile_),
      content::Details<const extensions::InstalledExtensionInfo>(&details));

  registry_->TriggerOnWillBeInstalled(
      extension,
      true /* is update */,
      true /* from ephemeral */,
      extension->name() /* old name */);

  if (registry_->enabled_extensions().Contains(extension->id())) {
    // If the app is already enabled and loaded, fire the load events to allow
    // observers to handle the promotion of the ephemeral app.
    content::NotificationService::current()->Notify(
        extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
        content::Source<Profile>(profile_),
        content::Details<const Extension>(extension));

    registry_->TriggerOnLoaded(extension);
  } else {
    // Cached ephemeral apps may be updated and disabled due to permissions
    // increase. The app can be enabled (as long as no other disable reasons
    // exist) as the install was user-acknowledged.
    int disable_mask = Extension::DISABLE_NONE;
    if (!is_from_sync)
      disable_mask |= Extension::DISABLE_PERMISSIONS_INCREASE;

    int other_disable_reasons =
        extension_prefs_->GetDisableReasons(extension->id()) & ~disable_mask;
    if (!other_disable_reasons) {
      if (extension_prefs_->DidExtensionEscalatePermissions(extension->id()))
        GrantPermissionsAndEnableExtension(extension);
      else
        EnableExtension(extension->id());
    }
  }

  registry_->TriggerOnInstalled(extension, true);

  if (!is_from_sync && extension_sync_service_)
    extension_sync_service_->SyncExtensionChangeIfNeeded(*extension);
}

const Extension* ExtensionService::GetPendingExtensionUpdate(
    const std::string& id) const {
  return delayed_installs_.GetByID(id);
}

void ExtensionService::TrackTerminatedExtension(const Extension* extension) {
  // No need to check for duplicates; inserting a duplicate is a no-op.
  registry_->AddTerminated(make_scoped_refptr(extension));
  extensions_being_terminated_.erase(extension->id());
  UnloadExtension(extension->id(), UnloadedExtensionInfo::REASON_TERMINATE);
}

void ExtensionService::TerminateExtension(const std::string& extension_id) {
  const Extension* extension = GetInstalledExtension(extension_id);
  TrackTerminatedExtension(extension);
}

void ExtensionService::UntrackTerminatedExtension(const std::string& id) {
  std::string lowercase_id = base::StringToLowerASCII(id);
  const Extension* extension =
      registry_->terminated_extensions().GetByID(lowercase_id);
  registry_->RemoveTerminated(lowercase_id);
  if (extension) {
    content::NotificationService::current()->Notify(
        extensions::NOTIFICATION_EXTENSION_REMOVED,
        content::Source<Profile>(profile_),
        content::Details<const Extension>(extension));
  }
}

const Extension* ExtensionService::GetInstalledExtension(
    const std::string& id) const {
  return registry_->GetExtensionById(id, ExtensionRegistry::EVERYTHING);
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
          id, location, *version, creation_flags, mark_acknowledged)) {
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
    external_install_manager_->AcknowledgeExternalExtension(id);

  return true;
}

void ExtensionService::DidCreateRenderViewForBackgroundPage(
    extensions::ExtensionHost* host) {
  OrphanedDevTools::iterator iter =
      orphaned_dev_tools_.find(host->extension_id());
  if (iter == orphaned_dev_tools_.end())
    return;

  iter->second->ConnectWebContents(host->host_contents());
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
    case extensions::NOTIFICATION_EXTENSION_PROCESS_TERMINATED: {
      if (profile_ !=
          content::Source<Profile>(source).ptr()->GetOriginalProfile()) {
        break;
      }

      extensions::ExtensionHost* host =
          content::Details<extensions::ExtensionHost>(details).ptr();

      // If the extension is already being terminated, there is nothing left to
      // do.
      if (!extensions_being_terminated_.insert(host->extension_id()).second)
        break;

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
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED: {
      content::RenderProcessHost* process =
          content::Source<content::RenderProcessHost>(source).ptr();
      Profile* host_profile =
          Profile::FromBrowserContext(process->GetBrowserContext());
      if (!profile_->IsSameProfile(host_profile->GetOriginalProfile()))
          break;

      extensions::ProcessMap* process_map =
          extensions::ProcessMap::Get(profile_);
      if (process_map->Contains(process->GetID())) {
        // An extension process was terminated, this might have resulted in an
        // app or extension becoming idle.
        std::set<std::string> extension_ids =
            process_map->GetExtensionsInProcess(process->GetID());
        for (std::set<std::string>::const_iterator it = extension_ids.begin();
             it != extension_ids.end(); ++it) {
          if (delayed_installs_.Contains(*it)) {
            base::MessageLoop::current()->PostDelayedTask(
                FROM_HERE,
                base::Bind(&ExtensionService::MaybeFinishDelayedInstallation,
                           AsWeakPtr(), *it),
                base::TimeDelta::FromSeconds(kUpdateIdleDelay));
          }
        }
      }

      process_map->RemoveAllFromProcess(process->GetID());
      BrowserThread::PostTask(
          BrowserThread::IO,
          FROM_HERE,
          base::Bind(&extensions::InfoMap::UnregisterAllExtensionsInProcess,
                     system_->info_map(),
                     process->GetID()));
      break;
    }
    case chrome::NOTIFICATION_UPGRADE_RECOMMENDED: {
      // Notify observers that chrome update is available.
      FOR_EACH_OBSERVER(extensions::UpdateObserver, update_observers_,
                        OnChromeUpdateAvailable());
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTRUCTION_STARTED: {
      OnProfileDestructionStarted();
      break;
    }

    default:
      NOTREACHED() << "Unexpected notification type.";
  }
}

void ExtensionService::OnExtensionInstallPrefChanged() {
  error_controller_->ShowErrorIfNeeded();
  CheckManagementPolicy();
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

bool ExtensionService::ShouldDelayExtensionUpdate(
    const std::string& extension_id,
    bool install_immediately) const {
  const char kOnUpdateAvailableEvent[] = "runtime.onUpdateAvailable";

  // If delayed updates are globally disabled, or just for this extension,
  // don't delay.
  if (!install_updates_when_idle_ || install_immediately)
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
    return !extensions::util::IsExtensionIdle(extension_id, profile_);
  }
}

void ExtensionService::OnGarbageCollectIsolatedStorageStart() {
  DCHECK(!installs_delayed_for_gc_);
  installs_delayed_for_gc_ = true;
}

void ExtensionService::OnGarbageCollectIsolatedStorageFinished() {
  DCHECK(installs_delayed_for_gc_);
  installs_delayed_for_gc_ = false;
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

void ExtensionService::OnBlacklistUpdated() {
  blacklist_->GetBlacklistedIDs(
      registry_->GenerateInstalledExtensionsSet()->GetIDs(),
      base::Bind(&ExtensionService::ManageBlacklist, AsWeakPtr()));
}

void ExtensionService::ManageBlacklist(
    const extensions::Blacklist::BlacklistStateMap& state_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::set<std::string> blocked;
  ExtensionIdSet greylist;
  ExtensionIdSet unchanged;
  for (extensions::Blacklist::BlacklistStateMap::const_iterator it =
           state_map.begin();
       it != state_map.end();
       ++it) {
    switch (it->second) {
      case extensions::NOT_BLACKLISTED:
        break;

      case extensions::BLACKLISTED_MALWARE:
        blocked.insert(it->first);
        break;

      case extensions::BLACKLISTED_SECURITY_VULNERABILITY:
      case extensions::BLACKLISTED_CWS_POLICY_VIOLATION:
      case extensions::BLACKLISTED_POTENTIALLY_UNWANTED:
        greylist.insert(it->first);
        break;

      case extensions::BLACKLISTED_UNKNOWN:
        unchanged.insert(it->first);
        break;
    }
  }

  UpdateBlockedExtensions(blocked, unchanged);
  UpdateGreylistedExtensions(greylist, unchanged, state_map);

  error_controller_->ShowErrorIfNeeded();
}

namespace {
void Partition(const ExtensionIdSet& before,
               const ExtensionIdSet& after,
               const ExtensionIdSet& unchanged,
               ExtensionIdSet* no_longer,
               ExtensionIdSet* not_yet) {
  *not_yet   = base::STLSetDifference<ExtensionIdSet>(after, before);
  *no_longer = base::STLSetDifference<ExtensionIdSet>(before, after);
  *no_longer = base::STLSetDifference<ExtensionIdSet>(*no_longer, unchanged);
}
}  // namespace

void ExtensionService::UpdateBlockedExtensions(
    const ExtensionIdSet& blocked,
    const ExtensionIdSet& unchanged) {
  ExtensionIdSet not_yet_blocked, no_longer_blocked;
  Partition(registry_->blacklisted_extensions().GetIDs(),
            blocked, unchanged,
            &no_longer_blocked, &not_yet_blocked);

  for (ExtensionIdSet::iterator it = no_longer_blocked.begin();
       it != no_longer_blocked.end(); ++it) {
    scoped_refptr<const Extension> extension =
        registry_->blacklisted_extensions().GetByID(*it);
    if (!extension.get()) {
      NOTREACHED() << "Extension " << *it << " no longer blocked, "
                   << "but it was never blocked.";
      continue;
    }
    registry_->RemoveBlacklisted(*it);
    extension_prefs_->SetExtensionBlacklisted(extension->id(), false);
    AddExtension(extension.get());
    UMA_HISTOGRAM_ENUMERATION("ExtensionBlacklist.UnblacklistInstalled",
                              extension->location(),
                              Manifest::NUM_LOCATIONS);
  }

  for (ExtensionIdSet::iterator it = not_yet_blocked.begin();
       it != not_yet_blocked.end(); ++it) {
    scoped_refptr<const Extension> extension = GetInstalledExtension(*it);
    if (!extension.get()) {
      NOTREACHED() << "Extension " << *it << " needs to be "
                   << "blacklisted, but it's not installed.";
      continue;
    }
    registry_->AddBlacklisted(extension);
    extension_prefs_->SetExtensionBlacklistState(
        extension->id(), extensions::BLACKLISTED_MALWARE);
    UnloadExtension(*it, UnloadedExtensionInfo::REASON_BLACKLIST);
    UMA_HISTOGRAM_ENUMERATION("ExtensionBlacklist.BlacklistInstalled",
                              extension->location(), Manifest::NUM_LOCATIONS);
  }
}

// TODO(oleg): UMA logging
void ExtensionService::UpdateGreylistedExtensions(
    const ExtensionIdSet& greylist,
    const ExtensionIdSet& unchanged,
    const extensions::Blacklist::BlacklistStateMap& state_map) {
  ExtensionIdSet not_yet_greylisted, no_longer_greylisted;
  Partition(greylist_.GetIDs(),
            greylist, unchanged,
            &no_longer_greylisted, &not_yet_greylisted);

  for (ExtensionIdSet::iterator it = no_longer_greylisted.begin();
       it != no_longer_greylisted.end(); ++it) {
    scoped_refptr<const Extension> extension = greylist_.GetByID(*it);
    if (!extension.get()) {
      NOTREACHED() << "Extension " << *it << " no longer greylisted, "
                   << "but it was not marked as greylisted.";
      continue;
    }

    greylist_.Remove(*it);
    extension_prefs_->SetExtensionBlacklistState(extension->id(),
                                                 extensions::NOT_BLACKLISTED);
    if (extension_prefs_->GetDisableReasons(extension->id()) &
        extensions::Extension::DISABLE_GREYLIST)
      EnableExtension(*it);
  }

  for (ExtensionIdSet::iterator it = not_yet_greylisted.begin();
       it != not_yet_greylisted.end(); ++it) {
    scoped_refptr<const Extension> extension = GetInstalledExtension(*it);
    if (!extension.get()) {
      NOTREACHED() << "Extension " << *it << " needs to be "
                   << "disabled, but it's not installed.";
      continue;
    }
    greylist_.Insert(extension);
    extension_prefs_->SetExtensionBlacklistState(extension->id(),
                                                 state_map.find(*it)->second);
    if (registry_->enabled_extensions().Contains(extension->id()))
      DisableExtension(*it, extensions::Extension::DISABLE_GREYLIST);
  }
}

void ExtensionService::AddUpdateObserver(extensions::UpdateObserver* observer) {
  update_observers_.AddObserver(observer);
}

void ExtensionService::RemoveUpdateObserver(
    extensions::UpdateObserver* observer) {
  update_observers_.RemoveObserver(observer);
}

// Used only by test code.
void ExtensionService::UnloadAllExtensionsInternal() {
  profile_->GetExtensionSpecialStoragePolicy()->RevokeRightsForAllExtensions();

  registry_->ClearAll();
  system_->runtime_data()->ClearAll();

  // TODO(erikkay) should there be a notification for this?  We can't use
  // EXTENSION_UNLOADED since that implies that the extension has been disabled
  // or uninstalled.
}

void ExtensionService::OnProfileDestructionStarted() {
  ExtensionIdSet ids_to_unload = registry_->enabled_extensions().GetIDs();
  for (ExtensionIdSet::iterator it = ids_to_unload.begin();
       it != ids_to_unload.end();
       ++it) {
    UnloadExtension(*it, UnloadedExtensionInfo::REASON_PROFILE_SHUTDOWN);
  }
}
