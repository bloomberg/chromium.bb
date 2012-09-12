// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_service.h"

#include <algorithm>
#include <set>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/accessibility/accessibility_extension_api.h"
#include "chrome/browser/bookmarks/bookmark_extension_api.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_plugin_service_filter.h"
#include "chrome/browser/extensions/api/cookies/cookies_api.h"
#include "chrome/browser/extensions/api/declarative/rules_registry_service.h"
#include "chrome/browser/extensions/api/extension_action/extension_actions_api.h"
#include "chrome/browser/extensions/api/font_settings/font_settings_api.h"
#include "chrome/browser/extensions/api/managed_mode/managed_mode_api.h"
#include "chrome/browser/extensions/api/management/management_api.h"
#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_event_router.h"
#include "chrome/browser/extensions/api/processes/processes_api.h"
#include "chrome/browser/extensions/api/push_messaging/push_messaging_api.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/app_notification_manager.h"
#include "chrome/browser/extensions/app_sync_data.h"
#include "chrome/browser/extensions/browser_event_router.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/data_deleter.h"
#include "chrome/browser/extensions/default_apps_trial.h"
#include "chrome/browser/extensions/extension_disabled_ui.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_error_ui.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_preference_api.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_sorting.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/extension_sync_data.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/external_provider_interface.h"
#include "chrome/browser/extensions/installed_loader.h"
#include "chrome/browser/extensions/lazy_background_task_queue.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/platform_app_launcher.h"
#include "chrome/browser/extensions/settings/settings_frontend.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/extensions/window_event_router.h"
#include "chrome/browser/history/history_extension_api.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/ntp/thumbnail_source.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/extensions/features/feature.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host_registry.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/pepper_plugin_info.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error_factory.h"
#include "webkit/database/database_tracker.h"
#include "webkit/database/database_util.h"
#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/extensions/bluetooth_event_router.h"
#include "chrome/browser/chromeos/extensions/file_browser_event_router.h"
#include "chrome/browser/chromeos/extensions/input_method_event_router.h"
#include "chrome/browser/chromeos/extensions/media_player_event_router.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#endif

using content::BrowserContext;
using content::BrowserThread;
using content::DevToolsAgentHost;
using content::DevToolsAgentHostRegistry;
using content::PluginService;
using extensions::CrxInstaller;
using extensions::Extension;
using extensions::ExtensionIdSet;
using extensions::ExtensionInfo;
using extensions::UnloadedExtensionInfo;
using extensions::PermissionMessage;
using extensions::PermissionMessages;
using extensions::PermissionSet;

namespace errors = extension_manifest_errors;

namespace {

#if defined(OS_LINUX)
static const int kOmniboxIconPaddingLeft = 2;
static const int kOmniboxIconPaddingRight = 2;
#elif defined(OS_MACOSX)
static const int kOmniboxIconPaddingLeft = 0;
static const int kOmniboxIconPaddingRight = 2;
#else
static const int kOmniboxIconPaddingLeft = 0;
static const int kOmniboxIconPaddingRight = 0;
#endif

const char* kNaClPluginMimeType = "application/x-nacl";

static bool IsSyncableExtension(const Extension& extension) {
  return extension.GetSyncType() == Extension::SYNC_TYPE_EXTENSION;
}

static bool IsSyncableApp(const Extension& extension) {
  return extension.GetSyncType() == Extension::SYNC_TYPE_APP;
}

}  // namespace

ExtensionService::ExtensionRuntimeData::ExtensionRuntimeData()
    : background_page_ready(false),
      being_upgraded(false),
      has_used_webrequest(false) {
}

ExtensionService::ExtensionRuntimeData::~ExtensionRuntimeData() {
}

ExtensionService::NaClModuleInfo::NaClModuleInfo() {
}

ExtensionService::NaClModuleInfo::~NaClModuleInfo() {
}

// ExtensionService.

const char ExtensionService::kInstallDirectoryName[] = "Extensions";

const char ExtensionService::kLocalAppSettingsDirectoryName[] =
    "Local App Settings";
const char ExtensionService::kLocalExtensionSettingsDirectoryName[] =
    "Local Extension Settings";
const char ExtensionService::kSyncAppSettingsDirectoryName[] =
    "Sync App Settings";
const char ExtensionService::kSyncExtensionSettingsDirectoryName[] =
    "Sync Extension Settings";
const char ExtensionService::kStateStoreName[] = "Extension State";

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
    Extension::Location location) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(Extension::IdIsValid(id));

  const Extension* extension = GetExtensionById(id, true);
  if (extension) {
    // Already installed. Skip this install if the current location has
    // higher priority than |location|.
    Extension::Location current = extension->location();
    if (current == Extension::GetHigherPriorityLocation(current, location))
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
  const Extension* extension = extensions_.GetExtensionOrAppByURL(
      ExtensionURLInfo(url));
  return (extension && extension->is_app()) ? extension : NULL;
}

bool ExtensionService::IsInstalledApp(const GURL& url) const {
  return !!GetInstalledApp(url);
}

void ExtensionService::SetInstalledAppForRenderer(int renderer_child_id,
                                                  const Extension* app) {
  installed_app_hosts_[renderer_child_id] = app;
}

const Extension* ExtensionService::GetInstalledAppForRenderer(
    int renderer_child_id) const {
  InstalledAppMap::const_iterator i =
      installed_app_hosts_.find(renderer_child_id);
  return i == installed_app_hosts_.end() ? NULL : i->second;
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
                                   const FilePath& install_directory,
                                   extensions::ExtensionPrefs* extension_prefs,
                                   bool autoupdate_enabled,
                                   bool extensions_enabled)
    : profile_(profile),
      system_(extensions::ExtensionSystem::Get(profile)),
      extension_prefs_(extension_prefs),
      settings_frontend_(extensions::SettingsFrontend::Create(profile)),
      pending_extension_manager_(*ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      install_directory_(install_directory),
      extensions_enabled_(extensions_enabled),
      show_extensions_prompts_(true),
      ready_(false),
      toolbar_model_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      menu_manager_(profile),
      app_notification_manager_(
          new extensions::AppNotificationManager(profile)),
      event_routers_initialized_(false),
      update_once_all_providers_are_ready_(false),
      app_sync_bundle_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      extension_sync_bundle_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      extension_warnings_(profile),
      app_shortcut_manager_(profile) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Figure out if extension installation should be enabled.
  if (command_line->HasSwitch(switches::kDisableExtensions) ||
      profile->GetPrefs()->GetBoolean(prefs::kDisableExtensions)) {
    extensions_enabled_ = false;
  }

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(prefs::kExtensionInstallAllowList, this);
  pref_change_registrar_.Add(prefs::kExtensionInstallDenyList, this);

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

  app_notification_manager_->Init();

  if (extensions_enabled_) {
    if (!command_line->HasSwitch(switches::kImport) &&
        !command_line->HasSwitch(switches::kImportFromFile)) {
      extensions::ExternalProviderImpl::CreateExternalProviders(
          this, profile_, &external_extension_providers_);
    }
  }

  // Use monochrome icons for Omnibox icons.
  omnibox_popup_icon_manager_.set_monochrome(true);
  omnibox_icon_manager_.set_monochrome(true);
  omnibox_icon_manager_.set_padding(gfx::Insets(0, kOmniboxIconPaddingLeft,
                                                0, kOmniboxIconPaddingRight));

  // Set this as the ExtensionService for extension sorting to ensure it
  // cause syncs if required.
  extension_prefs_->extension_sorting()->SetExtensionService(this);

#if defined(ENABLE_EXTENSIONS)
  extension_action_storage_manager_.reset(
      new extensions::ExtensionActionStorageManager(profile_));
#endif

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

const ExtensionSet* ExtensionService::GenerateInstalledExtensionsSet() const {
  ExtensionSet* installed_extensions = new ExtensionSet();
  installed_extensions->InsertAll(extensions_);
  installed_extensions->InsertAll(disabled_extensions_);
  installed_extensions->InsertAll(terminated_extensions_);
  return installed_extensions;
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

void ExtensionService::InitEventRoutersAfterImport() {
  RegisterForImportFinished();
}

void ExtensionService::RegisterForImportFinished() {
  if (!registrar_.IsRegistered(this, chrome::NOTIFICATION_IMPORT_FINISHED,
                               content::Source<Profile>(profile_))) {
    registrar_.Add(this, chrome::NOTIFICATION_IMPORT_FINISHED,
                   content::Source<Profile>(profile_));
  }
}

void ExtensionService::InitAfterImport() {
  CheckForExternalUpdates();

  GarbageCollectExtensions();

  // Idempotent, so although there is a possible race if the import
  // process finished sometime in the middle of ProfileImpl::InitExtensions,
  // it cannot happen twice.
  InitEventRouters();
}

void ExtensionService::InitEventRouters() {
  if (event_routers_initialized_)
    return;

#if defined(ENABLE_EXTENSIONS)
  history_event_router_.reset(new HistoryExtensionEventRouter());
  history_event_router_->ObserveProfile(profile_);
  browser_event_router_.reset(new extensions::BrowserEventRouter(profile_));
  browser_event_router_->Init();
  window_event_router_.reset(new extensions::WindowEventRouter(profile_));
  window_event_router_->Init();
  preference_event_router_.reset(new ExtensionPreferenceEventRouter(profile_));
  bookmark_event_router_.reset(new BookmarkExtensionEventRouter(
      BookmarkModelFactory::GetForProfile(profile_)));
  bookmark_event_router_->Init();
  cookies_event_router_.reset(
      new extensions::ExtensionCookiesEventRouter(profile_));
  cookies_event_router_->Init();
  management_event_router_.reset(new ExtensionManagementEventRouter(profile_));
  management_event_router_->Init();
  extensions::ProcessesEventRouter::GetInstance()->ObserveProfile(profile_);
  web_navigation_event_router_.reset(
      new extensions::WebNavigationEventRouter(profile_));
  web_navigation_event_router_->Init();
  font_settings_event_router_.reset(
      new extensions::FontSettingsEventRouter(profile_));
  font_settings_event_router_->Init();
  managed_mode_event_router_.reset(
      new extensions::ExtensionManagedModeEventRouter(profile_));
  managed_mode_event_router_->Init();
  push_messaging_event_router_.reset(
      new extensions::PushMessagingEventRouter(profile_));
  push_messaging_event_router_->Init();
  media_galleries_private_event_router_.reset(
      new extensions::MediaGalleriesPrivateEventRouter(profile_));

#if defined(OS_CHROMEOS)
  FileBrowserEventRouterFactory::GetForProfile(
      profile_)->ObserveFileSystemEvents();

  bluetooth_event_router_.reset(
      new chromeos::ExtensionBluetoothEventRouter(profile_));

  input_method_event_router_.reset(
      new chromeos::ExtensionInputMethodEventRouter);

  ExtensionMediaPlayerEventRouter::GetInstance()->Init(profile_);
  extensions::InputImeEventRouter::GetInstance()->Init();
#endif  // defined(OS_CHROMEOS)
#endif  // defined(ENABLE_EXTENSIONS)
  event_routers_initialized_ = true;
}

void ExtensionService::Shutdown() {
  if (push_messaging_event_router_.get())
    push_messaging_event_router_->Shutdown();
}

const Extension* ExtensionService::GetExtensionById(
    const std::string& id, bool include_disabled) const {
  int include_mask = INCLUDE_ENABLED;
  if (include_disabled)
    include_mask |= INCLUDE_DISABLED;
  return GetExtensionByIdInternal(id, include_mask);
}

void ExtensionService::Init() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK(!ready_);  // Can't redo init.
  DCHECK_EQ(extensions_.size(), 0u);

  component_loader_->LoadAll();
  extensions::InstalledLoader(this).LoadAllExtensions();

  // If we are running in the import process, don't bother initializing the
  // extension service since this can interfere with the main browser process
  // that is already running an extension service for this profile.
  // TODO(aa): can we start up even less of ExtensionService?
  // http://crbug.com/107636
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kImport) &&
      !CommandLine::ForCurrentProcess()->HasSwitch(switches::kImportFromFile)) {
    if (g_browser_process->profile_manager() &&
        g_browser_process->profile_manager()->will_import()) {
      RegisterForImportFinished();
    } else {
      // TODO(erikkay) this should probably be deferred to a future point
      // rather than running immediately at startup.
      CheckForExternalUpdates();

      // TODO(erikkay) this should probably be deferred as well.
      GarbageCollectExtensions();
    }
  }
}

bool ExtensionService::UpdateExtension(const std::string& id,
                                       const FilePath& extension_path,
                                       const GURL& download_url,
                                       CrxInstaller** out_crx_installer) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const extensions::PendingExtensionInfo* pending_extension_info =
      pending_extension_manager()->GetById(id);

  int include_mask = INCLUDE_ENABLED | INCLUDE_DISABLED;
  const Extension* extension =
      GetExtensionByIdInternal(id, include_mask);
  if (!pending_extension_info && !extension) {
    LOG(WARNING) << "Will not update extension " << id
                 << " because it is not installed or pending";
    // Delete extension_path since we're not creating a CrxInstaller
    // that would do it for us.
    if (!BrowserThread::PostTask(
            BrowserThread::FILE, FROM_HERE,
            base::Bind(
                &extension_file_util::DeleteFile, extension_path, false)))
      NOTREACHED();

    return false;
  }

  // We want a silent install only for non-pending extensions and
  // pending extensions that have install_silently set.
  ExtensionInstallPrompt* client =
      (!pending_extension_info || pending_extension_info->install_silently()) ?
      NULL : ExtensionInstallUI::CreateInstallPromptWithProfile(profile_);

  scoped_refptr<CrxInstaller> installer(CrxInstaller::Create(this, client));
  installer->set_expected_id(id);
  if (pending_extension_info) {
    installer->set_install_source(pending_extension_info->install_source());
    if (pending_extension_info->install_silently())
      installer->set_allow_silent_install(true);
  } else if (extension) {
    installer->set_install_source(extension->location());
  }
  // If the extension was installed from or has migrated to the webstore, or
  // if the extension came from sync and its auto-update URL is from the
  // webstore, treat it as a webstore install. Note that we ignore some older
  // extensions with blank auto-update URLs because we are mostly concerned
  // with restrictions on NaCl extensions, which are newer.
  int creation_flags = Extension::NO_FLAGS;
  if ((extension && extension->from_webstore()) ||
      (extension && extension->UpdatesFromGallery()) ||
      (!extension && pending_extension_info->is_from_sync() &&
       extension_urls::IsWebstoreUpdateUrl(
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
    *out_crx_installer = installer;

  return true;
}

void ExtensionService::ReloadExtension(const std::string& extension_id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FilePath path;
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
    if (host && DevToolsAgentHostRegistry::HasDevToolsAgentHost(
            host->render_view_host())) {
      // Look for an open inspector for the background page.
      DevToolsAgentHost* agent =
          DevToolsAgentHostRegistry::GetDevToolsAgentHost(
              host->render_view_host());
      int devtools_cookie =
          content::DevToolsManager::GetInstance()->DetachClientHost(agent);
      if (devtools_cookie >= 0)
        orphaned_dev_tools_[extension_id] = devtools_cookie;
    }

    if (current_extension->is_platform_app() &&
        !extensions::ShellWindowRegistry::Get(profile_)->
            GetShellWindowsForApp(extension_id).empty()) {
      relaunch_app_ids_.insert(extension_id);
    }

    path = current_extension->path();
    DisableExtension(extension_id, Extension::DISABLE_RELOAD);
    disabled_extension_paths_[extension_id] = path;
  } else {
    path = unloaded_extension_paths_[extension_id];
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
    extensions::UnpackedInstaller::Create(this)->Load(path);
  }
}

bool ExtensionService::UninstallExtension(
    std::string extension_id,
    bool external_uninstall,
    string16* error) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_refptr<const Extension> extension(GetInstalledExtension(extension_id));

  // Callers should not send us nonexistent extensions.
  CHECK(extension);

  // Policy change which triggers an uninstall will always set
  // |external_uninstall| to true so this is the only way to uninstall
  // managed extensions.
  if (!external_uninstall &&
      !system_->management_policy()->UserMayModifySettings(
        extension.get(), error)) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_UNINSTALL_NOT_ALLOWED,
        content::Source<Profile>(profile_),
        content::Details<const Extension>(extension));
    return false;
  }

  // Extract the data we need for sync now, but don't actually sync until we've
  // completed the uninstallation.
  syncer::SyncChange sync_change;
  if (app_sync_bundle_.HandlesApp(*extension)) {
    sync_change = app_sync_bundle_.CreateSyncChangeToDelete(extension);
  } else if (extension_sync_bundle_.HandlesExtension(*extension)) {
    sync_change = extension_sync_bundle_.CreateSyncChangeToDelete(extension);
  }

  UMA_HISTOGRAM_ENUMERATION("Extensions.UninstallType",
                            extension->GetType(), 100);
  RecordPermissionMessagesHistogram(
      extension, "Extensions.Permissions_Uninstall");

  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (url_service)
    url_service->UnregisterExtensionKeyword(extension);

  // Unload before doing more cleanup to ensure that nothing is hanging on to
  // any of these resources.
  UnloadExtension(extension_id, extension_misc::UNLOAD_REASON_UNINSTALL);

  extension_prefs_->OnExtensionUninstalled(extension_id, extension->location(),
                                           external_uninstall);

  // Tell the backend to start deleting installed extensions on the file thread.
  if (Extension::LOAD != extension->location()) {
    if (!BrowserThread::PostTask(
            BrowserThread::FILE, FROM_HERE,
            base::Bind(
                &extension_file_util::UninstallExtension,
                install_directory_,
                extension_id)))
      NOTREACHED();
  }

  GURL launch_web_url_origin(extension->launch_web_url());
  launch_web_url_origin = launch_web_url_origin.GetOrigin();
  bool is_storage_isolated = extension->is_storage_isolated();

  if (extension->is_hosted_app() &&
      !profile_->GetExtensionSpecialStoragePolicy()->
          IsStorageProtected(launch_web_url_origin)) {
    extensions::DataDeleter::StartDeleting(
        profile_, extension_id, launch_web_url_origin, is_storage_isolated);
  }
  extensions::DataDeleter::StartDeleting(
      profile_, extension_id, extension->url(), is_storage_isolated);

  UntrackTerminatedExtension(extension_id);

  // Notify interested parties that we've uninstalled this extension.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
      content::Source<Profile>(profile_),
      content::Details<const Extension>(extension));

  if (app_sync_bundle_.HasExtensionId(extension_id) &&
      sync_change.sync_data().GetDataType() == syncer::APPS) {
    app_sync_bundle_.ProcessDeletion(extension_id, sync_change);
  } else if (extension_sync_bundle_.HasExtensionId(extension_id) &&
             sync_change.sync_data().GetDataType() == syncer::EXTENSIONS) {
    extension_sync_bundle_.ProcessDeletion(extension_id, sync_change);
  }

  // Track the uninstallation.
  UMA_HISTOGRAM_ENUMERATION("Extensions.ExtensionUninstalled", 1, 2);

  static bool default_apps_trial_exists =
      base::FieldTrialList::TrialExists(kDefaultAppsTrialName);
  if (default_apps_trial_exists) {
    UMA_HISTOGRAM_ENUMERATION(
        base::FieldTrial::MakeName("Extensions.ExtensionUninstalled",
                                   kDefaultAppsTrialName),
        1, 2);
  }

  // Uninstalling one extension might have solved the problems of others.
  // Therefore, we clear warnings of this type for all extensions.
  std::set<ExtensionWarningSet::WarningType> warnings;
  extension_warnings_.GetWarningsAffectingExtension(extension_id, &warnings);
  extension_warnings_.ClearWarnings(warnings);

  return true;
}

bool ExtensionService::IsExtensionEnabled(
    const std::string& extension_id) const {
  if (extensions_.Contains(extension_id) ||
      terminated_extensions_.Contains(extension_id)) {
    return true;
  }

  if (disabled_extensions_.Contains(extension_id))
    return false;

  // If the extension hasn't been loaded yet, check the prefs for it. Assume
  // enabled unless otherwise noted.
  return !extension_prefs_->IsExtensionDisabled(extension_id) &&
      !extension_prefs_->IsExternalExtensionUninstalled(extension_id);
}

bool ExtensionService::IsExternalExtensionUninstalled(
    const std::string& extension_id) const {
  return extension_prefs_->IsExternalExtensionUninstalled(extension_id);
}

void ExtensionService::EnableExtension(const std::string& extension_id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (IsExtensionEnabled(extension_id))
    return;

  extension_prefs_->SetExtensionState(extension_id, Extension::ENABLED);
  extension_prefs_->ClearDisableReasons(extension_id);

  const Extension* extension = GetExtensionByIdInternal(extension_id,
      INCLUDE_DISABLED);
  // This can happen if sync enables an extension that is not
  // installed yet.
  if (!extension)
    return;

  // Move it over to the enabled list.
  extensions_.Insert(make_scoped_refptr(extension));
  disabled_extensions_.Remove(extension->id());

  // Make sure any browser action contained within it is not hidden.
  extension_prefs_->SetBrowserActionVisibility(extension, true);

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
      !system_->management_policy()->UserMayModifySettings(extension, NULL)) {
    return;
  }

  extension_prefs_->SetExtensionState(extension_id, Extension::DISABLED);
  extension_prefs_->AddDisableReason(extension_id, disable_reason);

  int include_mask = INCLUDE_ENABLED | INCLUDE_TERMINATED;
  extension = GetExtensionByIdInternal(extension_id, include_mask);
  if (!extension)
    return;

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

  // Deactivating one extension might have solved the problems of others.
  // Therefore, we clear warnings of this type for all extensions.
  std::set<ExtensionWarningSet::WarningType> warnings;
  extension_warnings_.GetWarningsAffectingExtension(extension_id, &warnings);
  extension_warnings_.ClearWarnings(warnings);
}

void ExtensionService::GrantPermissionsAndEnableExtension(
    const Extension* extension, bool record_oauth2_grant) {
  GrantPermissions(extension, record_oauth2_grant);
  RecordPermissionMessagesHistogram(
      extension, "Extensions.Permissions_ReEnable");
  extension_prefs_->SetDidExtensionEscalatePermissions(extension, false);
  EnableExtension(extension->id());
}

void ExtensionService::GrantPermissions(const Extension* extension,
                                        bool record_oauth2_grant) {
  CHECK(extension);
  extensions::PermissionsUpdater perms_updater(profile());
  perms_updater.GrantActivePermissions(extension, record_oauth2_grant);
}

// static
void ExtensionService::RecordPermissionMessagesHistogram(
    const Extension* e, const char* histogram) {
  // Since this is called from multiple sources, and since the Histogram macros
  // use statics, we need to manually lookup the Histogram ourselves.
  base::Histogram* counter = base::LinearHistogram::FactoryGet(
      histogram,
      1,
      PermissionMessage::kEnumBoundary,
      PermissionMessage::kEnumBoundary + 1,
      base::Histogram::kUmaTargetedHistogramFlag);

  PermissionMessages permissions = e->GetPermissionMessages();
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

  ExtensionWebUI::RegisterChromeURLOverrides(
      profile_, extension->GetChromeURLOverrides());

  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (url_service)
    url_service->RegisterExtensionKeyword(extension);

  // Load the icon for omnibox-enabled extensions so it will be ready to display
  // in the URL bar.
  if (!extension->omnibox_keyword().empty()) {
    omnibox_popup_icon_manager_.LoadIcon(extension);
    omnibox_icon_manager_.LoadIcon(extension);
  }

  // If the extension has permission to load chrome://favicon/ resources we need
  // to make sure that the FaviconSource is registered with the
  // ChromeURLDataManager.
  if (extension->HasHostPermission(GURL(chrome::kChromeUIFaviconURL))) {
    FaviconSource* favicon_source = new FaviconSource(profile_,
                                                      FaviconSource::FAVICON);
    ChromeURLDataManager::AddDataSource(profile_, favicon_source);
  }
  // Same for chrome://thumb/ resources.
  if (extension->HasHostPermission(GURL(chrome::kChromeUIThumbnailURL))) {
    ThumbnailSource* thumbnail_source = new ThumbnailSource(profile_);
    ChromeURLDataManagerFactory::GetForProfile(profile_)->
        AddDataSource(thumbnail_source);
  }

  // TODO(mpcomplete): This ends up affecting all profiles. See crbug.com/80757.
  bool plugins_changed = false;
  for (size_t i = 0; i < extension->plugins().size(); ++i) {
    const Extension::PluginInfo& plugin = extension->plugins()[i];
    PluginService::GetInstance()->RefreshPlugins();
    PluginService::GetInstance()->AddExtraPluginPath(plugin.path);
    plugins_changed = true;
    ChromePluginServiceFilter* filter =
        ChromePluginServiceFilter::GetInstance();
    if (plugin.is_public) {
      filter->RestrictPluginToProfileAndOrigin(
          plugin.path, profile_, GURL());
    } else {
      filter->RestrictPluginToProfileAndOrigin(
          plugin.path, profile_, extension->url());
    }
  }

  bool nacl_modules_changed = false;
  for (size_t i = 0; i < extension->nacl_modules().size(); ++i) {
    const Extension::NaClModuleInfo& module = extension->nacl_modules()[i];
    RegisterNaClModule(module.url, module.mime_type);
    nacl_modules_changed = true;
  }

  if (nacl_modules_changed)
    UpdatePluginListWithNaClModules();

  if (plugins_changed || nacl_modules_changed)
    PluginService::GetInstance()->PurgePluginListCache(profile_, false);

#if defined(OS_CHROMEOS)
  for (std::vector<Extension::InputComponentInfo>::const_iterator component =
           extension->input_components().begin();
       component != extension->input_components().end();
       ++component) {
    if (component->type == Extension::INPUT_COMPONENT_TYPE_IME) {
      extensions::InputImeEventRouter::GetInstance()->RegisterIme(
          profile_, extension->id(), *component);
    }
  }
#endif
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

  ExtensionWebUI::UnregisterChromeURLOverrides(
      profile_, extension->GetChromeURLOverrides());

#if defined(OS_CHROMEOS)
    // Revoke external file access to
  if (BrowserContext::GetFileSystemContext(profile_) &&
      BrowserContext::GetFileSystemContext(profile_)->external_provider()) {
    BrowserContext::GetFileSystemContext(profile_)->external_provider()->
        RevokeAccessForExtension(extension->id());
  }

  if (extension->input_components().size() > 0) {
    extensions::InputImeEventRouter::GetInstance()->UnregisterAllImes(
        profile_, extension->id());
  }
#endif

  UpdateActiveExtensionsInCrashReporter();

  bool plugins_changed = false;
  for (size_t i = 0; i < extension->plugins().size(); ++i) {
    const Extension::PluginInfo& plugin = extension->plugins()[i];
    PluginService::GetInstance()->ForcePluginShutdown(plugin.path);
    PluginService::GetInstance()->RefreshPlugins();
    PluginService::GetInstance()->RemoveExtraPluginPath(plugin.path);
    plugins_changed = true;
    ChromePluginServiceFilter::GetInstance()->UnrestrictPlugin(plugin.path);
  }

  bool nacl_modules_changed = false;
  for (size_t i = 0; i < extension->nacl_modules().size(); ++i) {
    const Extension::NaClModuleInfo& module = extension->nacl_modules()[i];
    UnregisterNaClModule(module.url);
    nacl_modules_changed = true;
  }

  if (nacl_modules_changed)
    UpdatePluginListWithNaClModules();

  if (plugins_changed || nacl_modules_changed)
    PluginService::GetInstance()->PurgePluginListCache(profile_, false);
}

void ExtensionService::UpdateExtensionBlacklist(
  const std::vector<std::string>& blacklist) {
  // Use this set to indicate if an extension in the blacklist has been used.
  std::set<std::string> blacklist_set;
  for (unsigned int i = 0; i < blacklist.size(); ++i) {
    if (Extension::IdIsValid(blacklist[i]))
      blacklist_set.insert(blacklist[i]);
  }
  extension_prefs_->UpdateBlacklist(blacklist_set);
  CheckManagementPolicy();
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
  return ready_;
}

extensions::ExtensionUpdater* ExtensionService::updater() {
  return updater_.get();
}

void ExtensionService::CheckManagementPolicy() {
  std::vector<std::string> to_be_removed;
  // Loop through extensions list, unload installed extensions.
  for (ExtensionSet::const_iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    const Extension* extension = (*iter);
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

syncer::SyncError ExtensionService::MergeDataAndStartSyncing(
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

  return syncer::SyncError();
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
      extension_prefs_->GetAppNotificationClientId(extension.id()),
      extension_prefs_->IsAppNotificationDisabled(extension.id()),
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
  const Extension* extension = GetInstalledExtension(id);
  bool extension_installed = (extension != NULL);

  if (app_sync_data.app_launch_ordinal().IsValid() &&
      app_sync_data.page_ordinal().IsValid()) {
    extension_prefs_->extension_sorting()->SetAppLaunchOrdinal(
        id,
        app_sync_data.app_launch_ordinal());
    extension_prefs_->extension_sorting()->SetPageOrdinal(
        id,
        app_sync_data.page_ordinal());
  }

  if (extension_installed) {
    if (app_sync_data.notifications_disabled() !=
        extension_prefs_->IsAppNotificationDisabled(id)) {
      extension_prefs_->SetAppNotificationDisabled(
          id, app_sync_data.notifications_disabled());
    }
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
      extension.GetSyncType() == Extension::SYNC_TYPE_EXTENSION) {
    return true;
  }

  if (type == syncer::APPS &&
      extension.GetSyncType() == Extension::SYNC_TYPE_APP) {
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
    std::string error;
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
  if (extension_sync_data.enabled())
    EnableExtension(id);
  else
    DisableExtension(id, Extension::DISABLE_USER_ACTION);

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
    ExtensionFilter filter =
        (type == syncer::APPS) ? IsSyncableApp : IsSyncableExtension;

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
  // If this is an existing component extension we always allow it to
  // work in incognito mode.
  const Extension* extension = GetInstalledExtension(extension_id);
  if (extension && extension->location() == Extension::COMPONENT)
    return true;

  // Check the prefs.
  return extension_prefs_->IsIncognitoEnabled(extension_id);
}

void ExtensionService::SetIsIncognitoEnabled(
    const std::string& extension_id, bool enabled) {
  const Extension* extension = GetInstalledExtension(extension_id);
  if (extension && extension->location() == Extension::COMPONENT) {
    // This shouldn't be called for component extensions unless they are
    // syncable.
    DCHECK(extension->IsSyncable());

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

void ExtensionService::SetAppNotificationSetupDone(
    const std::string& extension_id,
    const std::string& oauth_client_id) {
  const Extension* extension = GetInstalledExtension(extension_id);
  // This method is called when the user sets up app notifications.
  // So it is not expected to be called until the extension is installed.
  if (!extension) {
    NOTREACHED();
    return;
  }
  extension_prefs_->SetAppNotificationClientId(extension_id, oauth_client_id);
  SyncExtensionChangeIfNeeded(*extension);
}

void ExtensionService::SetAppNotificationDisabled(
    const std::string& extension_id,
    bool value) {
  const Extension* extension = GetInstalledExtension(extension_id);
  // This method is called when the user enables/disables app notifications.
  // So it is not expected to be called until the extension is installed.
  if (!extension) {
    NOTREACHED();
    return;
  }
  if (value)
    UMA_HISTOGRAM_COUNTS("Apps.SetAppNotificationsDisabled", 1);
  else
    UMA_HISTOGRAM_COUNTS("Apps.SetAppNotificationsEnabled", 1);
  extension_prefs_->SetAppNotificationDisabled(extension_id, value);
  SyncExtensionChangeIfNeeded(*extension);
}

bool ExtensionService::CanCrossIncognito(const Extension* extension) const {
  // We allow the extension to see events and data from another profile iff it
  // uses "spanning" behavior and it has incognito access. "split" mode
  // extensions only see events for a matching profile.
  CHECK(extension);
  return IsIncognitoEnabled(extension->id()) &&
      !extension->incognito_split_mode();
}

bool ExtensionService::CanLoadInIncognito(const Extension* extension) const {
  if (extension->is_hosted_app())
    return true;
  // Packaged apps and regular extensions need to be enabled specifically for
  // incognito (and split mode should be set).
  return extension->incognito_split_mode() &&
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
    updater()->CheckNow();
  }

  // Uninstall all the unclaimed extensions.
  scoped_ptr<extensions::ExtensionPrefs::ExtensionsInfo> extensions_info(
      extension_prefs_->GetInstalledExtensionsInfo());
  for (size_t i = 0; i < extensions_info->size(); ++i) {
    ExtensionInfo* info = extensions_info->at(i).get();
    if (Extension::IsExternalLocation(info->extension_location))
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
    if (extension_prefs_->SetAlertSystemFirstRun()) {
      CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
      did_show_alert = extension_error_ui_->ShowErrorInBubbleView();
    } else {
      // First run. Just acknowledge all the extensions, silently, by
      // shortcutting the display of the UI and going straight to the
      // callback for pressing the Accept button.
      HandleExtensionAlertAccept();
    }
  }

  if (!did_show_alert)
    extension_error_ui_.reset();
}

bool ExtensionService::PopulateExtensionErrorUI(
    ExtensionErrorUI* extension_error_ui) {
  bool needs_alert = false;
  for (ExtensionSet::const_iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    const Extension* e = *iter;
    if (Extension::IsExternalLocation(e->location())) {
      if (!e->is_hosted_app()) {
        if (!extension_prefs_->IsExternalExtensionAcknowledged(e->id())) {
          extension_error_ui->AddExternalExtension(e->id());
          needs_alert = true;
        }
      }
    }
    if (!extension_prefs_->UserMayLoad(e, NULL)) {
      if (!extension_prefs_->IsBlacklistedExtensionAcknowledged(e->id())) {
        extension_error_ui->AddBlacklistedExtension(e->id());
        needs_alert = true;
      }
    }
    if (extension_prefs_->IsExtensionOrphaned(e->id())) {
      if (!extension_prefs_->IsOrphanedExtensionAcknowledged(e->id())) {
        extension_error_ui->AddOrphanedExtension(e->id());
        needs_alert = true;
      }
    }
  }
  return needs_alert;
}

void ExtensionService::HandleExtensionAlertClosed() {
  extension_error_ui_.reset();
}

void ExtensionService::HandleExtensionAlertAccept() {
  const ExtensionIdSet *extension_ids =
      extension_error_ui_->get_external_extension_ids();
  for (ExtensionIdSet::const_iterator iter = extension_ids->begin();
       iter != extension_ids->end(); ++iter) {
    AcknowledgeExternalExtension(*iter);
  }
  extension_ids = extension_error_ui_->get_blacklisted_extension_ids();
  for (ExtensionIdSet::const_iterator iter = extension_ids->begin();
       iter != extension_ids->end(); ++iter) {
    extension_prefs_->AcknowledgeBlacklistedExtension(*iter);
  }
  extension_ids = extension_error_ui_->get_orphaned_extension_ids();
  for (ExtensionIdSet::const_iterator iter = extension_ids->begin();
       iter != extension_ids->end(); ++iter) {
    extension_prefs_->AcknowledgeOrphanedExtension(*iter);
  }
}

void ExtensionService::AcknowledgeExternalExtension(const std::string& id) {
  extension_prefs_->AcknowledgeExternalExtension(id);
}

void ExtensionService::HandleExtensionAlertDetails() {
  extension_error_ui_->ShowExtensions();
}

void ExtensionService::UnloadExtension(
    const std::string& extension_id,
    extension_misc::UnloadedExtensionReason reason) {
  // Make sure the extension gets deleted after we return from this function.
  int include_mask = INCLUDE_ENABLED | INCLUDE_DISABLED;
  scoped_refptr<const Extension> extension(
      GetExtensionByIdInternal(extension_id, include_mask));

  // This method can be called via PostTask, so the extension may have been
  // unloaded by the time this runs.
  if (!extension) {
    // In case the extension may have crashed/uninstalled. Allow the profile to
    // clean up its RequestContexts.
    system_->UnregisterExtensionWithRequestContexts(extension_id, reason);
    return;
  }

  // Keep information about the extension so that we can reload it later
  // even if it's not permanently installed.
  unloaded_extension_paths_[extension->id()] = extension->path();

  // Clean up if the extension is meant to be enabled after a reload.
  disabled_extension_paths_.erase(extension->id());

  // Clean up runtime data.
  extension_runtime_data_.erase(extension_id);

  if (disabled_extensions_.Contains(extension->id())) {
    UnloadedExtensionInfo details(extension, reason);
    details.already_disabled = true;
    disabled_extensions_.Remove(extension->id());
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_UNLOADED,
        content::Source<Profile>(profile_),
        content::Details<UnloadedExtensionInfo>(&details));
    // Make sure the profile cleans up its RequestContexts when an already
    // disabled extension is unloaded (since they are also tracking the disabled
    // extensions).
    system_->UnregisterExtensionWithRequestContexts(extension_id, reason);
    return;
  }

  // Remove the extension from our list.
  extensions_.Remove(extension->id());

  NotifyExtensionUnloaded(extension.get(), reason);
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
}

void ExtensionService::GarbageCollectExtensions() {
  if (extension_prefs_->pref_service()->ReadOnly())
    return;

  scoped_ptr<extensions::ExtensionPrefs::ExtensionsInfo> info(
      extension_prefs_->GetInstalledExtensionsInfo());

  std::map<std::string, FilePath> extension_paths;
  for (size_t i = 0; i < info->size(); ++i)
    extension_paths[info->at(i)->extension_id] = info->at(i)->extension_path;

  if (!BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(
              &extension_file_util::GarbageCollectExtensions,
              install_directory_,
              extension_paths))) {
    NOTREACHED();
  }

#if defined(ENABLE_THEMES)
  // Also garbage-collect themes.  We check |profile_| to be
  // defensive; in the future, we may call GarbageCollectExtensions()
  // from somewhere other than Init() (e.g., in a timer).
  if (profile_) {
    ThemeServiceFactory::GetForProfile(profile_)->RemoveUnusedThemes();
  }
#endif
}

void ExtensionService::SyncExtensionChangeIfNeeded(const Extension& extension) {
  if (app_sync_bundle_.HandlesApp(extension)) {
    app_sync_bundle_.SyncChangeIfNeeded(extension);
  } else if (extension_sync_bundle_.HandlesExtension(extension)) {
    extension_sync_bundle_.SyncChangeIfNeeded(extension);
  }
}

void ExtensionService::OnLoadedInstalledExtensions() {
  if (updater_.get())
    updater_->Start();

  ready_ = true;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSIONS_READY,
      content::Source<Profile>(profile_),
      content::NotificationService::NoDetails());
}

void ExtensionService::AddExtension(const Extension* extension) {
  // Ensure extension is deleted unless we transfer ownership.
  scoped_refptr<const Extension> scoped_extension(extension);

  // TODO(jstritar): We may be able to get rid of this branch by overriding the
  // default extension state to DISABLED when the --disable-extensions flag
  // is set (http://crbug.com/29067).
  if (!extensions_enabled() &&
      !extension->is_theme() &&
      extension->location() != Extension::COMPONENT &&
      !Extension::IsExternalLocation(extension->location())) {
    return;
  }

  SetBeingUpgraded(extension, false);

  // The extension is now loaded, remove its data from unloaded extension map.
  unloaded_extension_paths_.erase(extension->id());

  // If a terminated extension is loaded, remove it from the terminated list.
  UntrackTerminatedExtension(extension->id());

  // If the extension was disabled for a reload, then enable it.
  if (disabled_extension_paths_.erase(extension->id()) > 0)
    EnableExtension(extension->id());

  // Check if the extension's privileges have changed and disable the
  // extension if necessary.
  InitializePermissions(extension);

  if (extension_prefs_->IsExtensionDisabled(extension->id())) {
    disabled_extensions_.Insert(scoped_extension);
    SyncExtensionChangeIfNeeded(*extension);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_UPDATE_DISABLED,
        content::Source<Profile>(profile_),
        content::Details<const Extension>(extension));

    if (extension_prefs_->GetDisableReasons(extension->id()) &
        Extension::DISABLE_PERMISSIONS_INCREASE) {
      extensions::AddExtensionDisabledError(this, extension);
    }
    return;
  }

  // All apps that are displayed in the launcher are ordered by their ordinals
  // so we must ensure they have valid ordinals.
  if (extension->ShouldDisplayInLauncher())
    extension_prefs_->extension_sorting()->EnsureValidOrdinals(extension->id());

  extensions_.Insert(scoped_extension);
  SyncExtensionChangeIfNeeded(*extension);
  NotifyExtensionLoaded(extension);
  QueueRestoreAppWindow(extension);
}

void ExtensionService::InitializePermissions(const Extension* extension) {
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
            extension->required_permission_set(),
            extension->optional_permission_set());

    // Make sure the active permissions contain no more than optional + default.
    scoped_refptr<PermissionSet> adjusted_active =
        PermissionSet::CreateIntersection(
            total_permissions.get(), active_permissions.get());

    // Make sure the active permissions contain the default permissions.
    adjusted_active = PermissionSet::CreateUnion(
            extension->required_permission_set(), adjusted_active.get());

    extensions::PermissionsUpdater perms_updater(profile());
    perms_updater.UpdateActivePermissions(extension, adjusted_active);
  }

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
  int include_mask = INCLUDE_ENABLED | INCLUDE_DISABLED;
  const Extension* old =
      GetExtensionByIdInternal(extension->id(), include_mask);
  bool is_extension_upgrade = old != NULL;
  bool is_privilege_increase = false;
  bool previously_disabled = false;
  int disable_reasons = extension_prefs_->GetDisableReasons(extension->id());

  // We only need to compare the granted permissions to the current permissions
  // if the extension is not allowed to silently increase its permissions.
  bool is_default_app_install =
      (!is_extension_upgrade && extension->was_installed_by_default());
  if (!(extension->CanSilentlyIncreasePermissions()
        || is_default_app_install)) {
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
        granted_permissions->HasLessPrivilegesThan(
            extension->GetActivePermissions());
  }

  // Silently grant all active permissions to default apps only on install.
  // After install they should behave like other apps.
  if (is_default_app_install)
    GrantPermissions(extension, true);

  if (is_extension_upgrade) {
    // Other than for unpacked extensions, CrxInstaller should have guaranteed
    // that we aren't downgrading.
    if (extension->location() != Extension::LOAD)
      CHECK_GE(extension->version()->CompareTo(*(old->version())), 0);

    // Extensions get upgraded if the privileges are allowed to increase or
    // the privileges haven't increased.
    if (!is_privilege_increase) {
      SetBeingUpgraded(old, true);
      SetBeingUpgraded(extension, true);
    }

    // If the extension was already disabled, suppress any alerts for becoming
    // disabled on permissions increase.
    previously_disabled = extension_prefs_->IsExtensionDisabled(old->id());
    if (previously_disabled) {
      int reasons = extension_prefs_->GetDisableReasons(old->id());
      if (reasons == Extension::DISABLE_NONE) {
        // Initialize the reason for legacy disabled extensions from whether the
        // extension already exceeded granted permissions.
        if (extension_prefs_->DidExtensionEscalatePermissions(old->id()))
          disable_reasons = Extension::DISABLE_PERMISSIONS_INCREASE;
        else
          disable_reasons = Extension::DISABLE_USER_ACTION;
      }
    } else {
      disable_reasons = Extension::DISABLE_PERMISSIONS_INCREASE;
    }

    // To upgrade an extension in place, unload the old one and
    // then load the new one.
    UnloadExtension(old->id(), extension_misc::UNLOAD_REASON_UPDATE);
    old = NULL;
  }

  // Extension has changed permissions significantly. Disable it. A
  // notification should be sent by the caller.
  if (is_privilege_increase) {
    if (!extension_prefs_->DidExtensionEscalatePermissions(extension->id())) {
      RecordPermissionMessagesHistogram(
          extension, "Extensions.Permissions_AutoDisable");
    }
    extension_prefs_->SetExtensionState(extension->id(), Extension::DISABLED);
    extension_prefs_->SetDidExtensionEscalatePermissions(extension, true);
    extension_prefs_->AddDisableReason(
        extension->id(),
        static_cast<Extension::DisableReason>(disable_reasons));
  }
}

void ExtensionService::UpdateActiveExtensionsInCrashReporter() {
  std::set<std::string> extension_ids;
  for (ExtensionSet::const_iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    const Extension* extension = *iter;
    if (!extension->is_theme() && extension->location() != Extension::COMPONENT)
      extension_ids.insert(extension->id());
  }

  child_process_logging::SetActiveExtensions(extension_ids);
}

void ExtensionService::OnExtensionInstalled(
    const Extension* extension,
    bool from_webstore,
    const syncer::StringOrdinal& page_ordinal,
    bool has_requirement_errors) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Ensure extension is deleted unless we transfer ownership.
  scoped_refptr<const Extension> scoped_extension(extension);
  const std::string& id = extension->id();
  // Extensions installed by policy can't be disabled. So even if a previous
  // installation disabled the extension, make sure it is now enabled.
  bool initial_enable =
      !extension_prefs_->IsExtensionDisabled(id) ||
      system_->management_policy()->MustRemainEnabled(extension, NULL);
  const extensions::PendingExtensionInfo* pending_extension_info = NULL;
  if ((pending_extension_info = pending_extension_manager()->GetById(id))) {
    if (!pending_extension_info->ShouldAllowInstall(*extension)) {
      pending_extension_manager()->Remove(id);

      LOG(WARNING) << "ShouldAllowInstall() returned false for "
                   << id << " of type " << extension->GetType()
                   << " and update URL " << extension->update_url().spec()
                   << "; not installing";

      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_EXTENSION_INSTALL_NOT_ALLOWED,
          content::Source<Profile>(profile_),
          content::Details<const Extension>(extension));

      // Delete the extension directory since we're not going to
      // load it.
      if (!BrowserThread::PostTask(
              BrowserThread::FILE, FROM_HERE,
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

  int include_mask = INCLUDE_ENABLED | INCLUDE_DISABLED;
  // Do not record the install histograms for upgrades.
  if (!GetExtensionByIdInternal(extension->id(), include_mask)) {
    UMA_HISTOGRAM_ENUMERATION("Extensions.InstallType",
                              extension->GetType(), 100);
    RecordPermissionMessagesHistogram(
        extension, "Extensions.Permissions_Install");
  }

  // Certain extension locations are specific enough that we can
  // auto-acknowledge any extension that came from one of them.
  if (extension->location() == Extension::EXTERNAL_POLICY_DOWNLOAD)
    AcknowledgeExternalExtension(extension->id());

  extension_prefs_->OnExtensionInstalled(
      extension,
      initial_enable ? Extension::ENABLED : Extension::DISABLED,
      from_webstore,
      page_ordinal);

  // Unpacked extensions default to allowing file access, but if that has been
  // overridden, don't reset the value.
  if (Extension::ShouldAlwaysAllowFileAccess(extension->location()) &&
      !extension_prefs_->HasAllowFileAccessSetting(id)) {
    extension_prefs_->SetAllowFileAccess(id, true);
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_INSTALLED,
      content::Source<Profile>(profile_),
      content::Details<const Extension>(extension));

  // Transfer ownership of |extension| to AddExtension.
  AddExtension(scoped_extension);
}

const Extension* ExtensionService::GetExtensionByIdInternal(
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
  return NULL;
}

void ExtensionService::TrackTerminatedExtension(const Extension* extension) {
  if (!terminated_extensions_.Contains(extension->id()))
    terminated_extensions_.Insert(make_scoped_refptr(extension));

  UnloadExtension(extension->id(), extension_misc::UNLOAD_REASON_TERMINATE);
}

void ExtensionService::UntrackTerminatedExtension(const std::string& id) {
  std::string lowercase_id = StringToLowerASCII(id);
  terminated_extensions_.Remove(lowercase_id);
}

const Extension* ExtensionService::GetTerminatedExtension(
    const std::string& id) const {
  return GetExtensionByIdInternal(id, INCLUDE_TERMINATED);
}

const Extension* ExtensionService::GetInstalledExtension(
    const std::string& id) const {
  int include_mask = INCLUDE_ENABLED | INCLUDE_DISABLED | INCLUDE_TERMINATED;
  return GetExtensionByIdInternal(id, include_mask);
}

bool ExtensionService::ExtensionBindingsAllowed(const GURL& url) {
  // Allow bindings for all packaged extensions and component hosted apps.
  const Extension* extension = extensions_.GetExtensionOrAppByURL(
      ExtensionURLInfo(url));
  return extension && (!extension->is_hosted_app() ||
                       extension->location() == Extension::COMPONENT);
}

gfx::Image ExtensionService::GetOmniboxIcon(
    const std::string& extension_id) {
  return gfx::Image(omnibox_icon_manager_.GetIcon(extension_id));
}

gfx::Image ExtensionService::GetOmniboxPopupIcon(
    const std::string& extension_id) {
  return gfx::Image(omnibox_popup_icon_manager_.GetIcon(extension_id));
}

bool ExtensionService::OnExternalExtensionFileFound(
         const std::string& id,
         const Version* version,
         const FilePath& path,
         Extension::Location location,
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
        (location == Extension::INTERNAL &&
         Extension::IsExternalLocation(existing->location()));

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
  scoped_refptr<CrxInstaller> installer(CrxInstaller::Create(this, NULL));
  installer->set_install_source(location);
  installer->set_expected_id(id);
  installer->set_expected_version(*version);
  installer->set_install_cause(extension_misc::INSTALL_CAUSE_EXTERNAL_FILE);
  installer->set_creation_flags(creation_flags);
  installer->InstallCrx(path);

  // Depending on the source, a new external extension might not need a user
  // notification on installation. For such extensions, mark them acknowledged
  // now to suppress the notification.
  if (mark_acknowledged)
    AcknowledgeExternalExtension(id);

  return true;
}

void ExtensionService::ReportExtensionLoadError(
    const FilePath& extension_path,
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

  DevToolsAgentHost* agent = DevToolsAgentHostRegistry::GetDevToolsAgentHost(
      host->render_view_host());
  content::DevToolsManager::GetInstance()->AttachClientHost(iter->second,
                                                            agent);
  orphaned_dev_tools_.erase(iter);
}

void ExtensionService::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  switch (type) {
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
      MessageLoop::current()->PostTask(
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
          extensions::Feature::GetCurrentChannel()));

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
          loaded_extensions.push_back(ExtensionMsg_Loaded_Params(*iter));
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

      installed_app_hosts_.erase(process->GetID());

      process_map_.RemoveAllFromProcess(process->GetID());
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&ExtensionInfoMap::UnregisterAllExtensionsInProcess,
                     system_->info_map(),
                     process->GetID()));
      break;
    }
    case chrome::NOTIFICATION_PREF_CHANGED: {
      std::string* pref_name = content::Details<std::string>(details).ptr();
      if (*pref_name == prefs::kExtensionInstallAllowList ||
          *pref_name == prefs::kExtensionInstallDenyList) {
        IdentifyAlertableExtensions();
        CheckManagementPolicy();
      } else {
        NOTREACHED() << "Unexpected preference name.";
      }
      break;
    }
    case chrome::NOTIFICATION_IMPORT_FINISHED: {
      InitAfterImport();
      break;
    }

    default:
      NOTREACHED() << "Unexpected notification type.";
  }
}

bool ExtensionService::HasApps() const {
  return !GetAppIds().empty();
}

ExtensionIdSet ExtensionService::GetAppIds() const {
  ExtensionIdSet result;
  for (ExtensionSet::const_iterator it = extensions_.begin();
       it != extensions_.end(); ++it) {
    if ((*it)->is_app() && (*it)->location() != Extension::COMPONENT)
      result.insert((*it)->id());
  }

  return result;
}

bool ExtensionService::IsBackgroundPageReady(const Extension* extension) const {
  if (!extension->has_persistent_background_page())
    return true;
  ExtensionRuntimeDataMap::const_iterator it =
      extension_runtime_data_.find(extension->id());
  return it == extension_runtime_data_.end() ? false :
                                               it->second.background_page_ready;
}

void ExtensionService::SetBackgroundPageReady(const Extension* extension) {
  DCHECK(extension->has_background_page());
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

void ExtensionService::RegisterNaClModule(const GURL& url,
                                          const std::string& mime_type) {
  NaClModuleInfo info;
  info.url = url;
  info.mime_type = mime_type;

  DCHECK(FindNaClModule(url) == nacl_module_list_.end());
  nacl_module_list_.push_front(info);
}

void ExtensionService::UnregisterNaClModule(const GURL& url) {
  NaClModuleInfoList::iterator iter = FindNaClModule(url);
  DCHECK(iter != nacl_module_list_.end());
  nacl_module_list_.erase(iter);
}

void ExtensionService::UpdatePluginListWithNaClModules() {
  // An extension has been added which has a nacl_module component, which means
  // there is a MIME type that module wants to handle, so we need to add that
  // MIME type to plugins which handle NaCl modules in order to allow the
  // individual modules to handle these types.
  FilePath path;
  if (!PathService::Get(chrome::FILE_NACL_PLUGIN, &path))
    return;
  const content::PepperPluginInfo* pepper_info =
      PluginService::GetInstance()->GetRegisteredPpapiPluginInfo(path);
  if (!pepper_info)
    return;

  std::vector<webkit::WebPluginMimeType>::const_iterator mime_iter;
  // Check each MIME type the plugins handle for the NaCl MIME type.
  for (mime_iter = pepper_info->mime_types.begin();
       mime_iter != pepper_info->mime_types.end(); ++mime_iter) {
    if (mime_iter->mime_type == kNaClPluginMimeType) {
      // This plugin handles "application/x-nacl".

      PluginService::GetInstance()->
          UnregisterInternalPlugin(pepper_info->path);

      webkit::WebPluginInfo info = pepper_info->ToWebPluginInfo();

      for (ExtensionService::NaClModuleInfoList::const_iterator iter =
          nacl_module_list_.begin();
          iter != nacl_module_list_.end(); ++iter) {
        // Add the MIME type specified in the extension to this NaCl plugin,
        // With an extra "nacl" argument to specify the location of the NaCl
        // manifest file.
        webkit::WebPluginMimeType mime_type_info;
        mime_type_info.mime_type = iter->mime_type;
        mime_type_info.additional_param_names.push_back(UTF8ToUTF16("nacl"));
        mime_type_info.additional_param_values.push_back(
            UTF8ToUTF16(iter->url.spec()));
        info.mime_types.push_back(mime_type_info);
      }

      PluginService::GetInstance()->RefreshPlugins();
      PluginService::GetInstance()->RegisterInternalPlugin(info, true);
      // This plugin has been modified, no need to check the rest of its
      // types, but continue checking other plugins.
      break;
    }
  }
}

ExtensionService::NaClModuleInfoList::iterator
    ExtensionService::FindNaClModule(const GURL& url) {
  for (NaClModuleInfoList::iterator iter = nacl_module_list_.begin();
       iter != nacl_module_list_.end(); ++iter) {
    if (iter->url == url)
      return iter;
  }
  return nacl_module_list_.end();
}

void ExtensionService::QueueRestoreAppWindow(const Extension* extension) {
  std::set<std::string>::iterator relaunch_iter =
      relaunch_app_ids_.find(extension->id());
  if (relaunch_iter != relaunch_app_ids_.end()) {
    extensions::LazyBackgroundTaskQueue* queue =
        system_->lazy_background_task_queue();
    if (queue->ShouldEnqueueTask(profile(), extension)) {
      queue->AddPendingTask(profile(), extension->id(),
                            base::Bind(&ExtensionService::LaunchApplication));
    }

    relaunch_app_ids_.erase(relaunch_iter);
  }
}

// static
void ExtensionService::LaunchApplication(
    extensions::ExtensionHost* extension_host) {
  if (!extension_host)
    return;

#if !defined(OS_ANDROID)
  extensions::LaunchPlatformApp(extension_host->profile(),
                                extension_host->extension(),
                                NULL, FilePath());
#endif
}
