// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_service.h"

#include <algorithm>
#include <set>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/apps_promo.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_accessibility_api.h"
#include "chrome/browser/extensions/extension_bookmarks_module.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_cookies_api.h"
#include "chrome/browser/extensions/extension_data_deleter.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_history_api.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_input_ime_api.h"
#include "chrome/browser/extensions/extension_install_ui.h"
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
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/api/sync_change.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/ntp/shown_sections_handler.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/debugger/devtools_manager.h"
#include "content/browser/plugin_process_host.h"
#include "content/browser/plugin_service.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/common/content_notification_types.h"
#include "content/common/json_value_serializer.h"
#include "content/common/notification_service.h"
#include "content/common/pepper_plugin_registry.h"
#include "googleurl/src/gurl.h"
#include "net/base/registry_controlled_domain.h"
#include "webkit/database/database_tracker.h"
#include "webkit/database/database_util.h"
#include "webkit/plugins/npapi/plugin_list.h"

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

// The following enumeration is used in histograms matching
// Extensions.ManifestReload* .  Values may be added, as long
// as existing values are not changed.
enum ManifestReloadReason {
  NOT_NEEDED = 0,  // Reload not needed.
  UNPACKED_DIR,  // Unpacked directory
  NEEDS_RELOCALIZATION,  // The local has changed since we read this extension.
  NUM_MANIFEST_RELOAD_REASONS
};

ManifestReloadReason ShouldReloadExtensionManifest(const ExtensionInfo& info) {
  // Always reload manifests of unpacked extensions, because they can change
  // on disk independent of the manifest in our prefs.
  if (info.extension_location == Extension::LOAD)
    return UNPACKED_DIR;

  // Reload the manifest if it needs to be relocalized.
  if (extension_l10n_util::ShouldRelocalizeManifest(info))
    return NEEDS_RELOCALIZATION;

  return NOT_NEEDED;
}

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

// Manages an ExtensionInstallUI for a particular extension.
class SimpleExtensionLoadPrompt : public ExtensionInstallUI::Delegate {
 public:
  SimpleExtensionLoadPrompt(Profile* profile,
                            base::WeakPtr<ExtensionService> extension_service,
                            const Extension* extension);
  ~SimpleExtensionLoadPrompt();

  void ShowPrompt();

  // ExtensionInstallUI::Delegate
  virtual void InstallUIProceed();
  virtual void InstallUIAbort(bool user_initiated);

 private:
  base::WeakPtr<ExtensionService> extension_service_;
  scoped_ptr<ExtensionInstallUI> install_ui_;
  scoped_refptr<const Extension> extension_;
};

SimpleExtensionLoadPrompt::SimpleExtensionLoadPrompt(
    Profile* profile,
    base::WeakPtr<ExtensionService> extension_service,
    const Extension* extension)
    : extension_service_(extension_service),
      install_ui_(new ExtensionInstallUI(profile)),
      extension_(extension) {
}

SimpleExtensionLoadPrompt::~SimpleExtensionLoadPrompt() {
}

void SimpleExtensionLoadPrompt::ShowPrompt() {
  install_ui_->ConfirmInstall(this, extension_);
}

void SimpleExtensionLoadPrompt::InstallUIProceed() {
  if (extension_service_.get())
    extension_service_->OnExtensionInstalled(
        extension_, false, -1);  // Not from web store.
  delete this;
}

void SimpleExtensionLoadPrompt::InstallUIAbort(bool user_initiated) {
  delete this;
}

}  // namespace

bool ExtensionService::ComponentExtensionInfo::Equals(
    const ComponentExtensionInfo& other) const {
  return other.manifest == manifest && other.root_directory == root_directory;
}

ExtensionService::ExtensionRuntimeData::ExtensionRuntimeData()
    : background_page_ready(false),
      being_upgraded(false) {
}

ExtensionService::ExtensionRuntimeData::~ExtensionRuntimeData() {
}

ExtensionService::NaClModuleInfo::NaClModuleInfo() {
}

ExtensionService::NaClModuleInfo::~NaClModuleInfo() {
}

// ExtensionService.

const char* ExtensionService::kInstallDirectoryName = "Extensions";
const char* ExtensionService::kCurrentVersionFileName = "Current Version";
const char* ExtensionService::kSettingsDirectoryName = "Extension Settings";

// Implements IO for the ExtensionService.

class ExtensionServiceBackend
    : public base::RefCountedThreadSafe<ExtensionServiceBackend> {
 public:
  // |install_directory| is a path where to look for extensions to load.
  ExtensionServiceBackend(
      base::WeakPtr<ExtensionService> frontend,
      const FilePath& install_directory);

  // Loads a single extension from |path| where |path| is the top directory of
  // a specific extension where its manifest file lives. If |prompt_for_plugins|
  // is true and the extension contains plugins, we prompt the user before
  // loading.
  // Errors are reported through ExtensionErrorReporter. On success,
  // AddExtension() is called.
  // TODO(erikkay): It might be useful to be able to load a packed extension
  // (presumably into memory) without installing it.
  void LoadSingleExtension(const FilePath &path, bool prompt_for_plugins);

 private:
  friend class base::RefCountedThreadSafe<ExtensionServiceBackend>;

  virtual ~ExtensionServiceBackend();

  // LoadSingleExtension needs to check the file access preference, which needs
  // to happen back on the UI thread, so it posts CheckExtensionFileAccess on
  // the UI thread. In turn, once that gets the pref, it goes back to the
  // file thread with LoadSingleExtensionWithFileAccess.
  void CheckExtensionFileAccess(const FilePath& extension_path,
                                bool prompt_for_plugins);
  void LoadSingleExtensionWithFileAccess(
      const FilePath &path, bool allow_file_access, bool prompt_for_plugins);

  // Notify the frontend that there was an error loading an extension.
  void ReportExtensionLoadError(const FilePath& extension_path,
                                const std::string& error);

  // Notify the frontend that an extension was installed.
  void OnLoadSingleExtension(const scoped_refptr<const Extension>& extension,
                             bool prompt_for_plugins);

  base::WeakPtr<ExtensionService> frontend_;

  // The top-level extensions directory being installed to.
  FilePath install_directory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionServiceBackend);
};

ExtensionServiceBackend::ExtensionServiceBackend(
    base::WeakPtr<ExtensionService> frontend,
    const FilePath& install_directory)
        : frontend_(frontend),
          install_directory_(install_directory) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

ExtensionServiceBackend::~ExtensionServiceBackend() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
        BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

void ExtensionServiceBackend::LoadSingleExtension(const FilePath& path_in,
                                                  bool prompt_for_plugins) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  FilePath extension_path = path_in;
  file_util::AbsolutePath(&extension_path);

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &ExtensionServiceBackend::CheckExtensionFileAccess,
                        extension_path, prompt_for_plugins));
}

void ExtensionServiceBackend::CheckExtensionFileAccess(
    const FilePath& extension_path, bool prompt_for_plugins) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string id = Extension::GenerateIdForPath(extension_path);
  // Unpacked extensions default to allowing file access, but if that has been
  // overridden, don't reset the value.
  bool allow_file_access =
      Extension::ShouldAlwaysAllowFileAccess(Extension::LOAD);
  if (frontend_->extension_prefs()->HasAllowFileAccessSetting(id))
    allow_file_access = frontend_->extension_prefs()->AllowFileAccess(id);

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this,
          &ExtensionServiceBackend::LoadSingleExtensionWithFileAccess,
          extension_path, allow_file_access, prompt_for_plugins));
}

void ExtensionServiceBackend::LoadSingleExtensionWithFileAccess(
    const FilePath& extension_path,
    bool allow_file_access,
    bool prompt_for_plugins) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  int flags = allow_file_access ?
      Extension::ALLOW_FILE_ACCESS : Extension::NO_FLAGS;
  if (Extension::ShouldDoStrictErrorChecking(Extension::LOAD))
    flags |= Extension::STRICT_ERROR_CHECKS;
  std::string error;
  scoped_refptr<const Extension> extension(extension_file_util::LoadExtension(
      extension_path,
      Extension::LOAD,
      flags,
      &error));

  if (!extension) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(
            this,
            &ExtensionServiceBackend::ReportExtensionLoadError,
            extension_path, error));
    return;
  }

  // Report this as an installed extension so that it gets remembered in the
  // prefs.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this,
          &ExtensionServiceBackend::OnLoadSingleExtension,
          extension, prompt_for_plugins));
}

void ExtensionServiceBackend::ReportExtensionLoadError(
    const FilePath& extension_path, const std::string &error) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (frontend_.get())
    frontend_->ReportExtensionLoadError(
        extension_path, error, chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR,
        true /* alert_on_error */);
}

void ExtensionServiceBackend::OnLoadSingleExtension(
    const scoped_refptr<const Extension>& extension, bool prompt_for_plugins) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (frontend_.get())
    frontend_->OnLoadSingleExtension(extension, prompt_for_plugins);
}

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
  external_extension_url_added_ |= true;
}

// If a download url matches one of these patterns and has a referrer of the
// webstore, then we're willing to treat that as a gallery download.
static const char* kAllowedDownloadURLPatterns[] = {
  "https://clients2.google.com/service/update2*",
  "https://clients2.googleusercontent.com/crx/*"
};

bool ExtensionService::IsDownloadFromGallery(const GURL& download_url,
                                             const GURL& referrer_url) {
  // Special-case the themes mini-gallery.
  // TODO(erikkay) When that gallery goes away, remove this code.
  if (IsDownloadFromMiniGallery(download_url) &&
      StartsWithASCII(referrer_url.spec(),
                      extension_urls::kMiniGalleryBrowsePrefix, false)) {
    return true;
  }

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

bool ExtensionService::IsDownloadFromMiniGallery(const GURL& download_url) {
  return StartsWithASCII(download_url.spec(),
                         extension_urls::kMiniGalleryDownloadPrefix,
                         false);  // case_sensitive
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
                                   ExtensionSettings* extension_settings,
                                   bool autoupdate_enabled,
                                   bool extensions_enabled)
    : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      method_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      profile_(profile),
      extension_prefs_(extension_prefs),
      extension_settings_(extension_settings),
      pending_extension_manager_(*ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      install_directory_(install_directory),
      extensions_enabled_(extensions_enabled),
      show_extensions_prompts_(true),
      ready_(false),
      toolbar_model_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      permissions_manager_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      apps_promo_(profile->GetPrefs()),
      event_routers_initialized_(false) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Figure out if extension installation should be enabled.
  if (command_line->HasSwitch(switches::kDisableExtensions)) {
    extensions_enabled_ = false;
  } else if (profile->GetPrefs()->GetBoolean(prefs::kDisableExtensions)) {
    extensions_enabled_ = false;
  }

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
                 NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 NotificationService::AllSources());
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

  backend_ =
      new ExtensionServiceBackend(weak_ptr_factory_.GetWeakPtr(),
                                  install_directory_);

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

void ExtensionService::UnregisterComponentExtension(
    const ComponentExtensionInfo& info) {
  RegisteredComponentExtensions new_component_extension_manifests;
  for (RegisteredComponentExtensions::iterator it =
           component_extension_manifests_.begin();
       it != component_extension_manifests_.end(); ++it) {
    if (!it->Equals(info))
      new_component_extension_manifests.push_back(*it);
  }
  component_extension_manifests_.swap(new_component_extension_manifests);
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
                 Source<Profile>(profile_));
}

void ExtensionService::InitEventRouters() {
  if (event_routers_initialized_)
    return;

  history_event_router_.reset(new ExtensionHistoryEventRouter());
  history_event_router_->ObserveProfile(profile_);
  ExtensionAccessibilityEventRouter::GetInstance()->ObserveProfile(profile_);
  browser_event_router_.reset(new ExtensionBrowserEventRouter(profile_));
  browser_event_router_->Init();
  preference_event_router_.reset(new ExtensionPreferenceEventRouter(profile_));
  bookmark_event_router_.reset(new ExtensionBookmarkEventRouter(
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
  // Lazy initialization.
  chromeos::ExtensionInputMethodEventRouter::GetInstance();

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

  LoadAllExtensions();

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
            NewRunnableFunction(
                extension_file_util::DeleteFile, extension_path, false)))
      NOTREACHED();

    return false;
  }

  // We want a silent install only for non-pending extensions and
  // pending extensions that have install_silently set.
  ExtensionInstallUI* client =
      (!is_pending_extension || pending_extension_info.install_silently()) ?
      NULL : new ExtensionInstallUI(profile_);

  scoped_refptr<CrxInstaller> installer(MakeCrxInstaller(client));
  installer->set_expected_id(id);
  if (is_pending_extension)
    installer->set_install_source(pending_extension_info.install_source());
  else if (extension)
    installer->set_install_source(extension->location());
  if (pending_extension_info.install_silently())
    installer->set_allow_silent_install(true);
  installer->set_delete_source(true);
  installer->set_original_url(download_url);
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
    ExtensionHost* host = manager->GetBackgroundHostForExtension(
        current_extension);
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
    LoadInstalledExtension(*installed_extension, false);
  } else {
    // We should always be able to remember the extension's path. If it's not in
    // the map, someone failed to update |unloaded_extension_paths_|.
    CHECK(!path.empty());
    LoadExtension(path);
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

  const Extension* extension = GetInstalledExtension(extension_id);

  // Callers should not send us nonexistent extensions.
  CHECK(extension);

  // Get hold of information we need after unloading, since the extension
  // pointer will be invalid then.
  GURL extension_url(extension->url());
  Extension::Location location(extension->location());

  // Policy change which triggers an uninstall will always set
  // |external_uninstall| to true so this is the only way to uninstall
  // managed extensions.
  if (!Extension::UserMayDisable(location) && !external_uninstall) {
    NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_UNINSTALL_NOT_ALLOWED,
        Source<Profile>(profile_),
        Details<const Extension>(extension));
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
    ExtensionSyncData extension_sync_data(*extension,
                                          IsExtensionEnabled(extension_id),
                                          IsIncognitoEnabled(extension_id));
    sync_change = extension_sync_data.GetSyncChange(SyncChange::ACTION_DELETE);
  }

  UninstalledExtensionInfo uninstalled_extension_info(*extension);

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

  extension_prefs_->OnExtensionUninstalled(extension_id, location,
                                           external_uninstall);

  // Tell the backend to start deleting installed extensions on the file thread.
  if (Extension::LOAD != location) {
    if (!BrowserThread::PostTask(
            BrowserThread::FILE, FROM_HERE,
            NewRunnableFunction(
                &extension_file_util::UninstallExtension,
                install_directory_,
                extension_id)))
      NOTREACHED();
  }

  ClearExtensionData(extension_url);
  UntrackTerminatedExtension(extension_id);

  // Notify interested parties that we've uninstalled this extension.
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
      Source<Profile>(profile_),
      Details<UninstalledExtensionInfo>(&uninstalled_extension_info));

  if (sync_bundle && sync_bundle->HasExtensionId(extension_id)) {
    sync_bundle->sync_processor->ProcessSyncChanges(
        FROM_HERE, SyncChangeList(1, sync_change));
    sync_bundle->synced_extensions.erase(extension_id);
  }

  return true;
}

void ExtensionService::ClearExtensionData(const GURL& extension_url) {
  scoped_refptr<ExtensionDataDeleter> deleter(
      new ExtensionDataDeleter(profile_, extension_url));
  deleter->StartDeleting();
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

void ExtensionService::LoadExtension(const FilePath& extension_path) {
  LoadExtension(extension_path, true);
}

void ExtensionService::LoadExtension(const FilePath& extension_path,
                                     bool prompt_for_plugins) {
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(backend_.get(),
                        &ExtensionServiceBackend::LoadSingleExtension,
                        extension_path, prompt_for_plugins));
}

void ExtensionService::LoadExtensionFromCommandLine(
    const FilePath& path_in) {

  // Load extensions from the command line synchronously to avoid a race
  // between extension loading and loading an URL from the command line.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  FilePath extension_path = path_in;
  file_util::AbsolutePath(&extension_path);

  std::string id = Extension::GenerateIdForPath(extension_path);
  bool allow_file_access =
      Extension::ShouldAlwaysAllowFileAccess(Extension::LOAD);
  if (extension_prefs()->HasAllowFileAccessSetting(id))
    allow_file_access = extension_prefs()->AllowFileAccess(id);

  int flags = Extension::NO_FLAGS;
  if (allow_file_access)
    flags |= Extension::ALLOW_FILE_ACCESS;
  if (Extension::ShouldDoStrictErrorChecking(Extension::LOAD))
    flags |= Extension::STRICT_ERROR_CHECKS;

  std::string error;
  scoped_refptr<const Extension> extension(extension_file_util::LoadExtension(
      extension_path,
      Extension::LOAD,
      flags,
      &error));

  if (!extension) {
    ReportExtensionLoadError(
        extension_path,
        error,
        chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR,
        true);
    return;
  }

  OnLoadSingleExtension(extension, false);
}

void ExtensionService::LoadComponentExtensions() {
  for (RegisteredComponentExtensions::iterator it =
           component_extension_manifests_.begin();
       it != component_extension_manifests_.end(); ++it) {
    LoadComponentExtension(*it);
  }
}

const Extension* ExtensionService::LoadComponentExtension(
    const ComponentExtensionInfo &info) {
  JSONStringValueSerializer serializer(info.manifest);
  scoped_ptr<Value> manifest(serializer.Deserialize(NULL, NULL));
  if (!manifest.get()) {
    LOG(ERROR) << "Failed to parse manifest for extension";
    return NULL;
  }

  int flags = Extension::REQUIRE_KEY;
  if (Extension::ShouldDoStrictErrorChecking(Extension::COMPONENT))
    flags |= Extension::STRICT_ERROR_CHECKS;
  std::string error;
  scoped_refptr<const Extension> extension(Extension::Create(
      info.root_directory,
      Extension::COMPONENT,
      *static_cast<DictionaryValue*>(manifest.get()),
      flags,
      &error));
  if (!extension.get()) {
    LOG(ERROR) << error;
    return NULL;
  }
  AddExtension(extension);
  return extension;
}

void ExtensionService::UnloadComponentExtension(
    const ComponentExtensionInfo& info) {
  JSONStringValueSerializer serializer(info.manifest);
  scoped_ptr<Value> manifest(serializer.Deserialize(NULL, NULL));
  if (!manifest.get()) {
    LOG(ERROR) << "Failed to parse manifest for extension";
    return;
  }
  std::string public_key;
  std::string public_key_bytes;
  std::string id;
  if (!static_cast<DictionaryValue*>(manifest.get())->
      GetString(extension_manifest_keys::kPublicKey, &public_key) ||
      !Extension::ParsePEMKeyBytes(public_key, &public_key_bytes) ||
      !Extension::GenerateId(public_key_bytes, &id)) {
    LOG(ERROR) << "Failed to get extension id";
    return;
  }
  UnloadExtension(id, extension_misc::UNLOAD_REASON_DISABLE);
}

void ExtensionService::LoadAllExtensions() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::TimeTicks start_time = base::TimeTicks::Now();

  // Load any component extensions.
  LoadComponentExtensions();

  // Load the previously installed extensions.
  scoped_ptr<ExtensionPrefs::ExtensionsInfo> extensions_info(
      extension_prefs_->GetInstalledExtensionsInfo());

  std::vector<int> reload_reason_counts(NUM_MANIFEST_RELOAD_REASONS, 0);
  bool should_write_prefs = false;

  for (size_t i = 0; i < extensions_info->size(); ++i) {
    ExtensionInfo* info = extensions_info->at(i).get();

    ManifestReloadReason reload_reason = ShouldReloadExtensionManifest(*info);
    ++reload_reason_counts[reload_reason];
    UMA_HISTOGRAM_ENUMERATION("Extensions.ManifestReloadEnumValue",
                              reload_reason, 100);

    if (reload_reason != NOT_NEEDED) {
      // Reloading and extension reads files from disk.  We do this on the
      // UI thread because reloads should be very rare, and the complexity
      // added by delaying the time when the extensions service knows about
      // all extensions is significant.  See crbug.com/37548 for details.
      // |allow_io| disables tests that file operations run on the file
      // thread.
      base::ThreadRestrictions::ScopedAllowIO allow_io;

      int flags = Extension::NO_FLAGS;
      if (Extension::ShouldDoStrictErrorChecking(info->extension_location))
        flags |= Extension::STRICT_ERROR_CHECKS;
      if (extension_prefs_->AllowFileAccess(info->extension_id))
        flags |= Extension::ALLOW_FILE_ACCESS;
      if (extension_prefs_->IsFromWebStore(info->extension_id))
        flags |= Extension::FROM_WEBSTORE;
      if (extension_prefs_->IsFromBookmark(info->extension_id))
        flags |= Extension::FROM_BOOKMARK;
      std::string error;
      scoped_refptr<const Extension> extension(
          extension_file_util::LoadExtension(
              info->extension_path,
              info->extension_location,
              flags,
              &error));

      if (extension.get()) {
        extensions_info->at(i)->extension_manifest.reset(
            static_cast<DictionaryValue*>(
                extension->manifest_value()->DeepCopy()));
        should_write_prefs = true;
      }
    }
  }

  for (size_t i = 0; i < extensions_info->size(); ++i) {
    LoadInstalledExtension(*extensions_info->at(i), should_write_prefs);
  }

  OnLoadedInstalledExtensions();

  // The histograms Extensions.ManifestReload* allow us to validate
  // the assumption that reloading manifest is a rare event.
  UMA_HISTOGRAM_COUNTS_100("Extensions.ManifestReloadNotNeeded",
                           reload_reason_counts[NOT_NEEDED]);
  UMA_HISTOGRAM_COUNTS_100("Extensions.ManifestReloadUnpackedDir",
                           reload_reason_counts[UNPACKED_DIR]);
  UMA_HISTOGRAM_COUNTS_100("Extensions.ManifestReloadNeedsRelocalization",
                           reload_reason_counts[NEEDS_RELOCALIZATION]);

  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadAll", extensions_.size());
  UMA_HISTOGRAM_COUNTS_100("Extensions.Disabled", disabled_extensions_.size());

  UMA_HISTOGRAM_TIMES("Extensions.LoadAllTime",
                      base::TimeTicks::Now() - start_time);

  int app_user_count = 0;
  int app_external_count = 0;
  int hosted_app_count = 0;
  int packaged_app_count = 0;
  int user_script_count = 0;
  int extension_user_count = 0;
  int extension_external_count = 0;
  int theme_count = 0;
  int page_action_count = 0;
  int browser_action_count = 0;
  ExtensionList::iterator ex;
  for (ex = extensions_.begin(); ex != extensions_.end(); ++ex) {
    Extension::Location location = (*ex)->location();
    Extension::Type type = (*ex)->GetType();
    if ((*ex)->is_app()) {
      UMA_HISTOGRAM_ENUMERATION("Extensions.AppLocation",
                                location, 100);
    } else if (type == Extension::TYPE_EXTENSION) {
      UMA_HISTOGRAM_ENUMERATION("Extensions.ExtensionLocation",
                                location, 100);
    }

    // Don't count component extensions, since they are only extensions as an
    // implementation detail.
    if (location == Extension::COMPONENT)
      continue;

    // Don't count unpacked extensions, since they're a developer-specific
    // feature.
    if (location == Extension::LOAD)
      continue;

    // Using an enumeration shows us the total installed ratio across all users.
    // Using the totals per user at each startup tells us the distribution of
    // usage for each user (e.g. 40% of users have at least one app installed).
    UMA_HISTOGRAM_ENUMERATION("Extensions.LoadType", type, 100);
    switch (type) {
      case Extension::TYPE_THEME:
        ++theme_count;
        break;
      case Extension::TYPE_USER_SCRIPT:
        ++user_script_count;
        break;
      case Extension::TYPE_HOSTED_APP:
        ++hosted_app_count;
        if (Extension::IsExternalLocation(location)) {
          ++app_external_count;
        } else {
          ++app_user_count;
        }
        break;
      case Extension::TYPE_PACKAGED_APP:
        ++packaged_app_count;
        if (Extension::IsExternalLocation(location)) {
          ++app_external_count;
        } else {
          ++app_user_count;
        }
        break;
      case Extension::TYPE_EXTENSION:
      default:
        if (Extension::IsExternalLocation(location)) {
          ++extension_external_count;
        } else {
          ++extension_user_count;
        }
        break;
    }
    if ((*ex)->page_action() != NULL)
      ++page_action_count;
    if ((*ex)->browser_action() != NULL)
      ++browser_action_count;

    RecordPermissionMessagesHistogram(
        ex->get(), "Extensions.Permissions_Load");
  }
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadApp",
                           app_user_count + app_external_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadAppUser", app_user_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadAppExternal", app_external_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadHostedApp", hosted_app_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadPackagedApp", packaged_app_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadExtension",
                           extension_user_count + extension_external_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadExtensionUser",
                           extension_user_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadExtensionExternal",
                           extension_external_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadUserScript", user_script_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadTheme", theme_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadPageAction", page_action_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadBrowserAction",
                           browser_action_count);
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

void ExtensionService::LoadInstalledExtension(const ExtensionInfo& info,
                                              bool write_to_prefs) {
  std::string error;
  scoped_refptr<const Extension> extension(NULL);
  if (!extension_prefs_->IsExtensionAllowedByPolicy(info.extension_id)) {
    error = errors::kDisabledByPolicy;
  } else if (info.extension_manifest.get()) {
    int flags = Extension::NO_FLAGS;
    if (info.extension_location != Extension::LOAD)
      flags |= Extension::REQUIRE_KEY;
    if (Extension::ShouldDoStrictErrorChecking(info.extension_location))
      flags |= Extension::STRICT_ERROR_CHECKS;
    if (extension_prefs_->AllowFileAccess(info.extension_id))
      flags |= Extension::ALLOW_FILE_ACCESS;
    if (extension_prefs_->IsFromWebStore(info.extension_id))
      flags |= Extension::FROM_WEBSTORE;
    if (extension_prefs_->IsFromBookmark(info.extension_id))
      flags |= Extension::FROM_BOOKMARK;
    extension = Extension::Create(
        info.extension_path,
        info.extension_location,
        *info.extension_manifest,
        flags,
        &error);
  } else {
    error = errors::kManifestUnreadable;
  }

  if (!extension) {
    ReportExtensionLoadError(info.extension_path,
                             error,
                             chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR,
                             false);
    return;
  }

  if (write_to_prefs)
    extension_prefs_->UpdateManifest(extension);

  AddExtension(extension);
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
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_LOADED,
      Source<Profile>(profile_),
      Details<const Extension>(extension));

  // Tell renderers about the new extension.
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    RenderProcessHost* host = i.GetCurrentValue();
    Profile* host_profile =
        Profile::FromBrowserContext(host->browser_context());
    if (host_profile->GetOriginalProfile() == profile_->GetOriginalProfile()) {
      host->Send(
          new ExtensionMsg_Loaded(ExtensionMsg_Loaded_Params(extension)));
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

  // TODO(mpcomplete): This ends up affecting all profiles. See crbug.com/80757.
  bool plugins_changed = false;
  for (size_t i = 0; i < extension->plugins().size(); ++i) {
    const Extension::PluginInfo& plugin = extension->plugins()[i];
    webkit::npapi::PluginList::Singleton()->RefreshPlugins();
    webkit::npapi::PluginList::Singleton()->AddExtraPluginPath(plugin.path);
    plugins_changed = true;
    if (!plugin.is_public) {
      PluginService::GetInstance()->RestrictPluginToUrl(
          plugin.path, extension->url());
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
    PluginService::GetInstance()->PurgePluginListCache(false);

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
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_UNLOADED,
      Source<Profile>(profile_),
      Details<UnloadedExtensionInfo>(&details));

  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    RenderProcessHost* host = i.GetCurrentValue();
    Profile* host_profile =
        Profile::FromBrowserContext(host->browser_context());
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
                                 NewRunnableFunction(&ForceShutdownPlugin,
                                                     plugin.path)))
      NOTREACHED();
    webkit::npapi::PluginList::Singleton()->RefreshPlugins();
    webkit::npapi::PluginList::Singleton()->RemoveExtraPluginPath(
        plugin.path);
    plugins_changed = true;
    if (!plugin.is_public)
      PluginService::GetInstance()->RestrictPluginToUrl(plugin.path, GURL());
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
    PluginService::GetInstance()->PurgePluginListCache(false);
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

ExtensionSettings* ExtensionService::extension_settings() {
  return extension_settings_;
}

ExtensionContentSettingsStore*
    ExtensionService::GetExtensionContentSettingsStore() {
  return extension_prefs()->content_settings_store();
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
    if (!extension_prefs_->IsExtensionAllowedByPolicy(extension->id()))
      to_be_removed.push_back(extension->id());
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
} // namespace

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
    ExtensionSyncData extension_sync_data(extension,
                                          IsExtensionEnabled(extension.id()),
                                          IsIncognitoEnabled(extension.id()));

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
      sync_data_list->push_back(
          ExtensionSyncData(extension,
                            IsExtensionEnabled(extension.id()),
                            IsIncognitoEnabled(extension.id())));
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
  SetIsIncognitoEnabled(id, extension_sync_data.incognito_enabled());

  const Extension* extension = GetInstalledExtension(id);
  if (extension) {
    // If the extension is already installed, check if it's outdated.
    int result = extension->version()->CompareTo(extension_sync_data.version());
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

  // If the extension is enabled (and not terminated), unload and
  // reload it to update UI.
  const Extension* enabled_extension = GetExtensionById(extension_id, false);
  if (enabled_extension) {
    NotifyExtensionUnloaded(
        enabled_extension, extension_misc::UNLOAD_REASON_DISABLE);
    NotifyExtensionLoaded(enabled_extension);
  }

  if (extension)
    SyncExtensionChangeIfNeeded(*extension);
}

bool ExtensionService::CanCrossIncognito(const Extension* extension) {
  // We allow the extension to see events and data from another profile iff it
  // uses "spanning" behavior and it has incognito access. "split" mode
  // extensions only see events for a matching profile.
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

  // Uninstall of unclaimed extensions will happen after all the providers
  // had reported ready.  Every provider calls OnExternalProviderReady()
  // when it finishes, and OnExternalProviderReady() only acts when all
  // providers are ready.  In case there are no providers, we call it
  // to trigger removal of extensions that used to have an external source.
  if (external_extension_providers_.empty())
    OnExternalProviderReady();
}

void ExtensionService::OnExternalProviderReady() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // An external provider has finished loading.  We only take action
  // if all of them are finished. So we check them first.
  ProviderCollection::const_iterator i;
  for (i = external_extension_providers_.begin();
       i != external_extension_providers_.end(); ++i) {
    ExternalExtensionProviderInterface* provider = i->get();
    if (!provider->IsReady())
      return;
  }

  // All the providers are ready.  Install any pending extensions.
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
    NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_UNLOADED,
        Source<Profile>(profile_),
        Details<UnloadedExtensionInfo>(&details));
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
  LoadAllExtensions();
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
          NewRunnableFunction(
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
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSIONS_READY,
      Source<Profile>(profile_),
      NotificationService::NoDetails());
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
    NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_UPDATE_DISABLED,
        Source<Profile>(profile_),
        Details<const Extension>(extension));
    SyncExtensionChangeIfNeeded(*extension);
    return;
  }

  // Unfortunately, we used to set app launcher indices for non-apps. If this
  // extension has an index (page or in-page), set it to -1.
  if (!extension->is_app()) {
    if (extension_prefs_->GetAppLaunchIndex(extension->id()) != -1)
      extension_prefs_->SetAppLaunchIndex(extension->id(), -1);
    if (extension_prefs_->GetPageIndex(extension->id()) != -1)
      extension_prefs_->SetPageIndex(extension->id(), -1);
  }

  extensions_.push_back(scoped_extension);
  SyncExtensionChangeIfNeeded(*extension);
  NotifyExtensionLoaded(extension);
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

void ExtensionService::OnLoadSingleExtension(const Extension* extension,
                                             bool prompt_for_plugins) {
  // If this is a new install of an extension with plugins, prompt the user
  // first.
  if (show_extensions_prompts_ && prompt_for_plugins &&
      !extension->plugins().empty() &&
      disabled_extension_paths_.find(extension->id()) ==
          disabled_extension_paths_.end()) {
    SimpleExtensionLoadPrompt* prompt = new SimpleExtensionLoadPrompt(
        profile_, weak_ptr_factory_.GetWeakPtr(), extension);
    prompt->ShowPrompt();
    return;  // continues in SimpleExtensionLoadPrompt::InstallUI*
  }
  OnExtensionInstalled(extension, false, -1);  // Not from web store.
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

      NotificationService::current()->Notify(
          chrome::NOTIFICATION_EXTENSION_INSTALL_NOT_ALLOWED,
          Source<Profile>(profile_),
          Details<const Extension>(extension));

      // Delete the extension directory since we're not going to
      // load it.
      if (!BrowserThread::PostTask(
              BrowserThread::FILE, FROM_HERE,
              NewRunnableFunction(&extension_file_util::DeleteFile,
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

  ShownSectionsHandler::OnExtensionInstalled(profile_->GetPrefs(), extension);
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

  NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_INSTALLED,
      Source<Profile>(profile_),
      Details<const Extension>(extension));

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

  UnloadExtension(extension->id(), extension_misc::UNLOAD_REASON_DISABLE);
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

bool ExtensionService::ExtensionBindingsAllowed(const GURL& url) {
  // Allow bindings for all packaged extensions.
  // Note that GetExtensionByURL may return an Extension for hosted apps
  // if the URL came from GetEffectiveURL.
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
         Extension::Location location) {
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
  scoped_refptr<CrxInstaller> installer(MakeCrxInstaller(NULL));
  installer->set_install_source(location);
  installer->set_expected_id(id);
  installer->set_expected_version(*version);
  installer->set_install_cause(extension_misc::INSTALL_CAUSE_EXTERNAL_FILE);
  installer->InstallCrx(path);
}

void ExtensionService::ReportExtensionLoadError(
    const FilePath& extension_path,
    const std::string &error,
    int type,
    bool be_noisy) {
  NotificationService* service = NotificationService::current();
  service->Notify(type,
                  Source<Profile>(profile_),
                  Details<const std::string>(&error));

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
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED: {
      if (profile_ != Source<Profile>(source).ptr()->GetOriginalProfile())
        break;

      ExtensionHost* host = Details<ExtensionHost>(details).ptr();

      // Mark the extension as terminated and Unload it. We want it to
      // be in a consistent state: either fully working or not loaded
      // at all, but never half-crashed.  We do it in a PostTask so
      // that other handlers of this notification will still have
      // access to the Extension and ExtensionHost.
      MessageLoop::current()->PostTask(
          FROM_HERE,
          method_factory_.NewRunnableMethod(
              &ExtensionService::TrackTerminatedExtension,
              host->extension()));
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_CREATED: {
      RenderProcessHost* process = Source<RenderProcessHost>(source).ptr();
      Profile* host_profile =
          Profile::FromBrowserContext(process->browser_context());
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
      for (size_t i = 0; i < extensions_.size(); ++i) {
        process->Send(new ExtensionMsg_Loaded(
            ExtensionMsg_Loaded_Params(extensions_[i])));
      }
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED: {
      RenderProcessHost* process = Source<RenderProcessHost>(source).ptr();
      Profile* host_profile =
          Profile::FromBrowserContext(process->browser_context());
      if (!profile_->IsSameProfile(host_profile->GetOriginalProfile()))
          break;

      installed_app_hosts_.erase(process->id());
      break;
    }
    case chrome::NOTIFICATION_PREF_CHANGED: {
      std::string* pref_name = Details<std::string>(details).ptr();
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
                        Source<Profile>(profile_));
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

scoped_refptr<CrxInstaller> ExtensionService::MakeCrxInstaller(
    ExtensionInstallUI* client) {
  return new CrxInstaller(weak_ptr_factory_.GetWeakPtr(), client);
}

bool ExtensionService::IsBackgroundPageReady(const Extension* extension) {
  return (extension->background_url().is_empty() ||
          extension_runtime_data_[extension->id()].background_page_ready);
}

void ExtensionService::SetBackgroundPageReady(const Extension* extension) {
  DCHECK(!extension->background_url().is_empty());
  extension_runtime_data_[extension->id()].background_page_ready = true;
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
      Source<const Extension>(extension),
      NotificationService::NoDetails());
}

bool ExtensionService::IsBeingUpgraded(const Extension* extension) {
  return extension_runtime_data_[extension->id()].being_upgraded;
}

void ExtensionService::SetBeingUpgraded(const Extension* extension,
                                         bool value) {
  extension_runtime_data_[extension->id()].being_upgraded = value;
}

PropertyBag* ExtensionService::GetPropertyBag(const Extension* extension) {
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
  const PepperPluginInfo* pepper_info = NULL;
  std::vector<PepperPluginInfo> plugins;
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

        webkit::npapi::PluginList::Singleton()->
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

        webkit::npapi::PluginList::Singleton()->RefreshPlugins();
        webkit::npapi::PluginList::Singleton()->RegisterInternalPlugin(info);
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
