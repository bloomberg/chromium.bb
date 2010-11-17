// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extensions_service.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "base/stl_util-inl.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/thread_restrictions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/dom_ui/shown_sections_handler.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/default_apps.h"
#include "chrome/browser/extensions/extension_accessibility_api.h"
#include "chrome/browser/extensions/extension_bookmarks_module.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_cookies_api.h"
#include "chrome/browser/extensions/extension_data_deleter.h"
#include "chrome/browser/extensions/extension_dom_ui.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_history_api.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_management_api.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_processes_api.h"
#include "chrome/browser/extensions/extension_updater.h"
#include "chrome/browser/extensions/extension_webnavigation_api.h"
#include "chrome/browser/extensions/external_extension_provider.h"
#include "chrome/browser/extensions/external_policy_extension_provider.h"
#include "chrome/browser/extensions/external_pref_extension_provider.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/sync/glue/extension_sync_traits.h"
#include "chrome/browser/sync/glue/extension_util.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "net/base/registry_controlled_domain.h"
#include "webkit/database/database_tracker.h"
#include "webkit/database/database_util.h"

#if defined(OS_WIN)
#include "chrome/browser/extensions/external_registry_extension_provider_win.h"
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

void GetExplicitOriginsInExtent(const Extension* extension,
                                std::vector<GURL>* origins) {
  typedef std::vector<URLPattern> PatternList;
  std::set<GURL> set;
  const PatternList& patterns = extension->web_extent().patterns();
  for (PatternList::const_iterator pattern = patterns.begin();
       pattern != patterns.end(); ++pattern) {
    if (pattern->match_subdomains() || pattern->match_all_urls())
      continue;
    // Wildcard URL schemes won't parse into a valid GURL, so explicit schemes
    // must be used.
    PatternList explicit_patterns = pattern->ConvertToExplicitSchemes();
    for (PatternList::const_iterator explicit_p = explicit_patterns.begin();
         explicit_p != explicit_patterns.end(); ++explicit_p) {
      GURL origin = GURL(explicit_p->GetAsString()).GetOrigin();
      if (origin.is_valid()) {
        set.insert(origin);
      } else {
        NOTREACHED();
      }
    }
  }

  for (std::set<GURL>::const_iterator unique = set.begin();
       unique != set.end(); ++unique) {
    origins->push_back(*unique);
  }
}

}  // namespace

PendingExtensionInfo::PendingExtensionInfo(
    const GURL& update_url,
    PendingExtensionInfo::ExpectedCrxType expected_crx_type,
    bool is_from_sync,
    bool install_silently,
    bool enable_on_install,
    bool enable_incognito_on_install,
    Extension::Location location)
    : update_url(update_url),
      expected_crx_type(expected_crx_type),
      is_from_sync(is_from_sync),
      install_silently(install_silently),
      enable_on_install(enable_on_install),
      enable_incognito_on_install(enable_incognito_on_install),
      install_source(location) {}

PendingExtensionInfo::PendingExtensionInfo()
    : update_url(),
      expected_crx_type(PendingExtensionInfo::UNKNOWN),
      is_from_sync(true),
      install_silently(false),
      enable_on_install(false),
      enable_incognito_on_install(false),
      install_source(Extension::INVALID) {}


ExtensionsService::ExtensionRuntimeData::ExtensionRuntimeData()
    : background_page_ready(false),
      being_upgraded(false) {
}

ExtensionsService::ExtensionRuntimeData::~ExtensionRuntimeData() {
}

// ExtensionsService.

const char* ExtensionsService::kInstallDirectoryName = "Extensions";
const char* ExtensionsService::kCurrentVersionFileName = "Current Version";

// Implements IO for the ExtensionsService.

class ExtensionsServiceBackend
    : public base::RefCountedThreadSafe<ExtensionsServiceBackend>,
      public ExternalExtensionProvider::Visitor {
 public:
  // |install_directory| is a path where to look for extensions to load.
  // |load_external_extensions| indicates whether or not backend should load
  // external extensions listed in JSON file and Windows registry.
  ExtensionsServiceBackend(PrefService* prefs,
                           const FilePath& install_directory,
                           bool load_external_extensions);

  // Loads a single extension from |path| where |path| is the top directory of
  // a specific extension where its manifest file lives.
  // Errors are reported through ExtensionErrorReporter. On success,
  // OnExtensionLoaded() is called.
  // TODO(erikkay): It might be useful to be able to load a packed extension
  // (presumably into memory) without installing it.
  void LoadSingleExtension(const FilePath &path,
                           scoped_refptr<ExtensionsService> frontend);

  // Check externally updated extensions for updates and install if necessary.
  // Errors are reported through ExtensionErrorReporter. Succcess is not
  // reported.
  void CheckForExternalUpdates(const std::set<std::string>& ids_to_ignore,
                               scoped_refptr<ExtensionsService> frontend);

  // For the extension in |version_path| with |id|, check to see if it's an
  // externally managed extension.  If so, tell the frontend to uninstall it.
  void CheckExternalUninstall(scoped_refptr<ExtensionsService> frontend,
                              const std::string& id);

  // Clear all ExternalExtensionProviders.
  void ClearProvidersForTesting();

  // Adds an ExternalExtensionProvider for the service to use during testing.
  // Takes ownership of |test_provider|.
  void AddProviderForTesting(ExternalExtensionProvider* test_provider);

  // ExternalExtensionProvider::Visitor implementation.
  virtual void OnExternalExtensionFileFound(const std::string& id,
                                            const Version* version,
                                            const FilePath& path,
                                            Extension::Location location);

  virtual void OnExternalExtensionUpdateUrlFound(const std::string& id,
                                                 const GURL& update_url,
                                                 Extension::Location location);

 private:
  friend class base::RefCountedThreadSafe<ExtensionsServiceBackend>;

  virtual ~ExtensionsServiceBackend();

  // Finish installing the extension in |crx_path| after it has been unpacked to
  // |unpacked_path|.  If |expected_id| is not empty, it's verified against the
  // extension's manifest before installation. If |silent| is true, there will
  // be no install confirmation dialog. |from_gallery| indicates whether the
  // crx was installed from our gallery, which results in different UI.
  //
  // Note: We take ownership of |extension|.
  void OnExtensionUnpacked(const FilePath& crx_path,
                           const FilePath& unpacked_path,
                           const Extension* extension,
                           const std::string expected_id);

  // Notify the frontend that there was an error loading an extension.
  void ReportExtensionLoadError(const FilePath& extension_path,
                                const std::string& error);

  // This is a naked pointer which is set by each entry point.
  // The entry point is responsible for ensuring lifetime.
  ExtensionsService* frontend_;

  // The top-level extensions directory being installed to.
  FilePath install_directory_;

  // Whether errors result in noisy alerts.
  bool alert_on_error_;

  // A collection of external extension providers.  Each provider reads
  // a source of external extension information.  Examples include the
  // windows registry and external_extensions.json.
  typedef std::vector<linked_ptr<ExternalExtensionProvider> >
      ProviderCollection;
  ProviderCollection external_extension_providers_;

  // Set to true by OnExternalExtensionUpdateUrlFound() when an external
  // extension URL is found.  Used in CheckForExternalUpdates() to see
  // if an update check is needed to install pending extensions.
  bool external_extension_added_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsServiceBackend);
};

ExtensionsServiceBackend::ExtensionsServiceBackend(
    PrefService* prefs,
    const FilePath& install_directory,
    bool load_external_extensions)
        : frontend_(NULL),
          install_directory_(install_directory),
          alert_on_error_(false),
          external_extension_added_(false) {
  if (!load_external_extensions)
    return;

  // TODO(aa): This ends up doing blocking IO on the UI thread because it reads
  // pref data in the ctor and that is called on the UI thread. Would be better
  // to re-read data each time we list external extensions, anyway.
  external_extension_providers_.push_back(
      linked_ptr<ExternalExtensionProvider>(
          new ExternalPrefExtensionProvider()));
#if defined(OS_WIN)
  external_extension_providers_.push_back(
      linked_ptr<ExternalExtensionProvider>(
          new ExternalRegistryExtensionProvider()));
#endif
  ExternalPolicyExtensionProvider* policy_extension_provider =
      new ExternalPolicyExtensionProvider();
  policy_extension_provider->SetPreferences(prefs);
  external_extension_providers_.push_back(
      linked_ptr<ExternalExtensionProvider>(policy_extension_provider));

}

ExtensionsServiceBackend::~ExtensionsServiceBackend() {
}

void ExtensionsServiceBackend::LoadSingleExtension(
    const FilePath& path_in, scoped_refptr<ExtensionsService> frontend) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  frontend_ = frontend;

  // Explicit UI loads are always noisy.
  alert_on_error_ = true;

  FilePath extension_path = path_in;
  file_util::AbsolutePath(&extension_path);

  std::string error;
  scoped_refptr<const Extension> extension(extension_file_util::LoadExtension(
      extension_path,
      Extension::LOAD,
      false,  // Don't require id
      &error));

  if (!extension) {
    ReportExtensionLoadError(extension_path, error);
    return;
  }

  // Report this as an installed extension so that it gets remembered in the
  // prefs.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(frontend_, &ExtensionsService::OnExtensionInstalled,
                        extension, true));
}

void ExtensionsServiceBackend::ReportExtensionLoadError(
    const FilePath& extension_path, const std::string &error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          frontend_,
          &ExtensionsService::ReportExtensionLoadError, extension_path,
          error, NotificationType::EXTENSION_INSTALL_ERROR, alert_on_error_));
}

// Some extensions will autoupdate themselves externally from Chrome.  These
// are typically part of some larger client application package.  To support
// these, the extension will register its location in the the preferences file
// (and also, on Windows, in the registry) and this code will periodically
// check that location for a .crx file, which it will then install locally if
// a new version is available.
void ExtensionsServiceBackend::CheckForExternalUpdates(
    const std::set<std::string>& ids_to_ignore,
    scoped_refptr<ExtensionsService> frontend) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Note that this installation is intentionally silent (since it didn't
  // go through the front-end).  Extensions that are registered in this
  // way are effectively considered 'pre-bundled', and so implicitly
  // trusted.  In general, if something has HKLM or filesystem access,
  // they could install an extension manually themselves anyway.
  alert_on_error_ = false;
  frontend_ = frontend;
  external_extension_added_ = false;

  // Ask each external extension provider to give us a call back for each
  // extension they know about. See OnExternalExtension(File|UpdateUrl)Found.
  ProviderCollection::const_iterator i;
  for (i = external_extension_providers_.begin();
       i != external_extension_providers_.end(); ++i) {
    ExternalExtensionProvider* provider = i->get();
    provider->VisitRegisteredExtension(this, ids_to_ignore);
  }

  if (external_extension_added_ && frontend->updater()) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
            NewRunnableMethod(
                frontend->updater(), &ExtensionUpdater::CheckNow));
  }
}

void ExtensionsServiceBackend::CheckExternalUninstall(
    scoped_refptr<ExtensionsService> frontend, const std::string& id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Check if the providers know about this extension.
  ProviderCollection::const_iterator i;
  for (i = external_extension_providers_.begin();
       i != external_extension_providers_.end(); ++i) {
    if (i->get()->HasExtension(id))
      return;  // Yup, known extension, don't uninstall.
  }

  // This is an external extension that we don't have registered.  Uninstall.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          frontend.get(), &ExtensionsService::UninstallExtension, id, true));
}

void ExtensionsServiceBackend::ClearProvidersForTesting() {
  external_extension_providers_.clear();
}

void ExtensionsServiceBackend::AddProviderForTesting(
    ExternalExtensionProvider* test_provider) {
  DCHECK(test_provider);
  external_extension_providers_.push_back(
      linked_ptr<ExternalExtensionProvider>(test_provider));
}

void ExtensionsServiceBackend::OnExternalExtensionFileFound(
    const std::string& id, const Version* version, const FilePath& path,
    Extension::Location location) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  DCHECK(version);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          frontend_, &ExtensionsService::OnExternalExtensionFileFound, id,
          version->GetString(), path, location));
}

void ExtensionsServiceBackend::OnExternalExtensionUpdateUrlFound(
    const std::string& id,
    const GURL& update_url,
    Extension::Location location) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (frontend_->GetExtensionById(id, true)) {
    // Already installed.  Do not change the update URL that the extension set.
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          frontend_,
          &ExtensionsService::AddPendingExtensionFromExternalUpdateUrl,
          id, update_url, location));
  external_extension_added_ |= true;
}

bool ExtensionsService::IsDownloadFromGallery(const GURL& download_url,
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
              GURL(download_url));

      // Otherwise, the TLD must match the TLD of the command-line url.
      download_valid = (download_tld == store_tld);
    }
  }

  return (referrer_valid && download_valid);
}

bool ExtensionsService::IsDownloadFromMiniGallery(const GURL& download_url) {
  return StartsWithASCII(download_url.spec(),
                         extension_urls::kMiniGalleryDownloadPrefix,
                         false);  // case_sensitive
}

// static
bool ExtensionsService::UninstallExtensionHelper(
    ExtensionsService* extensions_service,
    const std::string& extension_id) {
  DCHECK(extensions_service);

  // We can't call UninstallExtension with an invalid extension ID, so check it
  // first.
  if (extensions_service->GetExtensionById(extension_id, true)) {
    extensions_service->UninstallExtension(extension_id, false);
  } else {
    LOG(WARNING) << "Attempted uninstallation of non-existent extension with "
      << "id: " << extension_id;
    return false;
  }

  return true;
}

ExtensionsService::ExtensionsService(Profile* profile,
                                     const CommandLine* command_line,
                                     const FilePath& install_directory,
                                     bool autoupdate_enabled)
    : profile_(profile),
      extension_prefs_(new ExtensionPrefs(profile->GetPrefs(),
                                          install_directory)),
      install_directory_(install_directory),
      extensions_enabled_(true),
      show_extensions_prompts_(true),
      ready_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(toolbar_model_(this)),
      default_apps_(profile->GetPrefs()),
      event_routers_initialized_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Figure out if extension installation should be enabled.
  if (command_line->HasSwitch(switches::kDisableExtensions)) {
    extensions_enabled_ = false;
  } else if (profile->GetPrefs()->GetBoolean(prefs::kDisableExtensions)) {
    extensions_enabled_ = false;
  }

  registrar_.Add(this, NotificationType::EXTENSION_PROCESS_TERMINATED,
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
    updater_ = new ExtensionUpdater(this,
                                    profile->GetPrefs(),
                                    update_frequency);
  }

  backend_ = new ExtensionsServiceBackend(profile->GetPrefs(),
                                          install_directory_,
                                          extensions_enabled_);

  // Use monochrome icons for Omnibox icons.
  omnibox_popup_icon_manager_.set_monochrome(true);
  omnibox_icon_manager_.set_monochrome(true);
  omnibox_icon_manager_.set_padding(gfx::Insets(0, kOmniboxIconPaddingLeft,
                                                0, kOmniboxIconPaddingRight));
}

ExtensionsService::~ExtensionsService() {
  DCHECK(!profile_);  // Profile should have told us it's going away.
  UnloadAllExtensions();
  if (updater_.get()) {
    updater_->Stop();
  }
}

void ExtensionsService::InitEventRouters() {
  if (event_routers_initialized_)
    return;

  ExtensionHistoryEventRouter::GetInstance()->ObserveProfile(profile_);
  ExtensionAccessibilityEventRouter::GetInstance()->ObserveProfile(profile_);
  ExtensionBrowserEventRouter::GetInstance()->Init(profile_);
  ExtensionBookmarkEventRouter::GetSingleton()->Observe(
      profile_->GetBookmarkModel());
  ExtensionCookiesEventRouter::GetInstance()->Init();
  ExtensionManagementEventRouter::GetInstance()->Init();
  ExtensionProcessesEventRouter::GetInstance()->ObserveProfile(profile_);
  ExtensionWebNavigationEventRouter::GetInstance()->Init();
  event_routers_initialized_ = true;
}

void ExtensionsService::Init() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK(!ready_);
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

void ExtensionsService::InstallExtension(const FilePath& extension_path) {
  scoped_refptr<CrxInstaller> installer(
      new CrxInstaller(install_directory_,
                       this,  // frontend
                       NULL));  // no client (silent install)
  installer->set_allow_privilege_increase(true);
  installer->InstallCrx(extension_path);
}

namespace {
  // TODO(akalin): Put this somewhere where both crx_installer.cc and
  // this file can use it.
  void DeleteFileHelper(const FilePath& path, bool recursive) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    file_util::Delete(path, recursive);
  }
}  // namespace

void ExtensionsService::UpdateExtension(const std::string& id,
                                        const FilePath& extension_path,
                                        const GURL& download_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  PendingExtensionMap::const_iterator it = pending_extensions_.find(id);
  bool is_pending_extension = (it != pending_extensions_.end());

  if (!is_pending_extension &&
      !GetExtensionByIdInternal(id, true, true)) {
    LOG(WARNING) << "Will not update extension " << id
                 << " because it is not installed or pending";
    // Delete extension_path since we're not creating a CrxInstaller
    // that would do it for us.
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableFunction(&DeleteFileHelper, extension_path, false));
    return;
  }

  // We want a silent install only for non-pending extensions and
  // pending extensions that have install_silently set.
  ExtensionInstallUI* client =
      (!is_pending_extension || it->second.install_silently) ?
      NULL : new ExtensionInstallUI(profile_);

  scoped_refptr<CrxInstaller> installer(
      new CrxInstaller(install_directory_,
                       this,  // frontend
                       client));
  installer->set_expected_id(id);
  if (is_pending_extension)
    installer->set_install_source(it->second.install_source);
  installer->set_delete_source(true);
  installer->set_original_url(download_url);
  installer->InstallCrx(extension_path);
}

void ExtensionsService::AddPendingExtensionFromSync(
    const std::string& id, const GURL& update_url,
    PendingExtensionInfo::ExpectedCrxType expected_crx_type,
    bool install_silently, bool enable_on_install,
    bool enable_incognito_on_install) {
  if (GetExtensionByIdInternal(id, true, true)) {
    LOG(DFATAL) << "Trying to add pending extension " << id
                << " which already exists";
    return;
  }

  AddPendingExtensionInternal(id, update_url, expected_crx_type, true,
                              install_silently, enable_on_install,
                              enable_incognito_on_install,
                              Extension::INTERNAL);
}

void ExtensionsService::AddPendingExtensionFromExternalUpdateUrl(
    const std::string& id, const GURL& update_url,
    Extension::Location location) {
  // Add the extension to this list of extensions to update.
  const PendingExtensionInfo::ExpectedCrxType kExpectedCrxType =
      PendingExtensionInfo::UNKNOWN;
  const bool kIsFromSync = false;
  const bool kInstallSilently = true;
  const bool kEnableOnInstall = true;
  const bool kEnableIncognitoOnInstall = false;

  if (GetExtensionByIdInternal(id, true, true)) {
    LOG(DFATAL) << "Trying to add extension " << id
                << " by external update, but it is already installed.";
    return;
  }

  AddPendingExtensionInternal(id, update_url, kExpectedCrxType, kIsFromSync,
                              kInstallSilently, kEnableOnInstall,
                              kEnableIncognitoOnInstall,
                              location);
}

void ExtensionsService::AddPendingExtensionFromDefaultAppList(
    const std::string& id) {
  // Add the extension to this list of extensions to update.
  const PendingExtensionInfo::ExpectedCrxType kExpectedCrxType =
      PendingExtensionInfo::APP;
  const bool kIsFromSync = false;
  const bool kInstallSilently = true;
  const bool kEnableOnInstall = true;
  const bool kEnableIncognitoOnInstall = true;

  // This can legitimately happen if the user manually installed one of the
  // default apps before this code ran.
  if (GetExtensionByIdInternal(id, true, true))
    return;

  AddPendingExtensionInternal(id, GURL(), kExpectedCrxType, kIsFromSync,
                              kInstallSilently, kEnableOnInstall,
                              kEnableIncognitoOnInstall,
                              Extension::INTERNAL);
}

void ExtensionsService::AddPendingExtensionInternal(
    const std::string& id, const GURL& update_url,
    PendingExtensionInfo::ExpectedCrxType expected_crx_type,
    bool is_from_sync, bool install_silently,
    bool enable_on_install, bool enable_incognito_on_install,
    Extension::Location install_source) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If a non-sync update is pending, a sync request should not
  // overwrite it.  This is important for external extensions.
  // If an external extension download is pending, and the user has
  // the extension in their sync profile, the install should set the
  // type to be external.  An external extension should not be
  // rejected if it fails the safty checks for a syncable extension.
  // TODO(skerner): Work out other potential overlapping conditions.
  // (crbug.com/61000)
  PendingExtensionMap::iterator it = pending_extensions_.find(id);
  if (it != pending_extensions_.end()) {
    VLOG(1) << "Extension id " << id
            << " was entered for update more than once."
            << "  old is_from_sync = " << it->second.is_from_sync
            << "  new is_from_sync = " << is_from_sync;
    if (!it->second.is_from_sync && is_from_sync)
      return;
  }


  pending_extensions_[id] =
      PendingExtensionInfo(update_url, expected_crx_type, is_from_sync,
                           install_silently, enable_on_install,
                           enable_incognito_on_install, install_source);
}

void ExtensionsService::ReloadExtension(const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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

void ExtensionsService::UninstallExtension(const std::string& extension_id,
                                           bool external_uninstall) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const Extension* extension =
      GetExtensionByIdInternal(extension_id, true, true);

  // Callers should not send us nonexistent extensions.
  DCHECK(extension);

  // Get hold of information we need after unloading, since the extension
  // pointer will be invalid then.
  GURL extension_url(extension->url());
  Extension::Location location(extension->location());
  UninstalledExtensionInfo uninstalled_extension_info(*extension);

  UMA_HISTOGRAM_ENUMERATION("Extensions.UninstallType",
                            extension->GetHistogramType(), 100);

  // Also copy the extension identifier since the reference might have been
  // obtained via Extension::id().
  std::string extension_id_copy(extension_id);

  if (profile_->GetTemplateURLModel())
    profile_->GetTemplateURLModel()->UnregisterExtensionKeyword(extension);

  // Unload before doing more cleanup to ensure that nothing is hanging on to
  // any of these resources.
  UnloadExtension(extension_id);

  extension_prefs_->OnExtensionUninstalled(extension_id_copy, location,
                                           external_uninstall);

  // Tell the backend to start deleting installed extensions on the file thread.
  if (Extension::LOAD != location) {
    BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableFunction(
          &extension_file_util::UninstallExtension,
          install_directory_,
          extension_id_copy));
  }

  ClearExtensionData(extension_url);

  // Notify interested parties that we've uninstalled this extension.
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_UNINSTALLED,
      Source<Profile>(profile_),
      Details<UninstalledExtensionInfo>(&uninstalled_extension_info));
}

void ExtensionsService::ClearExtensionData(const GURL& extension_url) {
  scoped_refptr<ExtensionDataDeleter> deleter(
      new ExtensionDataDeleter(profile_, extension_url));
  deleter->StartDeleting();
}

void ExtensionsService::EnableExtension(const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const Extension* extension =
      GetExtensionByIdInternal(extension_id, false, true);
  if (!extension) {
    return;
  }

  extension_prefs_->SetExtensionState(extension, Extension::ENABLED);

  // Move it over to the enabled list.
  extensions_.push_back(make_scoped_refptr(extension));
  ExtensionList::iterator iter = std::find(disabled_extensions_.begin(),
                                           disabled_extensions_.end(),
                                           extension);
  disabled_extensions_.erase(iter);

  ExtensionDOMUI::RegisterChromeURLOverrides(profile_,
      extension->GetChromeURLOverrides());

  NotifyExtensionLoaded(extension);
  UpdateActiveExtensionsInCrashReporter();
}

void ExtensionsService::DisableExtension(const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const Extension* extension =
      GetExtensionByIdInternal(extension_id, true, false);
  // The extension may have been disabled already.
  if (!extension)
    return;

  extension_prefs_->SetExtensionState(extension, Extension::DISABLED);

  // Move it over to the disabled list.
  disabled_extensions_.push_back(make_scoped_refptr(extension));
  ExtensionList::iterator iter = std::find(extensions_.begin(),
                                           extensions_.end(),
                                           extension);
  extensions_.erase(iter);

  ExtensionDOMUI::UnregisterChromeURLOverrides(profile_,
      extension->GetChromeURLOverrides());

  NotifyExtensionUnloaded(extension);
  UpdateActiveExtensionsInCrashReporter();
}

void ExtensionsService::LoadExtension(const FilePath& extension_path) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          backend_.get(),
          &ExtensionsServiceBackend::LoadSingleExtension,
          extension_path, scoped_refptr<ExtensionsService>(this)));
}

void ExtensionsService::LoadComponentExtensions() {
  for (RegisteredComponentExtensions::iterator it =
           component_extension_manifests_.begin();
       it != component_extension_manifests_.end(); ++it) {
    JSONStringValueSerializer serializer(it->manifest);
    scoped_ptr<Value> manifest(serializer.Deserialize(NULL, NULL));
    if (!manifest.get()) {
      DLOG(ERROR) << "Failed to parse manifest for extension";
      continue;
    }

    std::string error;
    scoped_refptr<const Extension> extension(Extension::Create(
        it->root_directory,
        Extension::COMPONENT,
        *static_cast<DictionaryValue*>(manifest.get()),
        true,  // require key
        &error));
    if (!extension.get()) {
      NOTREACHED() << error;
      return;
    }

    OnExtensionLoaded(extension, false);  // Don't allow privilege increase.
  }
}

void ExtensionsService::LoadAllExtensions() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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

      std::string error;
      scoped_refptr<const Extension> extension(
          extension_file_util::LoadExtension(
              info->extension_path, info->extension_location, false, &error));

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

  int app_count = 0;
  int hosted_app_count = 0;
  int packaged_app_count = 0;
  int user_script_count = 0;
  int extension_count = 0;
  int theme_count = 0;
  int external_count = 0;
  int page_action_count = 0;
  int browser_action_count = 0;
  ExtensionList::iterator ex;
  for (ex = extensions_.begin(); ex != extensions_.end(); ++ex) {
    Extension::Location location = (*ex)->location();
    Extension::HistogramType type = (*ex)->GetHistogramType();
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
        ++app_count;
        ++hosted_app_count;
        break;
      case Extension::TYPE_PACKAGED_APP:
        ++app_count;
        ++packaged_app_count;
        break;
      case Extension::TYPE_EXTENSION:
      default:
        ++extension_count;
        break;
    }
    if (Extension::IsExternalLocation(location))
      ++external_count;
    if ((*ex)->page_action() != NULL)
      ++page_action_count;
    if ((*ex)->browser_action() != NULL)
      ++browser_action_count;
  }
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadApp", app_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadHostedApp", hosted_app_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadPackagedApp", packaged_app_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadExtension", extension_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadUserScript", user_script_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadTheme", theme_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadExternal", external_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadPageAction", page_action_count);
  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadBrowserAction",
                           browser_action_count);
}


void ExtensionsService::LoadInstalledExtension(const ExtensionInfo& info,
                                               bool write_to_prefs) {
  std::string error;
  scoped_refptr<const Extension> extension(NULL);
  if (!extension_prefs_->IsExtensionAllowedByPolicy(info.extension_id)) {
    error = errors::kDisabledByPolicy;
  } else if (info.extension_manifest.get()) {
    bool require_key = info.extension_location != Extension::LOAD;
    extension = Extension::Create(
        info.extension_path, info.extension_location, *info.extension_manifest,
        require_key, &error);
  } else {
    error = errors::kManifestUnreadable;
  }

  if (!extension) {
    ReportExtensionLoadError(info.extension_path,
                             error,
                             NotificationType::EXTENSION_INSTALL_ERROR,
                             false);
    return;
  }

  if (write_to_prefs)
    extension_prefs_->UpdateManifest(extension);

  OnExtensionLoaded(extension, true);

  if (Extension::IsExternalLocation(info.extension_location)) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(
            backend_.get(),
            &ExtensionsServiceBackend::CheckExternalUninstall,
            scoped_refptr<ExtensionsService>(this),
            info.extension_id));
  }
}

void ExtensionsService::NotifyExtensionLoaded(const Extension* extension) {
  // The ChromeURLRequestContexts need to be first to know that the extension
  // was loaded, otherwise a race can arise where a renderer that is created
  // for the extension may try to load an extension URL with an extension id
  // that the request context doesn't yet know about. The profile is responsible
  // for ensuring its URLRequestContexts appropriately discover the loaded
  // extension.
  if (profile_) {
    profile_->RegisterExtensionWithRequestContexts(extension);

    // Check if this permission requires unlimited storage quota
    if (extension->HasApiPermission(Extension::kUnlimitedStoragePermission))
      GrantUnlimitedStorage(extension);

    // If the extension is an app, protect its local storage from
    // "Clear browsing data."
    if (extension->is_app())
      GrantProtectedStorage(extension);
  }

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_LOADED,
      Source<Profile>(profile_),
      Details<const Extension>(extension));
}

void ExtensionsService::NotifyExtensionUnloaded(const Extension* extension) {
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_UNLOADED,
      Source<Profile>(profile_),
      Details<const Extension>(extension));

  if (profile_) {
    profile_->UnregisterExtensionWithRequestContexts(extension);

    // Check if this permission required unlimited storage quota, reset its
    // in-memory quota.
    if (extension->HasApiPermission(Extension::kUnlimitedStoragePermission))
      RevokeUnlimitedStorage(extension);

    // If this is an app, then stop protecting its storage so it can be deleted.
    if (extension->is_app())
      RevokeProtectedStorage(extension);
  }
}

void ExtensionsService::GrantProtectedStorage(const Extension* extension) {
  DCHECK(extension->is_app()) << "Only Apps are allowed protected storage.";
  std::vector<GURL> origins;
  GetExplicitOriginsInExtent(extension, &origins);
  for (size_t i = 0; i < origins.size(); ++i)
    ++protected_storage_map_[origins[i]];
}

void ExtensionsService::RevokeProtectedStorage(const Extension* extension) {
  DCHECK(extension->is_app()) << "Attempting to revoke protected storage from "
      << " a non-app extension.";
  std::vector<GURL> origins;
  GetExplicitOriginsInExtent(extension, &origins);
  for (size_t i = 0; i < origins.size(); ++i) {
    const GURL& origin = origins[i];
    DCHECK(protected_storage_map_[origin] > 0);
    if (--protected_storage_map_[origin] <= 0)
      protected_storage_map_.erase(origin);
  }
}

void ExtensionsService::GrantUnlimitedStorage(const Extension* extension) {
  DCHECK(extension->HasApiPermission(Extension::kUnlimitedStoragePermission));
  std::vector<GURL> origins;
  GetExplicitOriginsInExtent(extension, &origins);
  origins.push_back(extension->url());

  for (size_t i = 0; i < origins.size(); ++i) {
    const GURL& origin = origins[i];
    if (++unlimited_storage_map_[origin] == 1) {
      string16 origin_identifier =
          webkit_database::DatabaseUtil::GetOriginIdentifier(origin);
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          NewRunnableMethod(
              profile_->GetDatabaseTracker(),
              &webkit_database::DatabaseTracker::SetOriginQuotaInMemory,
              origin_identifier,
              kint64max));
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          NewRunnableMethod(
              profile_->GetAppCacheService(),
              &ChromeAppCacheService::SetOriginQuotaInMemory,
              origin,
              kint64max));
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          NewRunnableMethod(
              profile_->GetFileSystemContext(),
              &BrowserFileSystemContext::SetOriginQuotaUnlimited,
              origin));
    }
  }
}

void ExtensionsService::RevokeUnlimitedStorage(const Extension* extension) {
  DCHECK(extension->HasApiPermission(Extension::kUnlimitedStoragePermission));
  std::vector<GURL> origins;
  GetExplicitOriginsInExtent(extension, &origins);
  origins.push_back(extension->url());

  for (size_t i = 0; i < origins.size(); ++i) {
    const GURL& origin = origins[i];
    DCHECK(unlimited_storage_map_[origin] > 0);
    if (--unlimited_storage_map_[origin] == 0) {
      unlimited_storage_map_.erase(origin);
      string16 origin_identifier =
          webkit_database::DatabaseUtil::GetOriginIdentifier(origin);
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          NewRunnableMethod(
              profile_->GetDatabaseTracker(),
              &webkit_database::DatabaseTracker::ResetOriginQuotaInMemory,
              origin_identifier));
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          NewRunnableMethod(
              profile_->GetAppCacheService(),
              &ChromeAppCacheService::ResetOriginQuotaInMemory,
              origin));
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          NewRunnableMethod(
              profile_->GetFileSystemContext(),
              &BrowserFileSystemContext::ResetOriginQuotaUnlimited,
              origin));
    }
  }
}

void ExtensionsService::UpdateExtensionBlacklist(
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
    UnloadExtension(to_be_removed[i]);
  }
}

void ExtensionsService::DestroyingProfile() {
  pref_change_registrar_.RemoveAll();
  profile_ = NULL;
  toolbar_model_.DestroyingProfile();
}

void ExtensionsService::CheckAdminBlacklist() {
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
    UnloadExtension(to_be_removed[i]);
}

bool ExtensionsService::IsIncognitoEnabled(const Extension* extension) {
  // If this is a component extension we always allow it to work in incognito
  // mode.
  if (extension->location() == Extension::COMPONENT)
    return true;

  // Check the prefs.
  return extension_prefs_->IsIncognitoEnabled(extension->id());
}

void ExtensionsService::SetIsIncognitoEnabled(const Extension* extension,
                                              bool enabled) {
  extension_prefs_->SetIsIncognitoEnabled(extension->id(), enabled);

  // Broadcast unloaded and loaded events to update browser state. Only bother
  // if the extension is actually enabled, since there is no UI otherwise.
  bool is_enabled = std::find(extensions_.begin(), extensions_.end(),
                              extension) != extensions_.end();
  if (is_enabled) {
    NotifyExtensionUnloaded(extension);
    NotifyExtensionLoaded(extension);
  }
}

bool ExtensionsService::CanCrossIncognito(const Extension* extension) {
  // We allow the extension to see events and data from another profile iff it
  // uses "spanning" behavior and it has incognito access. "split" mode
  // extensions only see events for a matching profile.
  return IsIncognitoEnabled(extension) && !extension->incognito_split_mode();
}

bool ExtensionsService::AllowFileAccess(const Extension* extension) {
  return (CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDisableExtensionsFileAccessCheck) ||
          extension_prefs_->AllowFileAccess(extension->id()));
}

void ExtensionsService::SetAllowFileAccess(const Extension* extension,
                                           bool allow) {
  extension_prefs_->SetAllowFileAccess(extension->id(), allow);
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_USER_SCRIPTS_UPDATED,
      Source<Profile>(profile_),
      Details<const Extension>(extension));
}

void ExtensionsService::CheckForExternalUpdates() {
  // This installs or updates externally provided extensions.
  // TODO(aa): Why pass this list into the provider, why not just filter it
  // later?
  std::set<std::string> killed_extensions;
  extension_prefs_->GetKilledExtensionIds(&killed_extensions);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          backend_.get(), &ExtensionsServiceBackend::CheckForExternalUpdates,
          killed_extensions, scoped_refptr<ExtensionsService>(this)));
}

void ExtensionsService::UnloadExtension(const std::string& extension_id) {
  // Make sure the extension gets deleted after we return from this function.
  scoped_refptr<const Extension> extension(
      GetExtensionByIdInternal(extension_id, true, true));

  // This method can be called via PostTask, so the extension may have been
  // unloaded by the time this runs.
  if (!extension)
    return;

  // Keep information about the extension so that we can reload it later
  // even if it's not permanently installed.
  unloaded_extension_paths_[extension->id()] = extension->path();

  // Clean up if the extension is meant to be enabled after a reload.
  disabled_extension_paths_.erase(extension->id());

  // Clean up runtime data.
  extension_runtime_data_.erase(extension_id);

  ExtensionDOMUI::UnregisterChromeURLOverrides(profile_,
      extension->GetChromeURLOverrides());

  ExtensionList::iterator iter = std::find(disabled_extensions_.begin(),
                                           disabled_extensions_.end(),
                                           extension.get());
  if (iter != disabled_extensions_.end()) {
    disabled_extensions_.erase(iter);
    NotificationService::current()->Notify(
        NotificationType::EXTENSION_UNLOADED_DISABLED,
        Source<Profile>(profile_),
        Details<const Extension>(extension.get()));
    return;
  }

  iter = std::find(extensions_.begin(), extensions_.end(), extension.get());

  // Remove the extension from our list.
  extensions_.erase(iter);

  NotifyExtensionUnloaded(extension.get());
  UpdateActiveExtensionsInCrashReporter();
}

void ExtensionsService::UnloadAllExtensions() {
  extensions_.clear();
  disabled_extensions_.clear();
  extension_runtime_data_.clear();

  // TODO(erikkay) should there be a notification for this?  We can't use
  // EXTENSION_UNLOADED since that implies that the extension has been disabled
  // or uninstalled, and UnloadAll is just part of shutdown.
}

void ExtensionsService::ReloadExtensions() {
  UnloadAllExtensions();
  LoadAllExtensions();
}

void ExtensionsService::GarbageCollectExtensions() {
  if (extension_prefs_->pref_service()->read_only())
    return;

  scoped_ptr<ExtensionPrefs::ExtensionsInfo> info(
      extension_prefs_->GetInstalledExtensionsInfo());

  std::map<std::string, FilePath> extension_paths;
  for (size_t i = 0; i < info->size(); ++i)
    extension_paths[info->at(i)->extension_id] = info->at(i)->extension_path;

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableFunction(
          &extension_file_util::GarbageCollectExtensions, install_directory_,
          extension_paths));

  // Also garbage-collect themes.  We check |profile_| to be
  // defensive; in the future, we may call GarbageCollectExtensions()
  // from somewhere other than Init() (e.g., in a timer).
  if (profile_) {
    profile_->GetThemeProvider()->RemoveUnusedThemes();
  }
}

void ExtensionsService::OnLoadedInstalledExtensions() {
  ready_ = true;
  if (updater_.get()) {
    updater_->Start();
  }
  NotificationService::current()->Notify(
      NotificationType::EXTENSIONS_READY,
      Source<Profile>(profile_),
      NotificationService::NoDetails());
}

void ExtensionsService::OnExtensionLoaded(const Extension* extension,
                                          bool allow_privilege_increase) {
  // Ensure extension is deleted unless we transfer ownership.
  scoped_refptr<const Extension> scoped_extension(extension);

  // The extension is now loaded, remove its data from unloaded extension map.
  unloaded_extension_paths_.erase(extension->id());

  // If the extension was disabled for a reload, then enable it.
  if (disabled_extension_paths_.erase(extension->id()) > 0)
    EnableExtension(extension->id());

  // TODO(aa): Need to re-evaluate this branch. Does this still make sense now
  // that extensions are enabled by default?
  if (extensions_enabled() ||
      extension->is_theme() ||
      extension->location() == Extension::LOAD ||
      extension->location() == Extension::COMPONENT ||
      Extension::IsExternalLocation(extension->location())) {
    const Extension* old = GetExtensionByIdInternal(extension->id(),
                                                    true, true);
    if (old) {
      // CrxInstaller should have guaranteed that we aren't downgrading.
      CHECK(extension->version()->CompareTo(*(old->version())) >= 0);

      bool allow_silent_upgrade =
          allow_privilege_increase || !Extension::IsPrivilegeIncrease(
              old, extension);

      // Extensions get upgraded if silent upgrades are allowed, otherwise
      // they get disabled.
      if (allow_silent_upgrade) {
        SetBeingUpgraded(old, true);
        SetBeingUpgraded(extension, true);
      }

      // To upgrade an extension in place, unload the old one and
      // then load the new one.
      UnloadExtension(old->id());
      old = NULL;

      if (!allow_silent_upgrade) {
        // Extension has changed permissions significantly. Disable it. We
        // send a notification below.
        extension_prefs_->SetExtensionState(extension, Extension::DISABLED);
        extension_prefs_->SetDidExtensionEscalatePermissions(extension, true);
      }
    }

    switch (extension_prefs_->GetExtensionState(extension->id())) {
      case Extension::ENABLED:
        extensions_.push_back(scoped_extension);

        NotifyExtensionLoaded(extension);

        ExtensionDOMUI::RegisterChromeURLOverrides(profile_,
            extension->GetChromeURLOverrides());
        break;
      case Extension::DISABLED:
        disabled_extensions_.push_back(scoped_extension);
        NotificationService::current()->Notify(
            NotificationType::EXTENSION_UPDATE_DISABLED,
            Source<Profile>(profile_),
            Details<const Extension>(extension));
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  SetBeingUpgraded(extension, false);

  UpdateActiveExtensionsInCrashReporter();

  if (profile_->GetTemplateURLModel())
    profile_->GetTemplateURLModel()->RegisterExtensionKeyword(extension);

  // Load the icon for omnibox-enabled extensions so it will be ready to display
  // in the URL bar.
  if (!extension->omnibox_keyword().empty()) {
    omnibox_popup_icon_manager_.LoadIcon(extension);
    omnibox_icon_manager_.LoadIcon(extension);
  }
}

void ExtensionsService::UpdateActiveExtensionsInCrashReporter() {
  std::set<std::string> extension_ids;
  for (size_t i = 0; i < extensions_.size(); ++i) {
    if (!extensions_[i]->is_theme() &&
        extensions_[i]->location() != Extension::COMPONENT)
      extension_ids.insert(extensions_[i]->id());
  }

  child_process_logging::SetActiveExtensions(extension_ids);
}

void ExtensionsService::OnExtensionInstalled(const Extension* extension,
                                             bool allow_privilege_increase) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Ensure extension is deleted unless we transfer ownership.
  scoped_refptr<const Extension> scoped_extension(extension);
  Extension::State initial_state = Extension::DISABLED;
  bool initial_enable_incognito = false;
  PendingExtensionMap::iterator it =
      pending_extensions_.find(extension->id());
  if (it != pending_extensions_.end()) {
    PendingExtensionInfo pending_extension_info = it->second;
    PendingExtensionInfo::ExpectedCrxType expected_crx_type =
        pending_extension_info.expected_crx_type;
    bool is_from_sync = pending_extension_info.is_from_sync;
    pending_extensions_.erase(it);
    it = pending_extensions_.end();

    // Set initial state from pending extension data.
    PendingExtensionInfo::ExpectedCrxType actual_crx_type =
        PendingExtensionInfo::EXTENSION;
    if (extension->is_app())
      actual_crx_type = PendingExtensionInfo::APP;
    else if (extension->is_theme())
      actual_crx_type = PendingExtensionInfo::THEME;

    if (expected_crx_type != PendingExtensionInfo::UNKNOWN &&
        expected_crx_type != actual_crx_type) {
      LOG(WARNING)
          << "Not installing pending extension " << extension->id()
          << " with is_theme = " << extension->is_theme();
      // Delete the extension directory since we're not going to
      // load it.
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          NewRunnableFunction(&DeleteFileHelper, extension->path(), true));
      return;
    }

    // If |extension| is not syncable, and was installed via sync, disallow
    // the instanation.
    //
    // Themes are always allowed.  Because they contain no active code, they
    // are less of a risk than extensions.
    //
    // If |is_from_sync| is false, then the install was not initiated by sync,
    // and this check should pass.  Extensions that were installed from an
    // update URL in external_extensions.json are an example.  They are not
    // syncable, because the user did not make an explicit choice to install
    // them.  However, they were installed through the update mechanism, so
    // control must pass into this function.
    //
    // TODO(akalin): When we do apps sync, we have to work with its
    // traits, too.
    const browser_sync::ExtensionSyncTraits extension_sync_traits =
        browser_sync::GetExtensionSyncTraits();
    const browser_sync::ExtensionSyncTraits app_sync_traits =
        browser_sync::GetAppSyncTraits();
    // If an extension is a theme, we bypass the valid/syncable check
    // as themes are harmless.
    if (!extension->is_theme() && is_from_sync &&
        !browser_sync::IsExtensionValidAndSyncable(
            *extension, extension_sync_traits.allowed_extension_types) &&
        !browser_sync::IsExtensionValidAndSyncable(
            *extension, app_sync_traits.allowed_extension_types)) {
      // We're an extension installed via sync that is unsyncable,
      // i.e. we may have been syncable previously.  We block these
      // installs.  We'll have to update the clause above if we decide
      // to sync other extension-like things, like apps or user
      // scripts.
      //
      // Note that this creates a small window where a user who tries
      // to download/install an extension that is simultaneously
      // installed via sync (and blocked) will find his download
      // blocked.
      //
      // TODO(akalin): Remove this check once we've put in UI to
      // approve synced extensions.
      LOG(WARNING)
          << "Not installing invalid or unsyncable extension "
          << extension->id();
      // Delete the extension directory since we're not going to
      // load it.
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          NewRunnableFunction(&DeleteFileHelper, extension->path(), true));
      return;
    }
    if (extension->is_theme()) {
      DCHECK(pending_extension_info.enable_on_install);
      initial_state = Extension::ENABLED;
      DCHECK(!pending_extension_info.enable_incognito_on_install);
      initial_enable_incognito = false;
    } else {
      initial_state =
          pending_extension_info.enable_on_install ?
          Extension::ENABLED : Extension::DISABLED;
      initial_enable_incognito =
          pending_extension_info.enable_incognito_on_install;
    }
  } else {
    // Make sure we preserve enabled/disabled states.
    Extension::State existing_state =
        extension_prefs_->GetExtensionState(extension->id());
    initial_state =
        (existing_state == Extension::DISABLED) ?
        Extension::DISABLED : Extension::ENABLED;
    initial_enable_incognito =
        extension_prefs_->IsIncognitoEnabled(extension->id());
  }

  UMA_HISTOGRAM_ENUMERATION("Extensions.InstallType",
                            extension->GetHistogramType(), 100);
  ShownSectionsHandler::OnExtensionInstalled(profile_->GetPrefs(), extension);
  extension_prefs_->OnExtensionInstalled(
      extension, initial_state, initial_enable_incognito);

  // Unpacked extensions start off with file access since they are a developer
  // feature.
  if (extension->location() == Extension::LOAD)
    extension_prefs_->SetAllowFileAccess(extension->id(), true);

  // If the extension is a theme, tell the profile (and therefore ThemeProvider)
  // to apply it.
  if (extension->is_theme()) {
    NotificationService::current()->Notify(
        NotificationType::THEME_INSTALLED,
        Source<Profile>(profile_),
        Details<const Extension>(extension));
  } else {
    NotificationService::current()->Notify(
        NotificationType::EXTENSION_INSTALLED,
        Source<Profile>(profile_),
        Details<const Extension>(extension));
  }

  if (extension->is_app()) {
    ExtensionIdSet installed_ids = GetAppIds();
    installed_ids.insert(extension->id());
    default_apps_.DidInstallApp(installed_ids);
  }

  // Transfer ownership of |extension| to OnExtensionLoaded.
  OnExtensionLoaded(scoped_extension, allow_privilege_increase);
}

const Extension* ExtensionsService::GetExtensionByIdInternal(
    const std::string& id, bool include_enabled, bool include_disabled) {
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
  return NULL;
}

const Extension* ExtensionsService::GetWebStoreApp() {
  return GetExtensionById(extension_misc::kWebStoreAppId, false);
}

const Extension* ExtensionsService::GetExtensionByURL(const GURL& url) {
  return url.scheme() != chrome::kExtensionScheme ? NULL :
      GetExtensionById(url.host(), false);
}

const Extension* ExtensionsService::GetExtensionByWebExtent(const GURL& url) {
  for (size_t i = 0; i < extensions_.size(); ++i) {
    if (extensions_[i]->web_extent().ContainsURL(url))
      return extensions_[i];
  }
  return NULL;
}

bool ExtensionsService::ExtensionBindingsAllowed(const GURL& url) {
  // Allow bindings for all packaged extension.
  if (GetExtensionByURL(url))
    return true;

  // Allow bindings for all component, hosted apps.
  const Extension* extension = GetExtensionByWebExtent(url);
  return (extension && extension->location() == Extension::COMPONENT);
}

const Extension* ExtensionsService::GetExtensionByOverlappingWebExtent(
    const ExtensionExtent& extent) {
  for (size_t i = 0; i < extensions_.size(); ++i) {
    if (extensions_[i]->web_extent().OverlapsWith(extent))
      return extensions_[i];
  }

  return NULL;
}

const SkBitmap& ExtensionsService::GetOmniboxIcon(
    const std::string& extension_id) {
  return omnibox_icon_manager_.GetIcon(extension_id);
}

const SkBitmap& ExtensionsService::GetOmniboxPopupIcon(
    const std::string& extension_id) {
  return omnibox_popup_icon_manager_.GetIcon(extension_id);
}

void ExtensionsService::ClearProvidersForTesting() {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          backend_.get(), &ExtensionsServiceBackend::ClearProvidersForTesting));
}

void ExtensionsService::AddProviderForTesting(
    ExternalExtensionProvider* test_provider) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          backend_.get(), &ExtensionsServiceBackend::AddProviderForTesting,
          test_provider));
}

void ExtensionsService::OnExternalExtensionFileFound(
         const std::string& id,
         const std::string& version,
         const FilePath& path,
         Extension::Location location) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Before even bothering to unpack, check and see if we already have this
  // version. This is important because these extensions are going to get
  // installed on every startup.
  const Extension* existing = GetExtensionById(id, true);
  scoped_ptr<Version> other(Version::GetVersionFromString(version));
  if (existing) {
    switch (existing->version()->CompareTo(*other)) {
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

  scoped_refptr<CrxInstaller> installer(
      new CrxInstaller(install_directory_,
                       this,  // frontend
                       NULL));  // no client (silent install)
  installer->set_install_source(location);
  installer->set_expected_id(id);
  installer->set_allow_privilege_increase(true);
  installer->InstallCrx(path);
}

void ExtensionsService::ReportExtensionLoadError(
    const FilePath& extension_path,
    const std::string &error,
    NotificationType type,
    bool be_noisy) {
  NotificationService* service = NotificationService::current();
  service->Notify(type,
                  Source<Profile>(profile_),
                  Details<const std::string>(&error));

  // TODO(port): note that this isn't guaranteed to work properly on Linux.
  std::string path_str = WideToUTF8(extension_path.ToWStringHack());
  std::string message = base::StringPrintf(
      "Could not load extension from '%s'. %s",
      path_str.c_str(), error.c_str());
  ExtensionErrorReporter::GetInstance()->ReportError(message, be_noisy);
}

void ExtensionsService::DidCreateRenderViewForBackgroundPage(
    ExtensionHost* host) {
  OrphanedDevTools::iterator iter =
      orphaned_dev_tools_.find(host->extension()->id());
  if (iter == orphaned_dev_tools_.end())
    return;

  DevToolsManager::GetInstance()->AttachClientHost(
      iter->second, host->render_view_host());
  orphaned_dev_tools_.erase(iter);
}

void ExtensionsService::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_PROCESS_TERMINATED: {
      if (profile_ != Source<Profile>(source).ptr()->GetOriginalProfile())
        break;

      ExtensionHost* host = Details<ExtensionHost>(details).ptr();

      // Unload the entire extension. We want it to be in a consistent state:
      // either fully working or not loaded at all, but never half-crashed.
      // We do it in a PostTask so that other handlers of this notification will
      // still have access to the Extension and ExtensionHost.
      MessageLoop::current()->PostTask(FROM_HERE,
          NewRunnableMethod(this, &ExtensionsService::UnloadExtension,
                            host->extension()->id()));
      break;
    }

    case NotificationType::PREF_CHANGED: {
      std::string* pref_name = Details<std::string>(details).ptr();
      DCHECK(*pref_name == prefs::kExtensionInstallAllowList ||
             *pref_name == prefs::kExtensionInstallDenyList);
      CheckAdminBlacklist();
      break;
    }

    default:
      NOTREACHED() << "Unexpected notification type.";
  }
}

bool ExtensionsService::HasApps() const {
  return !GetAppIds().empty();
}

ExtensionIdSet ExtensionsService::GetAppIds() const {
  ExtensionIdSet result;
  for (ExtensionList::const_iterator it = extensions_.begin();
       it != extensions_.end(); ++it) {
    if ((*it)->is_app() && (*it)->location() != Extension::COMPONENT)
      result.insert((*it)->id());
  }

  return result;
}

bool ExtensionsService::IsBackgroundPageReady(const Extension* extension) {
  return (extension->background_url().is_empty() ||
          extension_runtime_data_[extension->id()].background_page_ready);
}

void ExtensionsService::SetBackgroundPageReady(const Extension* extension) {
  DCHECK(!extension->background_url().is_empty());
  extension_runtime_data_[extension->id()].background_page_ready = true;
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_BACKGROUND_PAGE_READY,
      Source<const Extension>(extension),
      NotificationService::NoDetails());
}

bool ExtensionsService::IsBeingUpgraded(const Extension* extension) {
  return extension_runtime_data_[extension->id()].being_upgraded;
}

void ExtensionsService::SetBeingUpgraded(const Extension* extension,
                                         bool value) {
  extension_runtime_data_[extension->id()].being_upgraded = value;
}
