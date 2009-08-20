// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extensions_service.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_file_util.h"
#include "chrome/browser/extensions/extension_updater.h"
#include "chrome/browser/extensions/external_extension_provider.h"
#include "chrome/browser/extensions/external_pref_extension_provider.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/common/notification_service.h"
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
  InstalledExtensionSet(InstalledExtensions* installed) {
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

const char* ExtensionsService::kGalleryDownloadURLPrefix =
    "https://dl-ssl.google.com/chrome/";
const char* ExtensionsService::kGalleryURLPrefix =
    "https://tools.google.com/chrome/";

// static
bool ExtensionsService::IsDownloadFromGallery(const GURL& download_url,
                                              const GURL& referrer_url) {
  if (StartsWithASCII(download_url.spec(), kGalleryDownloadURLPrefix, false) &&
      StartsWithASCII(referrer_url.spec(), kGalleryURLPrefix, false)) {
    return true;
  } else {
    return false;
  }
}

ExtensionsService::ExtensionsService(Profile* profile,
                                     const CommandLine* command_line,
                                     PrefService* prefs,
                                     const FilePath& install_directory,
                                     MessageLoop* frontend_loop,
                                     MessageLoop* backend_loop,
                                     bool autoupdate_enabled)
    : profile_(profile),
      extension_prefs_(new ExtensionPrefs(prefs, install_directory)),
      backend_loop_(backend_loop),
      install_directory_(install_directory),
      extensions_enabled_(false),
      show_extensions_prompts_(true),
      ready_(false) {
  // Figure out if extension installation should be enabled.
  if (command_line->HasSwitch(switches::kEnableExtensions)) {
    extensions_enabled_ = true;
  } else if (profile->GetPrefs()->GetBoolean(prefs::kEnableExtensions)) {
    extensions_enabled_ = true;
  }

  // Set up the ExtensionUpdater
  if (autoupdate_enabled) {
    int update_frequency = kDefaultUpdateFrequencySeconds;
    if (command_line->HasSwitch(switches::kExtensionsUpdateFrequency)) {
      update_frequency = StringToInt(WideToASCII(command_line->GetSwitchValue(
          switches::kExtensionsUpdateFrequency)));
    }
    updater_ = new ExtensionUpdater(this, prefs, update_frequency,
        backend_loop_, ChromeThread::GetMessageLoop(ChromeThread::IO));
  }

  backend_ = new ExtensionsServiceBackend(install_directory_, frontend_loop);
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

  // Start up the extension event routers.
  ExtensionBrowserEventRouter::GetInstance()->Init();

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
                      backend_loop_,
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
                      backend_loop_,
                      this,
                      NULL);  // no client (silent install)
}

void ExtensionsService::ReloadExtension(const std::string& extension_id) {
  Extension* extension = GetExtensionById(extension_id);
  FilePath extension_path = extension->path();

  UnloadExtension(extension_id);
  LoadExtension(extension_path);
}

void ExtensionsService::UninstallExtension(const std::string& extension_id,
                                           bool external_uninstall) {
  Extension* extension = GetExtensionByIdInternal(extension_id, true, true);

  // Callers should not send us nonexistant extensions.
  DCHECK(extension);

  extension_prefs_->OnExtensionUninstalled(extension, external_uninstall);

  // Tell the backend to start deleting installed extensions on the file thread.
  if (Extension::LOAD != extension->location()) {
    backend_loop_->PostTask(FROM_HERE, NewRunnableFunction(
      &extension_file_util::UninstallExtension, extension_id,
      install_directory_));
  }

  UnloadExtension(extension_id);
}

void ExtensionsService::EnableExtension(const std::string& extension_id) {
  Extension* extension = GetExtensionByIdInternal(extension_id, false, true);
  if (!extension) {
    NOTREACHED() << "Trying to enable an extension that isn't disabled.";
    return;
  }

  // Move it over to the enabled list.
  extension_prefs_->SetExtensionState(extension, Extension::ENABLED);
  extensions_.push_back(extension);
  ExtensionList::iterator iter = std::find(disabled_extensions_.begin(),
                                           disabled_extensions_.end(),
                                           extension);
  disabled_extensions_.erase(iter);

  ExtensionList extensions;
  extensions.push_back(extension);
  NotificationService::current()->Notify(
      NotificationType::EXTENSIONS_LOADED,
      Source<ExtensionsService>(this),
      Details<ExtensionList>(&extensions));
}

void ExtensionsService::LoadExtension(const FilePath& extension_path) {
  backend_loop_->PostTask(FROM_HERE, NewRunnableMethod(backend_.get(),
      &ExtensionsServiceBackend::LoadSingleExtension,
      extension_path, scoped_refptr<ExtensionsService>(this)));
}

void ExtensionsService::LoadAllExtensions() {
  // Load the previously installed extensions.
  backend_loop_->PostTask(FROM_HERE, NewRunnableMethod(backend_.get(),
      &ExtensionsServiceBackend::LoadInstalledExtensions,
      scoped_refptr<ExtensionsService>(this),
      new InstalledExtensions(extension_prefs_.get())));
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
  backend_loop_->PostTask(FROM_HERE, NewRunnableMethod(backend_.get(),
      &ExtensionsServiceBackend::CheckForExternalUpdates,
      killed_extensions,
      scoped_refptr<ExtensionsService>(this)));
}

void ExtensionsService::UnloadExtension(const std::string& extension_id) {
  scoped_ptr<Extension> extension(
      GetExtensionByIdInternal(extension_id, true, true));

  // Callers should not send us nonexistant extensions.
  CHECK(extension.get());

  ExtensionList::iterator iter = std::find(disabled_extensions_.begin(),
                                           disabled_extensions_.end(),
                                           extension.get());
  if (iter != disabled_extensions_.end()) {
    disabled_extensions_.erase(iter);
    NotificationService::current()->Notify(
        NotificationType::EXTENSION_UNLOADED_DISABLED,
        Source<ExtensionsService>(this),
        Details<Extension>(extension.get()));
    return;
  }

  iter = std::find(extensions_.begin(), extensions_.end(), extension.get());

  // Remove the extension from our list.
  extensions_.erase(iter);

  // Tell other services the extension is gone.
  NotificationService::current()->Notify(NotificationType::EXTENSION_UNLOADED,
                                         Source<ExtensionsService>(this),
                                         Details<Extension>(extension.get()));
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
  backend_loop_->PostTask(FROM_HERE, NewRunnableFunction(
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
      Source<ExtensionsService>(this),
      NotificationService::NoDetails());
}

void ExtensionsService::OnExtensionsLoaded(ExtensionList* new_extensions) {
  scoped_ptr<ExtensionList> cleanup(new_extensions);

  // Filter out any extensions that shouldn't be loaded. If extensions are
  // enabled, load everything. Otherwise load only:
  // - themes
  // - --load-extension
  // - externally installed extensions
  ExtensionList enabled_extensions;
  ExtensionList disabled_extensions;
  for (ExtensionList::iterator iter = new_extensions->begin();
       iter != new_extensions->end(); ++iter) {
    // Extensions that get enabled get added to extensions_ and deleted later.
    // Anything skipped must be deleted now so we don't leak.
    scoped_ptr<Extension> extension(*iter);
    if (extensions_enabled() ||
        extension->IsTheme() ||
        extension->location() == Extension::LOAD ||
        Extension::IsExternalLocation(extension->location())) {
      Extension* old = GetExtensionByIdInternal(extension->id(), true, true);
      if (old) {
        if (extension->version()->CompareTo(*(old->version())) > 0) {
          bool higher_permissions =
              (extension->GetPermissionClass() > old->GetPermissionClass());

          // To upgrade an extension in place, unload the old one and
          // then load the new one.
          UnloadExtension(old->id());
          old = NULL;

          if (higher_permissions) {
            // Extension was upgraded to a high permission class. Disable it and
            // notify the user.
            extension_prefs_->SetExtensionState(extension.get(),
                                                Extension::DISABLED);
            NotificationService::current()->Notify(
                NotificationType::EXTENSION_UPDATE_DISABLED,
                Source<ExtensionsService>(this),
                Details<Extension>(extension.get()));
          }
        } else {
          // We already have the extension of the same or older version.
          LOG(WARNING) << "Duplicate extension load attempt: " << (*iter)->id();
          continue;
        }
      }

      switch (extension_prefs_->GetExtensionState(extension->id())) {
        case Extension::ENABLED:
          if (extension->location() != Extension::LOAD)
            extension_prefs_->MigrateToPrefs(extension.get());
          enabled_extensions.push_back(extension.get());
          extensions_.push_back(extension.release());
          break;
        case Extension::DISABLED:
          disabled_extensions.push_back(extension.get());
          disabled_extensions_.push_back(extension.release());
          break;
        default:
          break;
      }
    }
  }

  if (enabled_extensions.size()) {
    NotificationService::current()->Notify(
        NotificationType::EXTENSIONS_LOADED,
        Source<ExtensionsService>(this),
        Details<ExtensionList>(&enabled_extensions));
    for (ExtensionList::iterator iter = enabled_extensions.begin();
         iter != enabled_extensions.end(); ++iter) {
      if ((*iter)->IsTheme() && (*iter)->location() == Extension::LOAD) {
        NotificationService::current()->Notify(
            NotificationType::THEME_INSTALLED,
            Source<ExtensionsService>(this),
            Details<Extension>(*iter));
      }
    }
  }
}

void ExtensionsService::OnExtensionInstalled(Extension* extension) {
  extension_prefs_->OnExtensionInstalled(extension);

  // If the extension is a theme, tell the profile (and therefore ThemeProvider)
  // to apply it.
  if (extension->IsTheme()) {
    NotificationService::current()->Notify(
        NotificationType::THEME_INSTALLED,
        Source<ExtensionsService>(this),
        Details<Extension>(extension));
  } else {
    NotificationService::current()->Notify(
        NotificationType::EXTENSION_INSTALLED,
        Source<ExtensionsService>(this),
        Details<Extension>(extension));
  }

  // Also load the extension.
  ExtensionList* list = new ExtensionList;
  list->push_back(extension);
  OnExtensionsLoaded(list);
}

void ExtensionsService::OnExtensionOverinstallAttempted(const std::string& id) {
  Extension* extension = GetExtensionById(id);
  if (extension && extension->IsTheme()) {
    NotificationService::current()->Notify(
        NotificationType::THEME_INSTALLED,
        Source<ExtensionsService>(this),
        Details<Extension>(extension));
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
  return GetExtensionById(host);
}

void ExtensionsService::ClearProvidersForTesting() {
  backend_loop_->PostTask(FROM_HERE, NewRunnableMethod(backend_.get(),
      &ExtensionsServiceBackend::ClearProvidersForTesting));
}

void ExtensionsService::SetProviderForTesting(
    Extension::Location location, ExternalExtensionProvider* test_provider) {
  backend_loop_->PostTask(FROM_HERE, NewRunnableMethod(backend_.get(),
      &ExtensionsServiceBackend::SetProviderForTesting,
      location, test_provider));
}

void ExtensionsService::OnExternalExtensionFound(const std::string& id,
                                                 const std::string& version,
                                                 const FilePath& path,
                                                 Extension::Location location) {
  // Before even bothering to unpack, check and see if we already have this
  // version. This is important because these extensions are going to get
  // installed on every startup.
  Extension* existing = GetExtensionById(id);
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
                      backend_loop_,
                      this,
                      NULL);  // no client (silent install)
}


// ExtensionsServicesBackend

ExtensionsServiceBackend::ExtensionsServiceBackend(
    const FilePath& install_directory, MessageLoop* frontend_loop)
        : frontend_(NULL),
          install_directory_(install_directory),
          alert_on_error_(false),
          frontend_loop_(frontend_loop) {
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

void ExtensionsServiceBackend::LoadInstalledExtensions(
    scoped_refptr<ExtensionsService> frontend,
    InstalledExtensions* installed) {
  scoped_ptr<InstalledExtensions> cleanup(installed);
  frontend_ = frontend;
  alert_on_error_ = false;

  // Call LoadInstalledExtension for each extension |installed| knows about.
  installed->VisitInstalledExtensions(
      NewCallback(this, &ExtensionsServiceBackend::LoadInstalledExtension));

  frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      frontend_, &ExtensionsService::OnLoadedInstalledExtensions));
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
  ExtensionList* extensions = new ExtensionList;
  extensions->push_back(extension);
  ReportExtensionsLoaded(extensions);
}

void ExtensionsServiceBackend::LoadInstalledExtension(
    DictionaryValue* manifest, const std::string& id,
    const FilePath& path, Extension::Location location) {
  if (CheckExternalUninstall(id, location)) {
    frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        frontend_,
        &ExtensionsService::UninstallExtension,
        id, true));

    // No error needs to be reported. The extension effectively doesn't exist.
    return;
  }

  std::string error;
  Extension* extension = NULL;
  if (manifest) {
    scoped_ptr<Extension> tmp(new Extension(path));
    if (tmp->InitFromValue(*manifest, true, &error) &&
        extension_file_util::ValidateExtension(tmp.get(), &error)) {
      extension = tmp.release();
    }
  } else {
    // TODO(mpcomplete): obsolete. remove after migration period.
    // http://code.google.com/p/chromium/issues/detail?id=19733
    extension = extension_file_util::LoadExtension(path,
                                                   true,  // Require id
                                                   &error);
  }

  if (!extension) {
    ReportExtensionLoadError(path, error);
    return;
  }

  // TODO(erikkay) now we only report a single extension loaded at a time.
  // Perhaps we should change the notifications to remove ExtensionList.
  extension->set_location(location);
  ExtensionList* extensions = new ExtensionList;
  if (extension)
    extensions->push_back(extension);
  ReportExtensionsLoaded(extensions);
}

void ExtensionsServiceBackend::ReportExtensionLoadError(
    const FilePath& extension_path, const std::string &error) {
  // TODO(port): note that this isn't guaranteed to work properly on Linux.
  std::string path_str = WideToASCII(extension_path.ToWStringHack());
  std::string message = StringPrintf("Could not load extension from '%s'. %s",
                                     path_str.c_str(), error.c_str());
  ExtensionErrorReporter::GetInstance()->ReportError(message, alert_on_error_);
}

void ExtensionsServiceBackend::ReportExtensionsLoaded(
    ExtensionList* extensions) {
  frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      frontend_, &ExtensionsService::OnExtensionsLoaded, extensions));
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

bool ExtensionsServiceBackend::CheckExternalUninstall(
    const std::string& id, Extension::Location location) {
  // Check if the providers know about this extension.
  ProviderMap::const_iterator i = external_extension_providers_.find(location);
  if (i != external_extension_providers_.end()) {
    scoped_ptr<Version> version;
    version.reset(i->second->RegisteredVersion(id, NULL));
    if (version.get())
      return false;  // Yup, known extension, don't uninstall.
  } else {
    // Not from an external provider, so it's fine.
    return false;
  }

  return true;  // This is not a known extension, uninstall.
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
  frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(frontend_,
      &ExtensionsService::OnExternalExtensionFound, id, version->GetString(),
      path, location));
}
