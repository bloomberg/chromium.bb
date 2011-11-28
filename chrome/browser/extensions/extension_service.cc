// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "base/stl_util.h"
#include "base/string16.h"
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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_plugin_service_filter.h"
#include "chrome/browser/download/download_extension_api.h"
#include "chrome/browser/extensions/app_notification_manager.h"
#include "chrome/browser/extensions/apps_promo.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/default_apps_trial.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_cookies_api.h"
#include "chrome/browser/extensions/extension_data_deleter.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_global_error.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_input_ime_api.h"
#include "chrome/browser/extensions/extension_management_api.h"
#include "chrome/browser/extensions/extension_preference_api.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_processes_api.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/extension_sync_data.h"
#include "chrome/browser/extensions/extension_updater.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/extension_webnavigation_api.h"
#include "chrome/browser/extensions/external_extension_provider_impl.h"
#include "chrome/browser/extensions/external_extension_provider_interface.h"
#include "chrome/browser/extensions/installed_loader.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/extensions/settings/settings_frontend.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/history/history_extension_api.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/api/sync_change.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/global_error_service.h"
#include "chrome/browser/ui/global_error_service_factory.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/ntp/thumbnail_source.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/debugger/devtools_manager.h"
#include "content/browser/plugin_process_host.h"
#include "content/browser/plugin_service.h"
#include "content/browser/user_metrics.h"
#include "content/common/pepper_plugin_registry.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "googleurl/src/gurl.h"
#include "net/base/registry_controlled_domain.h"
#include "webkit/database/database_tracker.h"
#include "webkit/database/database_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/extensions/file_browser_event_router.h"
#include "chrome/browser/chromeos/extensions/input_method_event_router.h"
#include "chrome/browser/chromeos/extensions/media_player_event_router.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/extensions/extension_input_ime_api.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_path_manager.h"
#endif

#if defined(OS_CHROMEOS) && defined(TOUCH_UI)
#include "chrome/browser/extensions/extension_input_ui_api.h"
#endif

using base::Time;
using content::BrowserThread;

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

static void ForceShutdownPlugin(const FilePath& plugin_path) {
  PluginProcessHost* plugin =
      PluginService::GetInstance()->FindNpapiPluginProcess(plugin_path);
  if (plugin)
    plugin->ForceShutdown();
}

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

const char* ExtensionService::kInstallDirectoryName = "Extensions";

const char* ExtensionService::kLocalAppSettingsDirectoryName =
    "Local App Settings";
const char* ExtensionService::kLocalExtensionSettingsDirectoryName =
    "Local Extension Settings";
const char* ExtensionService::kSyncAppSettingsDirectoryName =
    "Sync App Settings";
const char* ExtensionService::kSyncExtensionSettingsDirectoryName =
    "Sync Extension Settings";

void ExtensionService::CheckExternalUninstall(const std::string& id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Check if the providers know about this extension.
  ProviderCollection::const_iterator i;
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
  const Extension* extension = GetInstalledExtension(id);
  if (!extension) {
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
    ExternalExtensionProviderInterface* test_provider) {
  CHECK(test_provider);
  external_extension_providers_.push_back(
      linked_ptr<ExternalExtensionProviderInterface>(test_provider));
}

void ExtensionService::OnExternalExtensionUpdateUrlFound(
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
      return;
    // Otherwise, overwrite the current installation.
  }
  pending_extension_manager()->AddFromExternalUpdateUrl(
      id, update_url, location);
  external_extension_url_added_ = true;
}

// If a download url matches one of these patterns and has a referrer of the
// webstore, then we're willing to treat that as a gallery download.
static const char* kAllowedDownloadURLPatterns[] = {
  "https://clients2.google.com/service/update2*",
  "https://clients2.googleusercontent.com/crx/*"
};

bool ExtensionService::IsDownloadFromGallery(const GURL& download_url,
                                             const GURL& referrer_url) {
  const Extension* download_extension = GetExtensionByWebExtent(download_url);
  const Extension* referrer_extension = GetExtensionByWebExtent(referrer_url);
  const Extension* webstore_app = GetWebStoreApp();

  bool referrer_valid = (referrer_extension == webstore_app);
  bool download_valid = (download_extension == webstore_app);

  // We also allow the download to be from a small set of trusted paths.
  if (!download_valid) {
    for (size_t i = 0; i < arraysize(kAllowedDownloadURLPatterns); i++) {
      URLPattern pattern(URLPattern::SCHEME_HTTPS,
                         kAllowedDownloadURLPatterns[i]);
      if (pattern.MatchesURL(download_url)) {
        download_valid = true;
        break;
      }
    }
  }

  // If the command-line gallery URL is set, then be a bit more lenient.
  GURL store_url =
      GURL(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
           switches::kAppsGalleryURL));
  if (!store_url.is_empty()) {
    std::string store_tld =
        net::RegistryControlledDomainService::GetDomainAndRegistry(store_url);
    if (!referrer_valid) {
      std::string referrer_tld =
          net::RegistryControlledDomainService::GetDomainAndRegistry(
              referrer_url);
      // The referrer gets stripped when transitioning from https to http,
      // or when hitting an unknown test cert and that commonly happens in
      // testing environments.  Given this, we allow an empty referrer when
      // the command-line flag is set.
      // Otherwise, the TLD must match the TLD of the command-line url.
      referrer_valid = referrer_url.is_empty() || (referrer_tld == store_tld);
    }

    if (!download_valid) {
      std::string download_tld =
          net::RegistryControlledDomainService::GetDomainAndRegistry(
              download_url);

      // Otherwise, the TLD must match the TLD of the command-line url.
      download_valid = (download_tld == store_tld);
    }
  }

  return (referrer_valid && download_valid);
}

const Extension* ExtensionService::GetInstalledApp(const GURL& url) {
  // Check for hosted app.
  const Extension* app = GetExtensionByWebExtent(url);
  if (app)
    return app;

  // Check for packaged app.
  app = GetExtensionByURL(url);
  if (app && app->is_app())
    return app;

  return NULL;
}

bool ExtensionService::IsInstalledApp(const GURL& url) {
  return !!GetInstalledApp(url);
}

void ExtensionService::SetInstalledAppForRenderer(int renderer_child_id,
                                                  const Extension* app) {
  installed_app_hosts_[renderer_child_id] = app;
}

const Extension* ExtensionService::GetInstalledAppForRenderer(
    int renderer_child_id) {
  InstalledAppMap::iterator i = installed_app_hosts_.find(renderer_child_id);
  if (i == installed_app_hosts_.end())
    return NULL;
  return i->second;
}

// static
// This function is used to implement the command-line switch
// --uninstall-extension, and to uninstall an extension via sync.  The LOG
// statements within this function are used to inform the user if the uninstall
// cannot be done.
bool ExtensionService::UninstallExtensionHelper(
    ExtensionService* extensions_service,
    const std::string& extension_id) {

  const Extension* extension =
      extensions_service->GetInstalledExtension(extension_id);

  // We can't call UninstallExtension with an invalid extension ID.
  if (!extension) {
    LOG(WARNING) << "Attempted uninstallation of non-existent extension with "
                 << "id: " << extension_id;
    return false;
  }

  // The following call to UninstallExtension will not allow an uninstall of a
  // policy-controlled extension.
  std::string error;
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
                                   ExtensionPrefs* extension_prefs,
                                   bool autoupdate_enabled,
                                   bool extensions_enabled)
    : profile_(profile),
      extension_prefs_(extension_prefs),
      settings_frontend_(extensions::SettingsFrontend::Create(profile)),
      pending_extension_manager_(*ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      install_directory_(install_directory),
      extensions_enabled_(extensions_enabled),
      show_extensions_prompts_(true),
      ready_(false),
      toolbar_model_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      menu_manager_(profile),
      app_notification_manager_(new AppNotificationManager(profile)),
      permissions_manager_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      apps_promo_(profile->GetPrefs()),
      event_routers_initialized_(false),
      extension_warnings_(profile) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Figure out if extension installation should be enabled.
  if (command_line->HasSwitch(switches::kDisableExtensions)) {
    extensions_enabled_ = false;
  } else if (profile->GetPrefs()->GetBoolean(prefs::kDisableExtensions)) {
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
    updater_.reset(new ExtensionUpdater(this,
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
    ExternalExtensionProviderImpl::CreateExternalProviders(
        this, profile_, &external_extension_providers_);
  }

  // Use monochrome icons for Omnibox icons.
  omnibox_popup_icon_manager_.set_monochrome(true);
  omnibox_icon_manager_.set_monochrome(true);
  omnibox_icon_manager_.set_padding(gfx::Insets(0, kOmniboxIconPaddingLeft,
                                                0, kOmniboxIconPaddingRight));

  // How long is the path to the Extensions directory?
  UMA_HISTOGRAM_CUSTOM_COUNTS("Extensions.ExtensionRootPathLength",
                              install_directory_.value().length(), 0, 500, 100);
}

const ExtensionList* ExtensionService::extensions() const {
  return &extensions_;
}

const ExtensionList* ExtensionService::disabled_extensions() const {
  return &disabled_extensions_;
}

const ExtensionList* ExtensionService::terminated_extensions() const {
  return &terminated_extensions_;
}

PendingExtensionManager* ExtensionService::pending_extension_manager() {
  return &pending_extension_manager_;
}

ExtensionService::~ExtensionService() {
  // No need to unload extensions here because they are profile-scoped, and the
  // profile is in the process of being deleted.

  ProviderCollection::const_iterator i;
  for (i = external_extension_providers_.begin();
       i != external_extension_providers_.end(); ++i) {
    ExternalExtensionProviderInterface* provider = i->get();
    provider->ServiceShutdown();
  }
}

void ExtensionService::InitEventRoutersAfterImport() {
  registrar_.Add(this, chrome::NOTIFICATION_IMPORT_FINISHED,
                 content::Source<Profile>(profile_));
}

void ExtensionService::InitEventRouters() {
  if (event_routers_initialized_)
    return;

  downloads_event_router_.reset(new ExtensionDownloadsEventRouter(profile_));
  history_event_router_.reset(new HistoryExtensionEventRouter());
  history_event_router_->ObserveProfile(profile_);
  browser_event_router_.reset(new ExtensionBrowserEventRouter(profile_));
  browser_event_router_->Init();
  preference_event_router_.reset(new ExtensionPreferenceEventRouter(profile_));
  bookmark_event_router_.reset(new BookmarkExtensionEventRouter(
      profile_->GetBookmarkModel()));
  bookmark_event_router_->Init();
  cookies_event_router_.reset(new ExtensionCookiesEventRouter(profile_));
  cookies_event_router_->Init();
  management_event_router_.reset(new ExtensionManagementEventRouter(profile_));
  management_event_router_->Init();
  ExtensionProcessesEventRouter::GetInstance()->ObserveProfile(profile_);
  web_navigation_event_router_.reset(
      new ExtensionWebNavigationEventRouter(profile_));
  web_navigation_event_router_->Init();

#if defined(OS_CHROMEOS)
  file_browser_event_router_.reset(
      new ExtensionFileBrowserEventRouter(profile_));
  file_browser_event_router_->ObserveFileSystemEvents();

  input_method_event_router_.reset(
      new chromeos::ExtensionInputMethodEventRouter);

  ExtensionMediaPlayerEventRouter::GetInstance()->Init(profile_);
  ExtensionInputImeEventRouter::GetInstance()->Init();
#endif

#if defined(OS_CHROMEOS) && defined(TOUCH_UI)
  ExtensionInputUiEventRouter::GetInstance()->Init();
#endif

  event_routers_initialized_ = true;
}

const Extension* ExtensionService::GetExtensionById(
    const std::string& id, bool include_disabled) const {
  return GetExtensionByIdInternal(id, true, include_disabled, false);
}

void ExtensionService::Init() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK(!ready_);  // Can't redo init.
  DCHECK_EQ(extensions_.size(), 0u);

  // Hack: we need to ensure the ResourceDispatcherHost is ready before we load
  // the first extension, because its members listen for loaded notifications.
  g_browser_process->resource_dispatcher_host();

  component_loader_->LoadAll();
  extensions::InstalledLoader(this).LoadAllExtensions();

  // TODO(erikkay) this should probably be deferred to a future point
  // rather than running immediately at startup.
  CheckForExternalUpdates();

  // TODO(erikkay) this should probably be deferred as well.
  GarbageCollectExtensions();
}

bool ExtensionService::UpdateExtension(
    const std::string& id,
    const FilePath& extension_path,
    const GURL& download_url,
    CrxInstaller** out_crx_installer) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  PendingExtensionInfo pending_extension_info;
  bool is_pending_extension = pending_extension_manager_.GetById(
      id, &pending_extension_info);

  const Extension* extension =
      GetExtensionByIdInternal(id, true, true, false);
  if (!is_pending_extension && !extension) {
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
  ExtensionInstallUI* client =
      (!is_pending_extension || pending_extension_info.install_silently()) ?
      NULL : new ExtensionInstallUI(profile_);

  scoped_refptr<CrxInstaller> installer(CrxInstaller::Create(this, client));
  installer->set_expected_id(id);
  if (is_pending_extension)
    installer->set_install_source(pending_extension_info.install_source());
  else if (extension)
    installer->set_install_source(extension->location());
  if (pending_extension_info.install_silently())
    installer->set_allow_silent_install(true);
  if (extension && extension->from_webstore())
    installer->set_is_gallery_install(true);
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
    ExtensionProcessManager* manager = profile_->GetExtensionProcessManager();
    ExtensionHost* host = manager->GetBackgroundHostForExtension(extension_id);
    if (host) {
      // Look for an open inspector for the background page.
      int devtools_cookie = DevToolsManager::GetInstance()->DetachClientHost(
          host->render_view_host());
      if (devtools_cookie >= 0)
        orphaned_dev_tools_[extension_id] = devtools_cookie;
    }

    path = current_extension->path();
    DisableExtension(extension_id);
    disabled_extension_paths_[extension_id] = path;
  } else {
    path = unloaded_extension_paths_[extension_id];
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
    const std::string& extension_id_unsafe,
    bool external_uninstall,
    std::string* error) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Copy the extension identifier since the reference might have been
  // obtained via Extension::id() and the extension may be deleted in
  // this function.
  std::string extension_id(extension_id_unsafe);

  scoped_refptr<const Extension> extension(GetInstalledExtension(extension_id));

  // Callers should not send us nonexistent extensions.
  CHECK(extension);

  // Policy change which triggers an uninstall will always set
  // |external_uninstall| to true so this is the only way to uninstall
  // managed extensions.
  if (!Extension::UserMayDisable(extension->location()) &&
      !external_uninstall) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_UNINSTALL_NOT_ALLOWED,
        content::Source<Profile>(profile_),
        content::Details<const Extension>(extension));
    if (error != NULL) {
      *error = errors::kCannotUninstallManagedExtension;
    }
    return false;
  }

  // Extract the data we need for sync now, but don't actually sync until we've
  // completed the uninstallation.
  SyncBundle* sync_bundle = GetSyncBundleForExtension(*extension);

  SyncChange sync_change;
  if (sync_bundle) {
    ExtensionSyncData extension_sync_data(
        *extension,
        IsExtensionEnabled(extension_id),
        IsIncognitoEnabled(extension_id),
        extension_prefs_->IsAppNotificationSetupDone(extension_id),
        extension_prefs_->IsAppNotificationDisabled(extension_id));
    sync_change = extension_sync_data.GetSyncChange(SyncChange::ACTION_DELETE);
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
  bool is_storage_isolated =
    (extension->is_storage_isolated() &&
    extension->HasAPIPermission(ExtensionAPIPermission::kExperimental));

  if (extension->is_hosted_app() &&
      !profile_->GetExtensionSpecialStoragePolicy()->
          IsStorageProtected(launch_web_url_origin)) {
    ExtensionDataDeleter::StartDeleting(
        profile_, extension_id, launch_web_url_origin, is_storage_isolated);
  }
  ExtensionDataDeleter::StartDeleting(
      profile_, extension_id, extension->url(), is_storage_isolated);

  UntrackTerminatedExtension(extension_id);

  // Notify interested parties that we've uninstalled this extension.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
      content::Source<Profile>(profile_),
      content::Details<const std::string>(&extension_id));

  if (sync_bundle && sync_bundle->HasExtensionId(extension_id)) {
    sync_bundle->sync_processor->ProcessSyncChanges(
        FROM_HERE, SyncChangeList(1, sync_change));
    sync_bundle->synced_extensions.erase(extension_id);
  }

  // Track the uninstallation.
  UMA_HISTOGRAM_ENUMERATION("Extensions.ExtensionUninstalled", 1, 2);

  static bool default_apps_trial_exists =
      base::FieldTrialList::TrialExists(kDefaultAppsTrial_Name);
  if (default_apps_trial_exists) {
    UMA_HISTOGRAM_ENUMERATION(
        base::FieldTrial::MakeName("Extensions.ExtensionUninstalled",
                                   kDefaultAppsTrial_Name),
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
  const Extension* extension =
      GetExtensionByIdInternal(extension_id, true, false, true);
  if (extension)
    return true;

  extension =
      GetExtensionByIdInternal(extension_id, false, true, false);
  if (extension)
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

  const Extension* extension =
      GetExtensionByIdInternal(extension_id, false, true, false);
  // This can happen if sync enables an extension that is not
  // installed yet.
  if (!extension)
    return;

  // Move it over to the enabled list.
  extensions_.push_back(make_scoped_refptr(extension));
  ExtensionList::iterator iter = std::find(disabled_extensions_.begin(),
                                           disabled_extensions_.end(),
                                           extension);
  disabled_extensions_.erase(iter);

  // Make sure any browser action contained within it is not hidden.
  extension_prefs_->SetBrowserActionVisibility(extension, true);

  NotifyExtensionLoaded(extension);

  SyncExtensionChangeIfNeeded(*extension);
}

void ExtensionService::DisableExtension(const std::string& extension_id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // The extension may have been disabled already.
  if (!IsExtensionEnabled(extension_id))
    return;

  const Extension* extension = GetInstalledExtension(extension_id);
  // |extension| can be NULL if sync disables an extension that is not
  // installed yet.
  if (extension && !Extension::UserMayDisable(extension->location()))
    return;

  extension_prefs_->SetExtensionState(extension_id, Extension::DISABLED);

  extension = GetExtensionByIdInternal(extension_id, true, false, true);
  if (!extension)
    return;

  // Move it over to the disabled list.
  disabled_extensions_.push_back(make_scoped_refptr(extension));
  ExtensionList::iterator iter = std::find(extensions_.begin(),
                                           extensions_.end(),
                                           extension);
  if (iter != extensions_.end()) {
    extensions_.erase(iter);
  } else {
    iter = std::find(terminated_extensions_.begin(),
                     terminated_extensions_.end(),
                     extension);
    terminated_extensions_.erase(iter);
  }

  NotifyExtensionUnloaded(extension, extension_misc::UNLOAD_REASON_DISABLE);

  SyncExtensionChangeIfNeeded(*extension);

  // Deactivating one extension might have solved the problems of others.
  // Therefore, we clear warnings of this type for all extensions.
  std::set<ExtensionWarningSet::WarningType> warnings;
  extension_warnings_.GetWarningsAffectingExtension(extension_id, &warnings);
  extension_warnings_.ClearWarnings(warnings);
}

void ExtensionService::GrantPermissions(const Extension* extension) {
  CHECK(extension);

  // We only maintain the granted permissions prefs for extensions that can't
  // silently increase their permissions.
  if (extension->CanSilentlyIncreasePermissions())
    return;

  extension_prefs_->AddGrantedPermissions(extension->id(),
                                          extension->GetActivePermissions());
}

void ExtensionService::GrantPermissionsAndEnableExtension(
    const Extension* extension) {
  CHECK(extension);
  RecordPermissionMessagesHistogram(
      extension, "Extensions.Permissions_ReEnable");
  GrantPermissions(extension);
  extension_prefs_->SetDidExtensionEscalatePermissions(extension, false);
  EnableExtension(extension->id());
}

void ExtensionService::UpdateActivePermissions(
    const Extension* extension,
    const ExtensionPermissionSet* permissions) {
  extension_prefs()->SetActivePermissions(extension->id(), permissions);
  extension->SetActivePermissions(permissions);
}

// static
void ExtensionService::RecordPermissionMessagesHistogram(
    const Extension* e, const char* histogram) {
  // Since this is called from multiple sources, and since the Histogram macros
  // use statics, we need to manually lookup the Histogram ourselves.
  base::Histogram* counter = base::LinearHistogram::FactoryGet(
      histogram,
      1,
      ExtensionPermissionMessage::kEnumBoundary,
      ExtensionPermissionMessage::kEnumBoundary + 1,
      base::Histogram::kUmaTargetedHistogramFlag);

  ExtensionPermissionMessages permissions = e->GetPermissionMessages();
  if (permissions.empty()) {
    counter->Add(ExtensionPermissionMessage::kNone);
  } else {
    for (ExtensionPermissionMessages::iterator it = permissions.begin();
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
  profile_->RegisterExtensionWithRequestContexts(extension);

  // Tell subsystems that use the EXTENSION_LOADED notification about the new
  // extension.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_LOADED,
      content::Source<Profile>(profile_),
      content::Details<const Extension>(extension));

  // Tell renderers about the new extension.
  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    content::RenderProcessHost* host = i.GetCurrentValue();
    Profile* host_profile =
        Profile::FromBrowserContext(host->GetBrowserContext());
    if (host_profile->GetOriginalProfile() == profile_->GetOriginalProfile()) {
      std::vector<ExtensionMsg_Loaded_Params> loaded_extensions(
          1, ExtensionMsg_Loaded_Params(extension));
      host->Send(
          new ExtensionMsg_Loaded(loaded_extensions));
    }
  }

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
    profile_->GetChromeURLDataManager()->AddDataSource(favicon_source);
  }
  // Same for chrome://thumb/ resources.
  if (extension->HasHostPermission(GURL(chrome::kChromeUIThumbnailURL))) {
    ThumbnailSource* thumbnail_source = new ThumbnailSource(profile_);
    profile_->GetChromeURLDataManager()->AddDataSource(thumbnail_source);
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
#if defined(TOUCH_UI)
  chromeos::input_method::InputMethodManager* input_method_manager =
      chromeos::input_method::InputMethodManager::GetInstance();
#endif
  for (std::vector<Extension::InputComponentInfo>::const_iterator component =
           extension->input_components().begin();
       component != extension->input_components().end();
       ++component) {
    if (component->type == Extension::INPUT_COMPONENT_TYPE_IME) {
      ExtensionInputImeEventRouter::GetInstance()->RegisterIme(
          profile_, extension->id(), *component);
    }
#if defined(TOUCH_UI)
    if (component->type == Extension::INPUT_COMPONENT_TYPE_VIRTUAL_KEYBOARD &&
        !component->layouts.empty()) {
      const bool is_system =
          !Extension::IsExternalLocation(extension->location());
      input_method_manager->RegisterVirtualKeyboard(
          extension->url(),
          component->name,  // human-readable name of the keyboard extension.
          component->layouts,
          is_system);
    }
#endif
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

  profile_->UnregisterExtensionWithRequestContexts(extension->id(), reason);
  profile_->GetExtensionSpecialStoragePolicy()->
      RevokeRightsForExtension(extension);

  ExtensionWebUI::UnregisterChromeURLOverrides(
      profile_, extension->GetChromeURLOverrides());

#if defined(OS_CHROMEOS)
    // Revoke external file access to
  if (profile_->GetFileSystemContext() &&
      profile_->GetFileSystemContext()->path_manager() &&
      profile_->GetFileSystemContext()->path_manager()->external_provider()) {
    profile_->GetFileSystemContext()->path_manager()->external_provider()->
        RevokeAccessForExtension(extension->id());
  }
#endif

  UpdateActiveExtensionsInCrashReporter();

  bool plugins_changed = false;
  for (size_t i = 0; i < extension->plugins().size(); ++i) {
    const Extension::PluginInfo& plugin = extension->plugins()[i];
    if (!BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                                 base::Bind(&ForceShutdownPlugin, plugin.path)))
      NOTREACHED();
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
    if (Extension::IdIsValid(blacklist[i])) {
      blacklist_set.insert(blacklist[i]);
    }
  }
  extension_prefs_->UpdateBlacklist(blacklist_set);
  std::vector<std::string> to_be_removed;
  // Loop current extensions, unload installed extensions.
  for (ExtensionList::const_iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    const Extension* extension = (*iter);
    if (blacklist_set.find(extension->id()) != blacklist_set.end()) {
      to_be_removed.push_back(extension->id());
    }
  }

  // UnloadExtension will change the extensions_ list. So, we should
  // call it outside the iterator loop.
  for (unsigned int i = 0; i < to_be_removed.size(); ++i) {
    UnloadExtension(to_be_removed[i], extension_misc::UNLOAD_REASON_DISABLE);
  }
}

Profile* ExtensionService::profile() {
  return profile_;
}

ExtensionPrefs* ExtensionService::extension_prefs() {
  return extension_prefs_;
}

extensions::SettingsFrontend* ExtensionService::settings_frontend() {
  return settings_frontend_.get();
}

ExtensionContentSettingsStore*
    ExtensionService::GetExtensionContentSettingsStore() {
  return extension_prefs()->content_settings_store();
}

bool ExtensionService::is_ready() {
  return ready_;
}

ExtensionUpdater* ExtensionService::updater() {
  return updater_.get();
}

void ExtensionService::CheckAdminBlacklist() {
  std::vector<std::string> to_be_removed;
  // Loop through extensions list, unload installed extensions.
  for (ExtensionList::const_iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    const Extension* extension = (*iter);
    if (!extension_prefs_->IsExtensionAllowedByPolicy(extension->id(),
                                                      extension->location())) {
      to_be_removed.push_back(extension->id());
    }
  }

  // UnloadExtension will change the extensions_ list. So, we should
  // call it outside the iterator loop.
  for (unsigned int i = 0; i < to_be_removed.size(); ++i)
    UnloadExtension(to_be_removed[i], extension_misc::UNLOAD_REASON_DISABLE);
}

void ExtensionService::CheckForUpdatesSoon() {
  if (updater()) {
    updater()->CheckSoon();
  } else {
    LOG(WARNING) << "CheckForUpdatesSoon() called with auto-update turned off";
  }
}

namespace {
  bool IsSyncableNone(const Extension& extension) { return false; }
}  // namespace

ExtensionService::SyncBundle::SyncBundle()
  : filter(IsSyncableNone),
    sync_processor(NULL) {
}

ExtensionService::SyncBundle::~SyncBundle() {
}

bool ExtensionService::SyncBundle::HasExtensionId(const std::string& id) const {
  return synced_extensions.find(id) != synced_extensions.end();
}

bool ExtensionService::SyncBundle::HasPendingExtensionId(const std::string& id)
    const {
  return pending_sync_data.find(id) != pending_sync_data.end();
}

void ExtensionService::SyncExtensionChangeIfNeeded(const Extension& extension) {
  SyncBundle* sync_bundle = GetSyncBundleForExtension(extension);
  if (sync_bundle) {
    ExtensionSyncData extension_sync_data(
        extension,
        IsExtensionEnabled(extension.id()),
        IsIncognitoEnabled(extension.id()),
        extension_prefs_->IsAppNotificationSetupDone(extension.id()),
        extension_prefs_->IsAppNotificationDisabled(extension.id()));

    SyncChangeList sync_change_list(1, extension_sync_data.GetSyncChange(
        sync_bundle->HasExtensionId(extension.id()) ?
            SyncChange::ACTION_UPDATE : SyncChange::ACTION_ADD));
    sync_bundle->sync_processor->ProcessSyncChanges(
        FROM_HERE, sync_change_list);
    sync_bundle->synced_extensions.insert(extension.id());
    sync_bundle->pending_sync_data.erase(extension.id());
  }
}

ExtensionService::SyncBundle* ExtensionService::GetSyncBundleForExtension(
    const Extension& extension) {
  if (app_sync_bundle_.filter(extension))
    return &app_sync_bundle_;
  else if (extension_sync_bundle_.filter(extension))
    return &extension_sync_bundle_;
  else
    return NULL;
}

ExtensionService::SyncBundle*
    ExtensionService::GetSyncBundleForExtensionSyncData(
    const ExtensionSyncData& extension_sync_data) {
  switch (extension_sync_data.type()) {
    case Extension::SYNC_TYPE_APP:
      return &app_sync_bundle_;
    case Extension::SYNC_TYPE_EXTENSION:
      return &extension_sync_bundle_;
    default:
      NOTREACHED();
      return NULL;
  }
}

#define GET_SYNC_BUNDLE_FOR_MODEL_TYPE_BODY() \
  do { \
    switch (type) { \
      case syncable::APPS: \
        return &app_sync_bundle_; \
      case syncable::EXTENSIONS: \
        return &extension_sync_bundle_; \
      default: \
        NOTREACHED(); \
        return NULL; \
    } \
  } while (0)

const ExtensionService::SyncBundle*
    ExtensionService::GetSyncBundleForModelTypeConst(
    syncable::ModelType type) const {
  GET_SYNC_BUNDLE_FOR_MODEL_TYPE_BODY();
}

ExtensionService::SyncBundle* ExtensionService::GetSyncBundleForModelType(
    syncable::ModelType type) {
  GET_SYNC_BUNDLE_FOR_MODEL_TYPE_BODY();
}

#undef GET_SYNC_BUNDLE_FOR_MODEL_TYPE_BODY

SyncError ExtensionService::MergeDataAndStartSyncing(
    syncable::ModelType type,
    const SyncDataList& initial_sync_data,
    SyncChangeProcessor* sync_processor) {
  CHECK(sync_processor);

  SyncBundle* bundle = NULL;

  switch (type) {
    case syncable::EXTENSIONS:
      bundle = &extension_sync_bundle_;
      bundle->filter = IsSyncableExtension;
      break;

    case syncable::APPS:
      bundle = &app_sync_bundle_;
      bundle->filter = IsSyncableApp;
      break;

    default:
      LOG(FATAL) << "Got " << type << " ModelType";
  }

  bundle->sync_processor = sync_processor;

  for (SyncDataList::const_iterator i = initial_sync_data.begin();
       i != initial_sync_data.end();
       ++i) {
    ExtensionSyncData extension_sync_data = ExtensionSyncData(*i);
    bundle->synced_extensions.insert(extension_sync_data.id());
    ProcessExtensionSyncData(extension_sync_data, *bundle);
  }

  SyncDataList sync_data_list = GetAllSyncData(type);
  SyncChangeList sync_change_list;
  for (SyncDataList::const_iterator i = sync_data_list.begin();
       i != sync_data_list.end();
       ++i) {
    if (bundle->HasExtensionId(i->GetTag()))
      sync_change_list.push_back(SyncChange(SyncChange::ACTION_UPDATE, *i));
    else
      sync_change_list.push_back(SyncChange(SyncChange::ACTION_ADD, *i));
  }
  bundle->sync_processor->ProcessSyncChanges(FROM_HERE, sync_change_list);

  return SyncError();
}

void ExtensionService::StopSyncing(syncable::ModelType type) {
  SyncBundle* bundle = GetSyncBundleForModelType(type);
  CHECK(bundle);
  // This is the simplest way to clear out the bundle.
  *bundle = SyncBundle();
}

SyncDataList ExtensionService::GetAllSyncData(syncable::ModelType type) const {
  const SyncBundle* bundle = GetSyncBundleForModelTypeConst(type);
  CHECK(bundle);
  std::vector<ExtensionSyncData> extension_sync_data = GetSyncDataList(*bundle);
  SyncDataList result(extension_sync_data.size());
  for (int i = 0; i < static_cast<int>(extension_sync_data.size()); ++i) {
    result[i] = extension_sync_data[i].GetSyncData();
  }
  return result;
}

SyncError ExtensionService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const SyncChangeList& change_list) {
  for (SyncChangeList::const_iterator i = change_list.begin();
      i != change_list.end();
      ++i) {
    ExtensionSyncData extension_sync_data = ExtensionSyncData(*i);
    SyncBundle* bundle = GetSyncBundleForExtensionSyncData(extension_sync_data);
    CHECK(bundle);

    if (extension_sync_data.uninstalled())
      bundle->synced_extensions.erase(extension_sync_data.id());
    else
      bundle->synced_extensions.insert(extension_sync_data.id());
    ProcessExtensionSyncData(extension_sync_data, *bundle);
  }

  return SyncError();
}

void ExtensionService::GetSyncDataListHelper(
    const ExtensionList& extensions,
    const SyncBundle& bundle,
    std::vector<ExtensionSyncData>* sync_data_list) const {
  for (ExtensionList::const_iterator it = extensions.begin();
       it != extensions.end(); ++it) {
    const Extension& extension = **it;
    if (bundle.filter(extension) &&
        // If we have pending extension data for this extension, then this
        // version is out of date.  We'll sync back the version we got from
        // sync.
        !bundle.HasPendingExtensionId(extension.id())) {
      sync_data_list->push_back(ExtensionSyncData(
          extension,
          IsExtensionEnabled(extension.id()),
          IsIncognitoEnabled(extension.id()),
          extension_prefs_->IsAppNotificationSetupDone(extension.id()),
          extension_prefs_->IsAppNotificationDisabled(extension.id())));
    }
  }
}

std::vector<ExtensionSyncData> ExtensionService::GetSyncDataList(
    const SyncBundle& bundle) const {
  std::vector<ExtensionSyncData> extension_sync_list;
  GetSyncDataListHelper(extensions_, bundle, &extension_sync_list);
  GetSyncDataListHelper(disabled_extensions_, bundle, &extension_sync_list);
  GetSyncDataListHelper(terminated_extensions_, bundle, &extension_sync_list);

  for (std::map<std::string, ExtensionSyncData>::const_iterator i =
           bundle.pending_sync_data.begin();
       i != bundle.pending_sync_data.end();
       ++i) {
    extension_sync_list.push_back(i->second);
  }

  return extension_sync_list;
}

void ExtensionService::ProcessExtensionSyncData(
    const ExtensionSyncData& extension_sync_data,
    SyncBundle& bundle) {
  const std::string& id = extension_sync_data.id();
  const Extension* extension = GetInstalledExtension(id);

  // TODO(bolms): we should really handle this better.  The particularly bad
  // case is where an app becomes an extension or vice versa, and we end up with
  // a zombie extension that won't go away.
  if (extension && !bundle.filter(*extension))
    return;

  // Handle uninstalls first.
  if (extension_sync_data.uninstalled()) {
    std::string error;
    if (!UninstallExtensionHelper(this, id)) {
      LOG(WARNING) << "Could not uninstall extension " << id
                   << " for sync";
    }
    return;
  }

  // Set user settings.
  if (extension_sync_data.enabled()) {
    EnableExtension(id);
  } else {
    DisableExtension(id);
  }

  // We need to cache some version information here because setting the
  // incognito flag invalidates the |extension| pointer (it reloads the
  // extension).
  bool extension_installed = extension != NULL;
  int result = extension ?
      extension->version()->CompareTo(extension_sync_data.version()) : 0;
  SetIsIncognitoEnabled(id, extension_sync_data.incognito_enabled());
  extension = NULL;  // No longer safe to use.

  if (extension_installed) {
    // If the extension is already installed, check if it's outdated.
    if (result < 0) {
      // Extension is outdated.
      bundle.pending_sync_data[extension_sync_data.id()] = extension_sync_data;
      CheckForUpdatesSoon();
    }
  } else {
    // TODO(akalin): Replace silent update with a list of enabled
    // permissions.
    const bool kInstallSilently = true;
    if (!pending_extension_manager()->AddFromSync(
            id,
            extension_sync_data.update_url(),
            bundle.filter,
            kInstallSilently)) {
      LOG(WARNING) << "Could not add pending extension for " << id;
      // This means that the extension is already pending installation, with a
      // non-INTERNAL location.  Add to pending_sync_data, even though it will
      // never be removed (we'll never install a syncable version of the
      // extension), so that GetAllSyncData() continues to send it.
    }
    // Track pending extensions so that we can return them in GetAllSyncData().
    bundle.pending_sync_data[extension_sync_data.id()] = extension_sync_data;
    CheckForUpdatesSoon();
  }
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
    // This shouldn't be called for component extensions.
    NOTREACHED();
    return;
  }

  // Broadcast unloaded and loaded events to update browser state. Only bother
  // if the value changed and the extension is actually enabled, since there is
  // no UI otherwise.
  bool old_enabled = extension_prefs_->IsIncognitoEnabled(extension_id);
  if (enabled == old_enabled)
    return;

  extension_prefs_->SetIsIncognitoEnabled(extension_id, enabled);

  bool extension_is_enabled = std::find(extensions_.begin(), extensions_.end(),
                                        extension) != extensions_.end();

  // When we reload the extension the ID may be invalidated if we've passed it
  // by const ref everywhere. Make a copy to be safe.
  std::string id = extension_id;
  if (extension_is_enabled)
    ReloadExtension(extension->id());

  // Reloading the extension invalidates the |extension| pointer.
  extension = GetInstalledExtension(id);
  if (extension)
    SyncExtensionChangeIfNeeded(*extension);
}

void ExtensionService::SetAppNotificationSetupDone(
    const std::string& extension_id,
    bool value) {
  const Extension* extension = GetInstalledExtension(extension_id);
  // This method is called when the user sets up app notifications.
  // So it is not expected to be called until the extension is installed.
  if (!extension) {
    NOTREACHED();
    return;
  }
  extension_prefs_->SetAppNotificationSetupDone(extension_id, value);
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

  extension_prefs_->SetAppNotificationDisabled(extension_id, value);
  SyncExtensionChangeIfNeeded(*extension);
}

bool ExtensionService::CanCrossIncognito(const Extension* extension) {
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

bool ExtensionService::AllowFileAccess(const Extension* extension) {
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

  bool extension_is_enabled = std::find(extensions_.begin(), extensions_.end(),
                                        extension) != extensions_.end();
  if (extension_is_enabled)
    ReloadExtension(extension->id());
}

bool ExtensionService::GetBrowserActionVisibility(const Extension* extension) {
  return extension_prefs_->GetBrowserActionVisibility(extension);
}

void ExtensionService::SetBrowserActionVisibility(const Extension* extension,
                                                  bool visible) {
  extension_prefs_->SetBrowserActionVisibility(extension, visible);
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

  // If any external extension records give a URL, a provider will set
  // this to true.  Used by OnExternalProviderReady() to see if we need
  // to start an update check to fetch a new external extension.
  external_extension_url_added_ = false;

  // Ask each external extension provider to give us a call back for each
  // extension they know about. See OnExternalExtension(File|UpdateUrl)Found.
  ProviderCollection::const_iterator i;
  for (i = external_extension_providers_.begin();
       i != external_extension_providers_.end(); ++i) {
    ExternalExtensionProviderInterface* provider = i->get();
    provider->VisitRegisteredExtension();
  }

  // Do any required work that we would have done after completion of all
  // providers.
  if (external_extension_providers_.empty()) {
    OnAllExternalProvidersReady();
  }
}

void ExtensionService::OnExternalProviderReady(
    const ExternalExtensionProviderInterface* provider) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(provider->IsReady());

  // An external provider has finished loading.  We only take action
  // if all of them are finished. So we check them first.
  ProviderCollection::const_iterator i;
  for (i = external_extension_providers_.begin();
       i != external_extension_providers_.end(); ++i) {
    ExternalExtensionProviderInterface* provider = i->get();
    if (!provider->IsReady())
      return;
  }

  OnAllExternalProvidersReady();
}

void ExtensionService::OnAllExternalProvidersReady() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Install any pending extensions.
  if (external_extension_url_added_ && updater()) {
    external_extension_url_added_ = false;
    updater()->CheckNow();
  }

  // Uninstall all the unclaimed extensions.
  scoped_ptr<ExtensionPrefs::ExtensionsInfo> extensions_info(
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

  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExtensionAlerts)) {
    return;  // TODO(miket): enable unconditionally when done.
  }

  // Build up the lists of extensions that require acknowledgment.
  // If this is the first time, grandfather extensions that would have
  // caused notification.
  scoped_ptr<ExtensionGlobalError> global_error(
      new ExtensionGlobalError(AsWeakPtr()));
  bool needs_alert = false;
  for (ExtensionList::const_iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    const Extension* e = *iter;
    if (!IsExtensionEnabled(e->id())) {
      continue;
    }
    if (Extension::IsExternalLocation(e->location())) {
      if (!extension_prefs_->IsExternalExtensionAcknowledged(e->id())) {
        global_error->AddExternalExtension(e->id());
        needs_alert = true;
      }
    }
    if (extension_prefs_->IsExtensionBlacklisted(e->id())) {
      if (!extension_prefs_->IsBlacklistedExtensionAcknowledged(e->id())) {
        global_error->AddBlacklistedExtension(e->id());
        needs_alert = true;
      }
    }
    if (extension_prefs_->IsExtensionOrphaned(e->id())) {
      if (!extension_prefs_->IsOrphanedExtensionAcknowledged(e->id())) {
        global_error->AddOrphanedExtension(e->id());
        needs_alert = true;
      }
    }
  }

  if (needs_alert) {
    if (extension_prefs_->SetAlertSystemFirstRun()) {
      global_error->set_accept_callback(
          base::Bind(&ExtensionService::HandleExtensionAlertAccept,
                     base::Unretained(this)));
      global_error->set_cancel_callback(
          base::Bind(&ExtensionService::HandleExtensionAlertDetails,
                     base::Unretained(this)));
      ShowExtensionAlert(global_error.release());
    } else {
      // First run. Just acknowledge all the extensions, silently, by
      // shortcutting the display of the UI and going straight to the
      // callback for pressing the Accept button.
      HandleExtensionAlertAccept(*global_error.get(), NULL);
    }
  }
}

void ExtensionService::ShowExtensionAlert(ExtensionGlobalError* global_error) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (browser) {
    global_error->ShowBubbleView(browser);
  }
}

void ExtensionService::HandleExtensionAlertAccept(
    const ExtensionGlobalError& global_error, Browser* browser) {
  const ExtensionIdSet *extension_ids =
      global_error.get_external_extension_ids();
  for (ExtensionIdSet::const_iterator iter = extension_ids->begin();
       iter != extension_ids->end(); ++iter) {
    extension_prefs_->AcknowledgeExternalExtension(*iter);
  }
  extension_ids = global_error.get_blacklisted_extension_ids();
  for (ExtensionIdSet::const_iterator iter = extension_ids->begin();
       iter != extension_ids->end(); ++iter) {
    extension_prefs_->AcknowledgeBlacklistedExtension(*iter);
  }
  extension_ids = global_error.get_orphaned_extension_ids();
  for (ExtensionIdSet::const_iterator iter = extension_ids->begin();
       iter != extension_ids->end(); ++iter) {
    extension_prefs_->AcknowledgeOrphanedExtension(*iter);
  }
}

void ExtensionService::HandleExtensionAlertDetails(
    const ExtensionGlobalError& global_error, Browser* browser) {
  if (browser) {
    browser->ShowExtensionsTab();
  }
}

void ExtensionService::UnloadExtension(
    const std::string& extension_id,
    extension_misc::UnloadedExtensionReason reason) {
  // Make sure the extension gets deleted after we return from this function.
  scoped_refptr<const Extension> extension(
      GetExtensionByIdInternal(extension_id, true, true, false));

  // This method can be called via PostTask, so the extension may have been
  // unloaded by the time this runs.
  if (!extension) {
    // In case the extension may have crashed/uninstalled. Allow the profile to
    // clean up its RequestContexts.
    profile_->UnregisterExtensionWithRequestContexts(extension_id, reason);
    return;
  }

  // Keep information about the extension so that we can reload it later
  // even if it's not permanently installed.
  unloaded_extension_paths_[extension->id()] = extension->path();

  // Clean up if the extension is meant to be enabled after a reload.
  disabled_extension_paths_.erase(extension->id());

  // Clean up runtime data.
  extension_runtime_data_.erase(extension_id);

  ExtensionList::iterator iter = std::find(disabled_extensions_.begin(),
                                           disabled_extensions_.end(),
                                           extension.get());
  if (iter != disabled_extensions_.end()) {
    UnloadedExtensionInfo details(extension, reason);
    details.already_disabled = true;
    disabled_extensions_.erase(iter);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_UNLOADED,
        content::Source<Profile>(profile_),
        content::Details<UnloadedExtensionInfo>(&details));
    // Make sure the profile cleans up its RequestContexts when an already
    // disabled extension is unloaded (since they are also tracking the disabled
    // extensions).
    profile_->UnregisterExtensionWithRequestContexts(extension_id, reason);
    return;
  }

  iter = std::find(extensions_.begin(), extensions_.end(), extension.get());

  // Remove the extension from our list.
  extensions_.erase(iter);

  NotifyExtensionUnloaded(extension.get(), reason);
}

void ExtensionService::UnloadAllExtensions() {
  profile_->GetExtensionSpecialStoragePolicy()->
      RevokeRightsForAllExtensions();

  extensions_.clear();
  disabled_extensions_.clear();
  terminated_extension_ids_.clear();
  terminated_extensions_.clear();
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

  scoped_ptr<ExtensionPrefs::ExtensionsInfo> info(
      extension_prefs_->GetInstalledExtensionsInfo());

  std::map<std::string, FilePath> extension_paths;
  for (size_t i = 0; i < info->size(); ++i)
    extension_paths[info->at(i)->extension_id] = info->at(i)->extension_path;

  if (!BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(
              &extension_file_util::GarbageCollectExtensions,
              install_directory_,
              extension_paths)))
    NOTREACHED();

  // Also garbage-collect themes.  We check |profile_| to be
  // defensive; in the future, we may call GarbageCollectExtensions()
  // from somewhere other than Init() (e.g., in a timer).
  if (profile_) {
    ThemeServiceFactory::GetForProfile(profile_)->RemoveUnusedThemes();
  }
}

void ExtensionService::OnLoadedInstalledExtensions() {
  if (updater_.get()) {
    updater_->Start();
  }

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
      !Extension::IsExternalLocation(extension->location()))
    return;

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

  bool disabled = extension_prefs_->IsExtensionDisabled(extension->id());
  if (disabled) {
    disabled_extensions_.push_back(scoped_extension);
    // TODO(aa): This seems dodgy. It seems that AddExtension() could get called
    // with a disabled extension for other reasons other than that an update was
    // disabled.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_UPDATE_DISABLED,
        content::Source<Profile>(profile_),
        content::Details<const Extension>(extension));
    SyncExtensionChangeIfNeeded(*extension);
    return;
  }

  extensions_.push_back(scoped_extension);
  SyncExtensionChangeIfNeeded(*extension);
  NotifyExtensionLoaded(extension);
  IdentifyAlertableExtensions();
}

void ExtensionService::InitializePermissions(const Extension* extension) {
  // If the extension has used the optional permissions API, it will have a
  // custom set of active permissions defined in the extension prefs. Here,
  // we update the extension's active permissions based on the prefs.
  scoped_refptr<ExtensionPermissionSet> active_permissions =
      extension_prefs()->GetActivePermissions(extension->id());

  if (active_permissions.get()) {
    // We restrict the active permissions to be within the bounds defined in the
    // extension's manifest.
    //  a) active permissions must be a subset of optional + default permissions
    //  b) active permissions must contains all default permissions
    scoped_refptr<ExtensionPermissionSet> total_permissions =
        ExtensionPermissionSet::CreateUnion(
            extension->required_permission_set(),
            extension->optional_permission_set());

    // Make sure the active permissions contain no more than optional + default.
    scoped_refptr<ExtensionPermissionSet> adjusted_active =
        ExtensionPermissionSet::CreateIntersection(
            total_permissions.get(), active_permissions.get());

    // Make sure the active permissions contain the default permissions.
    adjusted_active = ExtensionPermissionSet::CreateUnion(
            extension->required_permission_set(), adjusted_active.get());

    UpdateActivePermissions(extension, adjusted_active);
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
  const Extension* old = GetExtensionByIdInternal(extension->id(),
                                                  true, true, false);
  bool is_extension_upgrade = old != NULL;
  bool is_privilege_increase = false;

  // We only need to compare the granted permissions to the current permissions
  // if the extension is not allowed to silently increase its permissions.
  if (!extension->CanSilentlyIncreasePermissions()) {
    // Add all the recognized permissions if the granted permissions list
    // hasn't been initialized yet.
    scoped_refptr<ExtensionPermissionSet> granted_permissions =
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

  if (is_extension_upgrade) {
    // Other than for unpacked extensions, CrxInstaller should have guaranteed
    // that we aren't downgrading.
    if (extension->location() != Extension::LOAD)
      CHECK(extension->version()->CompareTo(*(old->version())) >= 0);

    // Extensions get upgraded if the privileges are allowed to increase or
    // the privileges haven't increased.
    if (!is_privilege_increase) {
      SetBeingUpgraded(old, true);
      SetBeingUpgraded(extension, true);
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
  }
}

void ExtensionService::UpdateActiveExtensionsInCrashReporter() {
  std::set<std::string> extension_ids;
  for (size_t i = 0; i < extensions_.size(); ++i) {
    if (!extensions_[i]->is_theme() &&
        extensions_[i]->location() != Extension::COMPONENT)
      extension_ids.insert(extensions_[i]->id());
  }

  child_process_logging::SetActiveExtensions(extension_ids);
}

void ExtensionService::OnExtensionInstalled(
    const Extension* extension, bool from_webstore, int page_index) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Ensure extension is deleted unless we transfer ownership.
  scoped_refptr<const Extension> scoped_extension(extension);
  const std::string& id = extension->id();
  // Extensions installed by policy can't be disabled. So even if a previous
  // installation disabled the extension, make sure it is now enabled.
  bool initial_enable =
      !extension_prefs_->IsExtensionDisabled(id) ||
      !Extension::UserMayDisable(extension->location());
  PendingExtensionInfo pending_extension_info;
  if (pending_extension_manager()->GetById(id, &pending_extension_info)) {
    pending_extension_manager()->Remove(id);

    if (!pending_extension_info.ShouldAllowInstall(*extension)) {
      LOG(WARNING)
          << "ShouldAllowInstall() returned false for "
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
                         extension->path(), true)))
        NOTREACHED();
      return;
    }
  } else {
    // We explicitly want to re-enable an uninstalled external
    // extension; if we're here, that means the user is manually
    // installing the extension.
    if (IsExternalExtensionUninstalled(id)) {
      initial_enable = true;
    }
  }

  // Do not record the install histograms for upgrades.
  if (!GetExtensionByIdInternal(extension->id(), true, true, false)) {
    UMA_HISTOGRAM_ENUMERATION("Extensions.InstallType",
                              extension->GetType(), 100);
    RecordPermissionMessagesHistogram(
        extension, "Extensions.Permissions_Install");
  }

  extension_prefs_->OnExtensionInstalled(
      extension,
      initial_enable ? Extension::ENABLED : Extension::DISABLED,
      from_webstore,
      page_index);

  // Unpacked extensions default to allowing file access, but if that has been
  // overridden, don't reset the value.
  if (Extension::ShouldAlwaysAllowFileAccess(extension->location()) &&
      !extension_prefs_->HasAllowFileAccessSetting(id)) {
    extension_prefs_->SetAllowFileAccess(id, true);
  }

  // If the extension should automatically block network startup (e.g., it uses
  // the webRequest API), set the preference. Otherwise clear it, in case the
  // extension stopped using a relevant API.
  extension_prefs_->SetDelaysNetworkRequests(
      extension->id(), extension->ImplicitlyDelaysNetworkStartup());

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_INSTALLED,
      content::Source<Profile>(profile_),
      content::Details<const Extension>(extension));

  // Transfer ownership of |extension| to AddExtension.
  AddExtension(scoped_extension);
}

const Extension* ExtensionService::GetExtensionByIdInternal(
    const std::string& id, bool include_enabled, bool include_disabled,
    bool include_terminated) const {
  std::string lowercase_id = StringToLowerASCII(id);
  if (include_enabled) {
    for (ExtensionList::const_iterator iter = extensions_.begin();
        iter != extensions_.end(); ++iter) {
      if ((*iter)->id() == lowercase_id)
        return *iter;
    }
  }
  if (include_disabled) {
    for (ExtensionList::const_iterator iter = disabled_extensions_.begin();
        iter != disabled_extensions_.end(); ++iter) {
      if ((*iter)->id() == lowercase_id)
        return *iter;
    }
  }
  if (include_terminated) {
    for (ExtensionList::const_iterator iter = terminated_extensions_.begin();
        iter != terminated_extensions_.end(); ++iter) {
      if ((*iter)->id() == lowercase_id)
        return *iter;
    }
  }
  return NULL;
}

void ExtensionService::TrackTerminatedExtension(const Extension* extension) {
  if (terminated_extension_ids_.insert(extension->id()).second)
    terminated_extensions_.push_back(make_scoped_refptr(extension));

  // TODO(yoz): Listen to navcontrollers for that extension. Is this a todo?

  // TODO(yoz): make sure this is okay in *ALL* the listeners!
  UnloadExtension(extension->id(), extension_misc::UNLOAD_REASON_TERMINATE);
}

void ExtensionService::UntrackTerminatedExtension(const std::string& id) {
  std::string lowercase_id = StringToLowerASCII(id);
  if (terminated_extension_ids_.erase(lowercase_id) <= 0)
    return;

  for (ExtensionList::iterator iter = terminated_extensions_.begin();
       iter != terminated_extensions_.end(); ++iter) {
    if ((*iter)->id() == lowercase_id) {
      terminated_extensions_.erase(iter);
      return;
    }
  }
}

const Extension* ExtensionService::GetTerminatedExtension(
    const std::string& id) const {
  return GetExtensionByIdInternal(id, false, false, true);
}

const Extension* ExtensionService::GetInstalledExtension(
    const std::string& id) const {
  return GetExtensionByIdInternal(id, true, true, true);
}

const Extension* ExtensionService::GetWebStoreApp() {
  return GetExtensionById(extension_misc::kWebStoreAppId, false);
}

const Extension* ExtensionService::GetExtensionByURL(const GURL& url) {
  return url.scheme() != chrome::kExtensionScheme ? NULL :
      GetExtensionById(url.host(), false);
}

const Extension* ExtensionService::GetExtensionByWebExtent(const GURL& url) {
  for (size_t i = 0; i < extensions_.size(); ++i) {
    if (extensions_[i]->web_extent().MatchesURL(url))
      return extensions_[i];
  }
  return NULL;
}

const Extension* ExtensionService::GetDisabledExtensionByWebExtent(
    const GURL& url) {
  for (size_t i = 0; i < disabled_extensions_.size(); ++i) {
    if (disabled_extensions_[i]->web_extent().MatchesURL(url))
      return disabled_extensions_[i];
  }
  return NULL;
}

bool ExtensionService::ExtensionBindingsAllowed(const GURL& url) {
  // Allow bindings for all packaged extensions.
  // Note that GetExtensionByURL may return an Extension for hosted apps
  // (excluding bookmark apps) if the URL came from GetEffectiveURL.
  const Extension* extension = GetExtensionByURL(url);
  if (extension && extension->GetType() != Extension::TYPE_HOSTED_APP)
    return true;

  // Allow bindings for all component, hosted apps.
  if (!extension)
    extension = GetExtensionByWebExtent(url);
  return (extension && extension->location() == Extension::COMPONENT);
}

const Extension* ExtensionService::GetExtensionByOverlappingWebExtent(
    const URLPatternSet& extent) {
  for (size_t i = 0; i < extensions_.size(); ++i) {
    if (extensions_[i]->web_extent().OverlapsWith(extent))
      return extensions_[i];
  }

  return NULL;
}

const SkBitmap& ExtensionService::GetOmniboxIcon(
    const std::string& extension_id) {
  return omnibox_icon_manager_.GetIcon(extension_id);
}

const SkBitmap& ExtensionService::GetOmniboxPopupIcon(
    const std::string& extension_id) {
  return omnibox_popup_icon_manager_.GetIcon(extension_id);
}

void ExtensionService::OnExternalExtensionFileFound(
         const std::string& id,
         const Version* version,
         const FilePath& path,
         Extension::Location location,
         int creation_flags) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(Extension::IdIsValid(id));
  if (extension_prefs_->IsExternalExtensionUninstalled(id))
    return;

  DCHECK(version);

  // Before even bothering to unpack, check and see if we already have this
  // version. This is important because these extensions are going to get
  // installed on every startup.
  const Extension* existing = GetExtensionById(id, true);
  if (existing) {
    switch (existing->version()->CompareTo(*version)) {
      case -1:  // existing version is older, we should upgrade
        break;
      case 0:  // existing version is same, do nothing
        return;
      case 1:  // existing version is newer, uh-oh
        LOG(WARNING) << "Found external version of extension " << id
                     << "that is older than current version. Current version "
                     << "is: " << existing->VersionString() << ". New version "
                     << "is: " << version << ". Keeping current version.";
        return;
    }
  }

  pending_extension_manager()->AddFromExternalFile(id, location);

  // no client (silent install)
  scoped_refptr<CrxInstaller> installer(CrxInstaller::Create(this, NULL));
  installer->set_install_source(location);
  installer->set_expected_id(id);
  installer->set_expected_version(*version);
  installer->set_install_cause(extension_misc::INSTALL_CAUSE_EXTERNAL_FILE);
  installer->set_creation_flags(creation_flags);
  installer->InstallCrx(path);
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
  std::string message = base::StringPrintf(
      "Could not load extension from '%s'. %s",
      path_str.c_str(), error.c_str());
  ExtensionErrorReporter::GetInstance()->ReportError(message, be_noisy);
}

void ExtensionService::DidCreateRenderViewForBackgroundPage(
    ExtensionHost* host) {
  OrphanedDevTools::iterator iter =
      orphaned_dev_tools_.find(host->extension_id());
  if (iter == orphaned_dev_tools_.end())
    return;

  DevToolsManager::GetInstance()->AttachClientHost(
      iter->second, host->render_view_host());
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

      ExtensionHost* host = content::Details<ExtensionHost>(details).ptr();

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
      for (size_t i = 0; i < extensions_.size(); ++i) {
        loaded_extensions.push_back(
            ExtensionMsg_Loaded_Params(extensions_[i]));
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

      process_map_.Remove(process->GetID());
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&ExtensionInfoMap::UnregisterAllExtensionsInProcess,
                     profile_->GetExtensionInfoMap(),
                     process->GetID()));
      break;
    }
    case chrome::NOTIFICATION_PREF_CHANGED: {
      std::string* pref_name = content::Details<std::string>(details).ptr();
      if (*pref_name == prefs::kExtensionInstallAllowList ||
          *pref_name == prefs::kExtensionInstallDenyList) {
        CheckAdminBlacklist();
      } else {
        NOTREACHED() << "Unexpected preference name.";
      }
      break;
    }
    case chrome::NOTIFICATION_IMPORT_FINISHED: {
      registrar_.Remove(this, chrome::NOTIFICATION_IMPORT_FINISHED,
                        content::Source<Profile>(profile_));
      InitEventRouters();
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
  for (ExtensionList::const_iterator it = extensions_.begin();
       it != extensions_.end(); ++it) {
    if ((*it)->is_app() && (*it)->location() != Extension::COMPONENT)
      result.insert((*it)->id());
  }

  return result;
}

bool ExtensionService::IsBackgroundPageReady(const Extension* extension) {
  return (extension->background_url().is_empty() ||
          extension_runtime_data_[extension->id()].background_page_ready);
}

void ExtensionService::SetBackgroundPageReady(const Extension* extension) {
  DCHECK(!extension->background_url().is_empty());
  extension_runtime_data_[extension->id()].background_page_ready = true;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
      content::Source<const Extension>(extension),
      content::NotificationService::NoDetails());
}

bool ExtensionService::IsBeingUpgraded(const Extension* extension) {
  return extension_runtime_data_[extension->id()].being_upgraded;
}

void ExtensionService::SetBeingUpgraded(const Extension* extension,
                                         bool value) {
  extension_runtime_data_[extension->id()].being_upgraded = value;
}

bool ExtensionService::HasUsedWebRequest(const Extension* extension) {
  return extension_runtime_data_[extension->id()].has_used_webrequest;
}

void ExtensionService::SetHasUsedWebRequest(const Extension* extension,
                                            bool value) {
  extension_runtime_data_[extension->id()].has_used_webrequest = value;
}

base::PropertyBag* ExtensionService::GetPropertyBag(
    const Extension* extension) {
  return &extension_runtime_data_[extension->id()].property_bag;
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
  const content::PepperPluginInfo* pepper_info = NULL;
  std::vector<content::PepperPluginInfo> plugins;
  PepperPluginRegistry::ComputeList(&plugins);

  // Search the entire plugin list for plugins that handle the NaCl MIME type.
  // There can be multiple plugins like this, for instance the internal NaCl
  // plugin and a plugin registered by --register-pepper-plugins during tests.
  for (size_t i = 0; i < plugins.size(); ++i) {
    pepper_info = &plugins[i];
    CHECK(pepper_info);
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
        PluginService::GetInstance()->RegisterInternalPlugin(info);
        // This plugin has been modified, no need to check the rest of its
        // types, but continue checking other plugins.
        break;
      }
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
