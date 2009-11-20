// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extensions_service.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/histogram.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_dom_ui.h"
#include "chrome/browser/extensions/extension_file_util.h"
#include "chrome/browser/extensions/extension_history_api.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_updater.h"
#include "chrome/browser/extensions/external_extension_provider.h"
#include "chrome/browser/extensions/external_pref_extension_provider.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"

#if defined(OS_WIN)
#include "chrome/browser/extensions/external_registry_extension_provider_win.h"
#endif

namespace {

// Helper class to collect the IDs of every extension listed in the prefs.
class InstalledExtensionSet {
 public:
  explicit InstalledExtensionSet(InstalledExtensions* installed) {
    scoped_ptr<InstalledExtensions> cleanup(installed);
    installed->VisitInstalledExtensions(
        NewCallback(this, &InstalledExtensionSet::ExtensionVisited));
  }

  const std::set<std::string>& extensions() { return extensions_; }

 private:
  void ExtensionVisited(
      DictionaryValue* manifest, const std::string& id,
      const FilePath& path, Extension::Location location) {
    extensions_.insert(id);
  }

  std::set<std::string> extensions_;
};

} // namespace

// ExtensionsService.

const char* ExtensionsService::kInstallDirectoryName = "Extensions";
const char* ExtensionsService::kCurrentVersionFileName = "Current Version";

// static
bool ExtensionsService::IsDownloadFromGallery(const GURL& download_url,
                                              const GURL& referrer_url) {
  if (StartsWithASCII(download_url.spec(),
                      extension_urls::kMiniGalleryDownloadPrefix, false) &&
      StartsWithASCII(referrer_url.spec(),
                      extension_urls::kMiniGalleryBrowsePrefix, false)) {
    return true;
  }

  if (StartsWithASCII(download_url.spec(),
                      extension_urls::kGalleryDownloadPrefix, false) &&
      StartsWithASCII(referrer_url.spec(),
                      extension_urls::kGalleryBrowsePrefix, false)) {
    return true;
  }

  return false;
}

ExtensionsService::ExtensionsService(Profile* profile,
                                     const CommandLine* command_line,
                                     PrefService* prefs,
                                     const FilePath& install_directory,
                                     bool autoupdate_enabled)
    : profile_(profile),
      extension_prefs_(new ExtensionPrefs(prefs, install_directory)),
      install_directory_(install_directory),
      extensions_enabled_(true),
      show_extensions_prompts_(true),
      ready_(false) {
  // Figure out if extension installation should be enabled.
  if (command_line->HasSwitch(switches::kDisableExtensions)) {
    extensions_enabled_ = false;
  } else if (profile->GetPrefs()->GetBoolean(prefs::kDisableExtensions)) {
    extensions_enabled_ = false;
  }

  registrar_.Add(this, NotificationType::EXTENSION_HOST_DID_STOP_LOADING,
                 NotificationService::AllSources());

  // Set up the ExtensionUpdater
  if (autoupdate_enabled) {
    int update_frequency = kDefaultUpdateFrequencySeconds;
    if (command_line->HasSwitch(switches::kExtensionsUpdateFrequency)) {
      update_frequency = StringToInt(command_line->GetSwitchValueASCII(
          switches::kExtensionsUpdateFrequency));
    }
    updater_ = new ExtensionUpdater(this, prefs, update_frequency);
  }

  backend_ = new ExtensionsServiceBackend(install_directory_);
}

ExtensionsService::~ExtensionsService() {
  UnloadAllExtensions();
  if (updater_.get()) {
    updater_->Stop();
  }
}

void ExtensionsService::Init() {
  DCHECK(!ready_);
  DCHECK_EQ(extensions_.size(), 0u);

  // Hack: we need to ensure the ResourceDispatcherHost is ready before we load
  // the first extension, because its members listen for loaded notifications.
  g_browser_process->resource_dispatcher_host();

  // Start up the extension event routers.
  ExtensionHistoryEventRouter::GetInstance()->ObserveProfile(profile_);

  LoadAllExtensions();

  // TODO(erikkay) this should probably be deferred to a future point
  // rather than running immediately at startup.
  CheckForExternalUpdates();

  // TODO(erikkay) this should probably be deferred as well.
  GarbageCollectExtensions();
}

void ExtensionsService::InstallExtension(const FilePath& extension_path) {
  CrxInstaller::Start(extension_path, install_directory_, Extension::INTERNAL,
                      "",   // no expected id
                      false,  // don't delete crx when complete
                      true,  // allow privilege increase
                      this,
                      NULL);  // no client (silent install)
}

void ExtensionsService::UpdateExtension(const std::string& id,
                                        const FilePath& extension_path) {
  if (!GetExtensionByIdInternal(id, true, true)) {
    LOG(WARNING) << "Will not update extension " << id << " because it is not "
                 << "installed";
    return;
  }

  CrxInstaller::Start(extension_path, install_directory_, Extension::INTERNAL,
                      id,
                      true,  // delete crx when complete
                      false,  // do not allow upgrade of privileges
                      this,
                      NULL);  // no client (silent install)
}

void ExtensionsService::ReloadExtension(const std::string& extension_id) {
  FilePath path;
  Extension* current_extension = GetExtensionById(extension_id, false);

  // Unload the extension if it's loaded. It might not be loaded if it crashed.
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
    UnloadExtension(extension_id);
  }

  if (path.empty()) {
    // At this point we have to reconstruct the path from prefs, because
    // we have no information about this extension in memory.
    path = extension_prefs_->GetExtensionPath(extension_id);
  }

  if (!path.empty())
    LoadExtension(path);
}

void ExtensionsService::UninstallExtension(const std::string& extension_id,
                                           bool external_uninstall) {
  Extension* extension = GetExtensionByIdInternal(extension_id, true, true);

  // Callers should not send us nonexistant extensions.
  DCHECK(extension);

  extension_prefs_->OnExtensionUninstalled(extension, external_uninstall);

  // Tell the backend to start deleting installed extensions on the file thread.
  if (Extension::LOAD != extension->location()) {
    ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableFunction(
          &extension_file_util::UninstallExtension, extension_id,
          install_directory_));
  }

  ExtensionDOMUI::UnregisterChromeURLOverrides(profile_,
      extension->GetChromeURLOverrides());

  UnloadExtension(extension_id);
}

void ExtensionsService::EnableExtension(const std::string& extension_id) {
  Extension* extension = GetExtensionByIdInternal(extension_id, false, true);
  if (!extension) {
    NOTREACHED() << "Trying to enable an extension that isn't disabled.";
    return;
  }

  // Remember that we enabled it, unless it's temporary.
  if (extension->location() != Extension::LOAD)
    extension_prefs_->SetExtensionState(extension, Extension::ENABLED);

  // Move it over to the enabled list.
  extensions_.push_back(extension);
  ExtensionList::iterator iter = std::find(disabled_extensions_.begin(),
                                           disabled_extensions_.end(),
                                           extension);
  disabled_extensions_.erase(iter);

  ExtensionDOMUI::RegisterChromeURLOverrides(profile_,
      extension->GetChromeURLOverrides());

  NotifyExtensionLoaded(extension);
}

void ExtensionsService::DisableExtension(const std::string& extension_id) {
  Extension* extension = GetExtensionByIdInternal(extension_id, true, false);
  if (!extension) {
    NOTREACHED() << "Trying to disable an extension that isn't enabled.";
    return;
  }

  // Remember that we disabled it, unless it's temporary.
  if (extension->location() != Extension::LOAD)
    extension_prefs_->SetExtensionState(extension, Extension::DISABLED);

  // Move it over to the disabled list.
  disabled_extensions_.push_back(extension);
  ExtensionList::iterator iter = std::find(extensions_.begin(),
                                           extensions_.end(),
                                           extension);
  extensions_.erase(iter);

  ExtensionDOMUI::UnregisterChromeURLOverrides(profile_,
      extension->GetChromeURLOverrides());

  NotifyExtensionUnloaded(extension);
}

void ExtensionsService::LoadExtension(const FilePath& extension_path) {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          backend_.get(),
          &ExtensionsServiceBackend::LoadSingleExtension,
          extension_path, scoped_refptr<ExtensionsService>(this)));
}

void ExtensionsService::LoadAllExtensions() {
  base::TimeTicks start_time = base::TimeTicks::Now();

  // Load the previously installed extensions.
  scoped_ptr<InstalledExtensions> installed(
      new InstalledExtensions(extension_prefs_.get()));
  installed->VisitInstalledExtensions(
      NewCallback(this, &ExtensionsService::LoadInstalledExtension));
  OnLoadedInstalledExtensions();

  UMA_HISTOGRAM_COUNTS_100("Extensions.LoadAll", extensions_.size());
  UMA_HISTOGRAM_COUNTS_100("Extensions.Disabled", disabled_extensions_.size());

  if (extensions_.size()) {
    UMA_HISTOGRAM_TIMES("Extensions.LoadAllTime",
                        base::TimeTicks::Now() - start_time);

    int user_script_count = 0;
    int extension_count = 0;
    int theme_count = 0;
    int external_count = 0;
    int page_action_count = 0;
    int browser_action_count = 0;
    ExtensionList::iterator ex;
    for (ex = extensions_.begin(); ex != extensions_.end(); ++ex) {
      if ((*ex)->IsTheme()) {
        theme_count++;
      } else if ((*ex)->converted_from_user_script()) {
        user_script_count++;
      } else {
        extension_count++;
      }
      if (Extension::IsExternalLocation((*ex)->location())) {
        external_count++;
      }
      if ((*ex)->page_action() != NULL) {
        page_action_count++;
      }
      if ((*ex)->browser_action() != NULL) {
        browser_action_count++;
      }
    }
    UMA_HISTOGRAM_COUNTS_100("Extensions.LoadExtension", extension_count);
    UMA_HISTOGRAM_COUNTS_100("Extensions.LoadUserScript", user_script_count);
    UMA_HISTOGRAM_COUNTS_100("Extensions.LoadTheme", theme_count);
    UMA_HISTOGRAM_COUNTS_100("Extensions.LoadExternal", external_count);
    UMA_HISTOGRAM_COUNTS_100("Extensions.LoadPageAction", page_action_count);
    UMA_HISTOGRAM_COUNTS_100("Extensions.LoadBrowserAction",
                             browser_action_count);
  }
}

void ExtensionsService::LoadInstalledExtension(
    DictionaryValue* manifest, const std::string& id,
    const FilePath& path, Extension::Location location) {
  std::string error;
  Extension* extension = NULL;
  if (manifest) {
    scoped_ptr<Extension> tmp(new Extension(path));
    if (tmp->InitFromValue(*manifest, true, &error)) {
      extension = tmp.release();
    }
  } else {
    error = extension_manifest_errors::kManifestUnreadable;
  }

  if (!extension) {
    ReportExtensionLoadError(path,
                             error,
                             NotificationType::EXTENSION_INSTALL_ERROR,
                             false);
    return;
  }

  extension->set_location(location);
  OnExtensionLoaded(extension, true);

  if (location == Extension::EXTERNAL_PREF ||
      location == Extension::EXTERNAL_REGISTRY) {
    ChromeThread::PostTask(
        ChromeThread::FILE, FROM_HERE,
        NewRunnableMethod(
            backend_.get(),
            &ExtensionsServiceBackend::CheckExternalUninstall,
            scoped_refptr<ExtensionsService>(this), id, location));
  }
}

void ExtensionsService::NotifyExtensionLoaded(Extension* extension) {
  LOG(INFO) << "Sending EXTENSION_LOADED";

  // The ChromeURLRequestContext needs to be first to know that the extension
  // was loaded, otherwise a race can arise where a renderer that is created
  // for the extension may try to load an extension URL with an extension id
  // that the request context doesn't yet know about.
  if (profile_ && !profile_->IsOffTheRecord()) {
    ChromeURLRequestContextGetter* context_getter =
        static_cast<ChromeURLRequestContextGetter*>(
            profile_->GetRequestContext());
    if (context_getter) {
      ChromeThread::PostTask(
          ChromeThread::IO, FROM_HERE,
          NewRunnableMethod(
              context_getter,
              &ChromeURLRequestContextGetter::OnNewExtensions,
              extension->id(),
              extension->path()));
    }
  }

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_LOADED,
      Source<Profile>(profile_),
      Details<Extension>(extension));
}

void ExtensionsService::NotifyExtensionUnloaded(Extension* extension) {
  LOG(INFO) << "Sending EXTENSION_UNLOADED";

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_UNLOADED,
      Source<Profile>(profile_),
      Details<Extension>(extension));

  if (profile_ && !profile_->IsOffTheRecord()) {
    ChromeURLRequestContextGetter* context_getter =
        static_cast<ChromeURLRequestContextGetter*>(
            profile_->GetRequestContext());
    if (context_getter) {
      ChromeThread::PostTask(
          ChromeThread::IO, FROM_HERE,
          NewRunnableMethod(
              context_getter,
              &ChromeURLRequestContextGetter::OnUnloadedExtension,
              extension->id()));
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
    Extension* extension = (*iter);
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

void ExtensionsService::CheckForExternalUpdates() {
  // This installs or updates externally provided extensions.
  // TODO(aa): Why pass this list into the provider, why not just filter it
  // later?
  std::set<std::string> killed_extensions;
  extension_prefs_->GetKilledExtensionIds(&killed_extensions);
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          backend_.get(), &ExtensionsServiceBackend::CheckForExternalUpdates,
          killed_extensions, scoped_refptr<ExtensionsService>(this)));
}

void ExtensionsService::UnloadExtension(const std::string& extension_id) {
  scoped_ptr<Extension> extension(
      GetExtensionByIdInternal(extension_id, true, true));

  // Callers should not send us nonexistant extensions.
  CHECK(extension.get());

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
        Details<Extension>(extension.get()));
    return;
  }

  iter = std::find(extensions_.begin(), extensions_.end(), extension.get());

  // Remove the extension from our list.
  extensions_.erase(iter);

  NotifyExtensionUnloaded(extension.get());
}

void ExtensionsService::UnloadAllExtensions() {
  ExtensionList::iterator iter;
  for (iter = extensions_.begin(); iter != extensions_.end(); ++iter)
    delete *iter;
  extensions_.clear();

  // TODO(erikkay) should there be a notification for this?  We can't use
  // EXTENSION_UNLOADED since that implies that the extension has been disabled
  // or uninstalled, and UnloadAll is just part of shutdown.
}

void ExtensionsService::ReloadExtensions() {
  UnloadAllExtensions();
  LoadAllExtensions();
}

void ExtensionsService::GarbageCollectExtensions() {
  InstalledExtensionSet installed(
      new InstalledExtensions(extension_prefs_.get()));
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableFunction(
          &extension_file_util::GarbageCollectExtensions, install_directory_,
          installed.extensions()));
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

void ExtensionsService::OnExtensionLoaded(Extension* extension,
                                          bool allow_privilege_increase) {
  // Ensure extension is deleted unless we transfer ownership.
  scoped_ptr<Extension> scoped_extension(extension);

  if (extensions_enabled() ||
      extension->IsTheme() ||
      extension->location() == Extension::LOAD ||
      Extension::IsExternalLocation(extension->location())) {
    Extension* old = GetExtensionByIdInternal(extension->id(), true, true);
    if (old) {
      if (extension->version()->CompareTo(*(old->version())) > 0) {
        bool allow_silent_upgrade =
            allow_privilege_increase || !Extension::IsPrivilegeIncrease(
                old, extension);

        // To upgrade an extension in place, unload the old one and
        // then load the new one.
        UnloadExtension(old->id());
        old = NULL;

        if (!allow_silent_upgrade) {
          // Extension has changed permissions significantly. Disable it and
          // notify the user.
          extension_prefs_->SetExtensionState(extension, Extension::DISABLED);
          NotificationService::current()->Notify(
              NotificationType::EXTENSION_UPDATE_DISABLED,
              Source<Profile>(profile_),
              Details<Extension>(extension));
        }
      } else {
        // We already have the extension of the same or older version.
        std::string error_message("Duplicate extension load attempt: ");
        error_message += extension->id();
        LOG(WARNING) << error_message;
        ReportExtensionLoadError(extension->path(),
                                 error_message,
                                 NotificationType::EXTENSION_OVERINSTALL_ERROR,
                                 false);
        return;
      }
    }

    switch (extension_prefs_->GetExtensionState(extension->id())) {
      case Extension::ENABLED:
        extensions_.push_back(scoped_extension.release());

        // We delay starting up the browser event router until at least one
        // extension that needs it is loaded.
        if (extension->HasApiPermission(Extension::kTabPermission)) {
          ExtensionBrowserEventRouter::GetInstance()->Init();
        }

        if (extension->location() != Extension::LOAD)
          extension_prefs_->MigrateToPrefs(extension);

        NotifyExtensionLoaded(extension);

        if (extension->IsTheme() && extension->location() == Extension::LOAD) {
          NotificationService::current()->Notify(
              NotificationType::THEME_INSTALLED,
              Source<Profile>(profile_),
              Details<Extension>(extension));
        } else {
          ExtensionDOMUI::RegisterChromeURLOverrides(profile_,
              extension->GetChromeURLOverrides());
        }
        break;
      case Extension::DISABLED:
        NotificationService::current()->Notify(
            NotificationType::EXTENSION_UPDATE_DISABLED,
            Source<Profile>(profile_),
            Details<Extension>(extension));
        disabled_extensions_.push_back(scoped_extension.release());
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

void ExtensionsService::OnExtensionInstalled(Extension* extension,
                                             bool allow_privilege_increase) {
  extension_prefs_->OnExtensionInstalled(extension);

  // If the extension is a theme, tell the profile (and therefore ThemeProvider)
  // to apply it.
  if (extension->IsTheme()) {
    NotificationService::current()->Notify(
        NotificationType::THEME_INSTALLED,
        Source<Profile>(profile_),
        Details<Extension>(extension));
  } else {
    NotificationService::current()->Notify(
        NotificationType::EXTENSION_INSTALLED,
        Source<Profile>(profile_),
        Details<Extension>(extension));
  }

  // Also load the extension.
  OnExtensionLoaded(extension, allow_privilege_increase);
}

void ExtensionsService::OnExtensionOverinstallAttempted(const std::string& id) {
  Extension* extension = GetExtensionById(id, false);
  if (extension && extension->IsTheme()) {
    NotificationService::current()->Notify(
        NotificationType::THEME_INSTALLED,
        Source<Profile>(profile_),
        Details<Extension>(extension));
  } else {
    NotificationService::current()->Notify(
      NotificationType::NO_THEME_DETECTED,
      Source<Profile>(profile_),
      NotificationService::NoDetails());
  }
}

Extension* ExtensionsService::GetExtensionByIdInternal(const std::string& id,
                                                       bool include_enabled,
                                                       bool include_disabled) {
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

Extension* ExtensionsService::GetExtensionByURL(const GURL& url) {
  std::string host = url.host();
  return GetExtensionById(host, false);
}

void ExtensionsService::ClearProvidersForTesting() {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          backend_.get(), &ExtensionsServiceBackend::ClearProvidersForTesting));
}

void ExtensionsService::SetProviderForTesting(
    Extension::Location location, ExternalExtensionProvider* test_provider) {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          backend_.get(), &ExtensionsServiceBackend::SetProviderForTesting,
          location, test_provider));
}

void ExtensionsService::OnExternalExtensionFound(const std::string& id,
                                                 const std::string& version,
                                                 const FilePath& path,
                                                 Extension::Location location) {
  // Before even bothering to unpack, check and see if we already have this
  // version. This is important because these extensions are going to get
  // installed on every startup.
  Extension* existing = GetExtensionById(id, true);
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

  CrxInstaller::Start(path, install_directory_, location, id,
                      false,  // don't delete crx when complete
                      true,  // allow privilege increase
                      this,
                      NULL);  // no client (silent install)
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
  std::string path_str = WideToASCII(extension_path.ToWStringHack());
  std::string message = StringPrintf("Could not load extension from '%s'. %s",
                                     path_str.c_str(), error.c_str());
  ExtensionErrorReporter::GetInstance()->ReportError(message, be_noisy);
}

std::vector<FilePath> ExtensionsService::GetPersistentBlacklistPaths() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  std::vector<FilePath> result;
  for (ExtensionList::const_iterator extension_iter = extensions()->begin();
       extension_iter != extensions()->end(); ++extension_iter) {
    if ((*extension_iter)->location() == Extension::LOAD)
      continue;

    std::vector<Extension::PrivacyBlacklistInfo> blacklists(
        (*extension_iter)->privacy_blacklists());
    std::vector<Extension::PrivacyBlacklistInfo>::const_iterator blacklist_iter;
    for (blacklist_iter = blacklists.begin();
         blacklist_iter != blacklists.end(); ++blacklist_iter) {
      result.push_back(blacklist_iter->path);
    }
  }
  return result;
}

std::vector<FilePath> ExtensionsService::GetTransientBlacklistPaths() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  std::vector<FilePath> result;
  for (ExtensionList::const_iterator extension_iter = extensions()->begin();
       extension_iter != extensions()->end(); ++extension_iter) {
    if ((*extension_iter)->location() != Extension::LOAD)
      continue;

    std::vector<Extension::PrivacyBlacklistInfo> blacklists(
        (*extension_iter)->privacy_blacklists());
    std::vector<Extension::PrivacyBlacklistInfo>::const_iterator blacklist_iter;
    for (blacklist_iter = blacklists.begin();
         blacklist_iter != blacklists.end(); ++blacklist_iter) {
      result.push_back(blacklist_iter->path);
    }
  }
  return result;
}

void ExtensionsService::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_HOST_DID_STOP_LOADING: {
      ExtensionHost* host = Details<ExtensionHost>(details).ptr();
      OrphanedDevTools::iterator iter =
          orphaned_dev_tools_.find(host->extension()->id());
      if (iter == orphaned_dev_tools_.end())
        return;

      DevToolsManager::GetInstance()->AttachClientHost(
          iter->second, host->render_view_host());
      orphaned_dev_tools_.erase(iter);
      break;
    }

    default:
      NOTREACHED() << "Unexpected notification type.";
  }
}


// ExtensionsServicesBackend

ExtensionsServiceBackend::ExtensionsServiceBackend(
    const FilePath& install_directory)
        : frontend_(NULL),
          install_directory_(install_directory),
          alert_on_error_(false) {
  // TODO(aa): This ends up doing blocking IO on the UI thread because it reads
  // pref data in the ctor and that is called on the UI thread. Would be better
  // to re-read data each time we list external extensions, anyway.
  external_extension_providers_[Extension::EXTERNAL_PREF] =
      linked_ptr<ExternalExtensionProvider>(
          new ExternalPrefExtensionProvider());
#if defined(OS_WIN)
  external_extension_providers_[Extension::EXTERNAL_REGISTRY] =
      linked_ptr<ExternalExtensionProvider>(
          new ExternalRegistryExtensionProvider());
#endif
}

ExtensionsServiceBackend::~ExtensionsServiceBackend() {
}

void ExtensionsServiceBackend::LoadSingleExtension(
    const FilePath& path_in, scoped_refptr<ExtensionsService> frontend) {
  frontend_ = frontend;

  // Explicit UI loads are always noisy.
  alert_on_error_ = true;

  FilePath extension_path = path_in;
  file_util::AbsolutePath(&extension_path);

  LOG(INFO) << "Loading single extension from " <<
      WideToASCII(extension_path.BaseName().ToWStringHack());

  std::string error;
  Extension* extension = extension_file_util::LoadExtension(
      extension_path,
      false,  // Don't require id
      &error);

  if (!extension) {
    ReportExtensionLoadError(extension_path, error);
    return;
  }

  extension->set_location(Extension::LOAD);
  ReportExtensionLoaded(extension);
}

void ExtensionsServiceBackend::ReportExtensionLoadError(
    const FilePath& extension_path, const std::string &error) {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(
          frontend_,
          &ExtensionsService::ReportExtensionLoadError, extension_path,
          error, NotificationType::EXTENSION_INSTALL_ERROR, alert_on_error_));
}

void ExtensionsServiceBackend::ReportExtensionLoaded(Extension* extension) {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(
          frontend_, &ExtensionsService::OnExtensionLoaded, extension, true));
}

bool ExtensionsServiceBackend::LookupExternalExtension(
    const std::string& id, Version** version, Extension::Location* location) {
  scoped_ptr<Version> extension_version;
  for (ProviderMap::const_iterator i = external_extension_providers_.begin();
       i != external_extension_providers_.end(); ++i) {
    const ExternalExtensionProvider* provider = i->second.get();
    extension_version.reset(provider->RegisteredVersion(id, location));
    if (extension_version.get()) {
      if (version)
        *version = extension_version.release();
      return true;
    }
  }
  return false;
}

// Some extensions will autoupdate themselves externally from Chrome.  These
// are typically part of some larger client application package.  To support
// these, the extension will register its location in the the preferences file
// (and also, on Windows, in the registry) and this code will periodically
// check that location for a .crx file, which it will then install locally if
// a new version is available.
void ExtensionsServiceBackend::CheckForExternalUpdates(
    std::set<std::string> ids_to_ignore,
    scoped_refptr<ExtensionsService> frontend) {
  // Note that this installation is intentionally silent (since it didn't
  // go through the front-end).  Extensions that are registered in this
  // way are effectively considered 'pre-bundled', and so implicitly
  // trusted.  In general, if something has HKLM or filesystem access,
  // they could install an extension manually themselves anyway.
  alert_on_error_ = false;
  frontend_ = frontend;

  // Ask each external extension provider to give us a call back for each
  // extension they know about. See OnExternalExtensionFound.
  for (ProviderMap::const_iterator i = external_extension_providers_.begin();
       i != external_extension_providers_.end(); ++i) {
    ExternalExtensionProvider* provider = i->second.get();
    provider->VisitRegisteredExtension(this, ids_to_ignore);
  }
}

void ExtensionsServiceBackend::CheckExternalUninstall(
    scoped_refptr<ExtensionsService> frontend, const std::string& id,
    Extension::Location location) {
  // Check if the providers know about this extension.
  ProviderMap::const_iterator i = external_extension_providers_.find(location);
  if (i == external_extension_providers_.end()) {
    NOTREACHED() << "CheckExternalUninstall called for non-external extension "
                 << location;
    return;
  }

  scoped_ptr<Version> version;
  version.reset(i->second->RegisteredVersion(id, NULL));
  if (version.get())
    return;  // Yup, known extension, don't uninstall.

  // This is an external extension that we don't have registered.  Uninstall.
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(
          frontend.get(), &ExtensionsService::UninstallExtension, id, true));
}

void ExtensionsServiceBackend::ClearProvidersForTesting() {
  external_extension_providers_.clear();
}

void ExtensionsServiceBackend::SetProviderForTesting(
    Extension::Location location,
    ExternalExtensionProvider* test_provider) {
  DCHECK(test_provider);
  external_extension_providers_[location] =
      linked_ptr<ExternalExtensionProvider>(test_provider);
}

void ExtensionsServiceBackend::OnExternalExtensionFound(
    const std::string& id, const Version* version, const FilePath& path,
    Extension::Location location) {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(
          frontend_, &ExtensionsService::OnExternalExtensionFound, id,
          version->GetString(), path, location));
}
