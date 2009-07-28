// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extensions_service.h"

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_file_util.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_updater.h"
#include "chrome/browser/extensions/external_extension_provider.h"
#include "chrome/browser/extensions/external_pref_extension_provider.h"
#include "chrome/browser/extensions/theme_preview_infobar_delegate.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#include "chrome/browser/extensions/external_registry_extension_provider_win.h"
#elif defined(OS_MACOSX)
#include "base/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"
#include <CoreFoundation/CFUserNotification.h>
#endif

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

// This class hosts a SandboxedExtensionUnpacker task and routes the results
// back to ExtensionsService. The unpack process is started immediately on
// construction of this object.
class ExtensionsServiceBackend::UnpackerClient
    : public SandboxedExtensionUnpackerClient {
 public:
  UnpackerClient(ExtensionsServiceBackend* backend,
                 const FilePath& extension_path,
                 const std::string& expected_id,
                 bool silent, bool from_gallery)
    : backend_(backend), extension_path_(extension_path),
      expected_id_(expected_id), silent_(silent), from_gallery_(from_gallery) {
    unpacker_ = new SandboxedExtensionUnpacker(extension_path,
            backend->resource_dispatcher_host_, this);
    unpacker_->Start();
  }

 private:
  // SandboxedExtensionUnpackerClient
  virtual void OnUnpackSuccess(const FilePath& temp_dir,
                               const FilePath& extension_dir,
                               Extension* extension) {
    backend_->OnExtensionUnpacked(extension_path_, extension_dir, extension,
                                  expected_id_, silent_, from_gallery_);
    file_util::Delete(temp_dir, true);
    delete this;
  }

  virtual void OnUnpackFailure(const std::string& error_message) {
    backend_->ReportExtensionInstallError(extension_path_, error_message);
    delete this;
  }

  scoped_refptr<SandboxedExtensionUnpacker> unpacker_;

  scoped_refptr<ExtensionsServiceBackend> backend_;

  // The path to the crx file that we're installing.
  FilePath extension_path_;

  // The path to the copy of the crx file in the temporary directory where we're
  // unpacking it.
  FilePath temp_extension_path_;

  // The ID we expect this extension to have, if any.
  std::string expected_id_;

  // True if the install should be done with no confirmation dialog.
  bool silent_;

  // True if the install is from the gallery (and therefore should not get an
  // alert UI if it turns out to also be a theme).
  bool from_gallery_;
};

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
  if (command_line->HasSwitch(switches::kEnableExtensions))
    extensions_enabled_ = true;
  else if (profile->GetPrefs()->GetBoolean(prefs::kEnableExtensions))
    extensions_enabled_ = true;

  // Set up the ExtensionUpdater
  if (autoupdate_enabled) {
    int update_frequency = kDefaultUpdateFrequencySeconds;
    if (command_line->HasSwitch(switches::kExtensionsUpdateFrequency)) {
      update_frequency = StringToInt(WideToASCII(command_line->GetSwitchValue(
          switches::kExtensionsUpdateFrequency)));
    }
    updater_ = new ExtensionUpdater(this, update_frequency, backend_loop_);
  }

  backend_ = new ExtensionsServiceBackend(
          install_directory_, g_browser_process->resource_dispatcher_host(),
          frontend_loop, extensions_enabled());
}

ExtensionsService::~ExtensionsService() {
  UnloadAllExtensions();
  if (updater_.get()) {
    updater_->Stop();
  }
}

void ExtensionsService::SetExtensionsEnabled(bool enabled) {
  extensions_enabled_ = true;
  backend_loop_->PostTask(FROM_HERE, NewRunnableMethod(backend_.get(),
    &ExtensionsServiceBackend::set_extensions_enabled, enabled));
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
  InstallExtension(extension_path, GURL(), GURL());
}

void ExtensionsService::InstallExtension(const FilePath& extension_path,
                                         const GURL& download_url,
                                         const GURL& referrer_url) {
  backend_loop_->PostTask(FROM_HERE, NewRunnableMethod(backend_.get(),
      &ExtensionsServiceBackend::InstallExtension, extension_path,
      IsDownloadFromGallery(download_url, referrer_url),
      scoped_refptr<ExtensionsService>(this)));

}

void ExtensionsService::UpdateExtension(const std::string& id,
                                        const FilePath& extension_path,
                                        bool alert_on_error,
                                        ExtensionInstallCallback* callback) {
  if (callback) {
    if (install_callbacks_.find(extension_path) != install_callbacks_.end()) {
      // We can't have multiple outstanding install requests for the same
      // path, so immediately indicate error via the callback here.
      LOG(WARNING) << "Dropping update request for '" <<
          extension_path.value() << "' (already in progress)'";
      callback->Run(extension_path, static_cast<Extension*>(NULL));
      delete callback;
      return;
    }
    install_callbacks_[extension_path] =
        linked_ptr<ExtensionInstallCallback>(callback);
  }

  if (!GetExtensionById(id)) {
    LOG(WARNING) << "Will not update extension " << id << " because it is not "
        << "installed";
    FireInstallCallback(extension_path, NULL);
    return;
  }

  backend_loop_->PostTask(FROM_HERE, NewRunnableMethod(backend_.get(),
      &ExtensionsServiceBackend::UpdateExtension, id, extension_path,
      alert_on_error, scoped_refptr<ExtensionsService>(this)));
}

void ExtensionsService::ReloadExtension(const std::string& extension_id) {
  Extension* extension = GetExtensionById(extension_id);
  FilePath extension_path = extension->path();

  UnloadExtension(extension_id);
  LoadExtension(extension_path);
}

void ExtensionsService::UninstallExtension(const std::string& extension_id,
                                           bool external_uninstall) {
  Extension* extension = GetExtensionById(extension_id);

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

void ExtensionsService::CheckForExternalUpdates() {
  // This installs or updates externally provided extensions.
  std::set<std::string> killed_extensions;
  extension_prefs_->GetKilledExtensionIds(&killed_extensions);
  backend_loop_->PostTask(FROM_HERE, NewRunnableMethod(backend_.get(),
      &ExtensionsServiceBackend::CheckForExternalUpdates,
      killed_extensions,
      scoped_refptr<ExtensionsService>(this)));
}

void ExtensionsService::UnloadExtension(const std::string& extension_id) {
  Extension* extension = NULL;
  ExtensionList::iterator iter;
  for (iter = extensions_.begin(); iter != extensions_.end(); ++iter) {
    if ((*iter)->id() == extension_id) {
      extension = *iter;
      break;
    }
  }

  // Callers should not send us nonexistant extensions.
  CHECK(extension);

  // Remove the extension from our list.
  extensions_.erase(iter);

  // Tell other services the extension is gone.
  NotificationService::current()->Notify(NotificationType::EXTENSION_UNLOADED,
                                         Source<ExtensionsService>(this),
                                         Details<Extension>(extension));

  delete extension;
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
  backend_loop_->PostTask(FROM_HERE, NewRunnableFunction(
      &extension_file_util::GarbageCollectExtensions, install_directory_));
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
  for (ExtensionList::iterator iter = new_extensions->begin();
       iter != new_extensions->end(); ++iter) {
    if (extensions_enabled() ||
        (*iter)->IsTheme() ||
        (*iter)->location() == Extension::LOAD ||
        Extension::IsExternalLocation((*iter)->location())) {
      Extension* old = GetExtensionById((*iter)->id());
      if (old) {
        if ((*iter)->version()->CompareTo(*(old->version())) > 0) {
          // To upgrade an extension in place, unload the old one and
          // then load the new one.
          // TODO(erikkay) issue 12399
          UnloadExtension(old->id());
        } else {
          // We already have the extension of the same or older version.
          LOG(WARNING) << "Duplicate extension load attempt: " << (*iter)->id();
          delete *iter;
          continue;
        }
      }
      enabled_extensions.push_back(*iter);
      extensions_.push_back(*iter);
    } else {
      // Extensions that get enabled get added to extensions_ and deleted later.
      // Anything skipped must be deleted now so we don't leak.
      delete *iter;
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

void ExtensionsService::OnExtensionInstalled(const FilePath& path,
    Extension* extension, Extension::InstallType install_type) {
  FireInstallCallback(path, extension);
  extension_prefs_->OnExtensionInstalled(extension);

  // If the extension is a theme, tell the profile (and therefore ThemeProvider)
  // to apply it.
  if (extension->IsTheme()) {
    ShowThemePreviewInfobar(extension);
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
}


void ExtensionsService::OnExtenionInstallError(const FilePath& path) {
  FireInstallCallback(path, NULL);
}

void ExtensionsService::FireInstallCallback(const FilePath& path,
                                            Extension* extension) {
  CallbackMap::iterator iter = install_callbacks_.find(path);
  if (iter != install_callbacks_.end()) {
    iter->second->Run(path, extension);
    install_callbacks_.erase(iter);
  }
}

void ExtensionsService::OnExtensionOverinstallAttempted(const std::string& id,
    const FilePath& path) {
  FireInstallCallback(path, NULL);
  Extension* extension = GetExtensionById(id);
  if (extension && extension->IsTheme()) {
    ShowThemePreviewInfobar(extension);
    NotificationService::current()->Notify(
        NotificationType::THEME_INSTALLED,
        Source<ExtensionsService>(this),
        Details<Extension>(extension));
  }
}

Extension* ExtensionsService::GetExtensionById(const std::string& id) {
  std::string lowercase_id = StringToLowerASCII(id);
  for (ExtensionList::const_iterator iter = extensions_.begin();
      iter != extensions_.end(); ++iter) {
    if ((*iter)->id() == lowercase_id)
      return *iter;
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

bool ExtensionsService::ShowThemePreviewInfobar(Extension* extension) {
  if (!profile_)
    return false;

  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (!browser)
    return false;

  TabContents* tab_contents = browser->GetSelectedTabContents();
  if (!tab_contents)
    return false;

  tab_contents->AddInfoBar(new ThemePreviewInfobarDelegate(tab_contents,
                                                           extension->name()));
  return true;
}

// ExtensionsServicesBackend

ExtensionsServiceBackend::ExtensionsServiceBackend(
    const FilePath& install_directory, ResourceDispatcherHost* rdh,
    MessageLoop* frontend_loop, bool extensions_enabled)
        : frontend_(NULL),
          install_directory_(install_directory),
          resource_dispatcher_host_(rdh),
          alert_on_error_(false),
          frontend_loop_(frontend_loop),
          extensions_enabled_(extensions_enabled) {
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
  scoped_ptr<InstalledExtensions::Callback> callback(
      NewCallback(this, &ExtensionsServiceBackend::LoadInstalledExtension));
  installed->VisitInstalledExtensions(callback.get());

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
    const std::string& id, const FilePath& path, Extension::Location location) {
  if (CheckExternalUninstall(id, location)) {
    frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        frontend_,
        &ExtensionsService::UninstallExtension,
        id, true));

    // No error needs to be reported. The extension effectively doesn't exist.
    return;
  }

  std::string error;
  Extension* extension = extension_file_util::LoadExtension(
      path,
      true,  // Require id
      &error);

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

void ExtensionsServiceBackend::InstallExtension(
    const FilePath& extension_path, bool from_gallery,
    scoped_refptr<ExtensionsService> frontend) {
  LOG(INFO) << "Installing extension " << extension_path.value();

  frontend_ = frontend;
  alert_on_error_ = true;

  InstallOrUpdateExtension(extension_path, from_gallery, std::string(), false);
}

void ExtensionsServiceBackend::UpdateExtension(const std::string& id,
    const FilePath& extension_path, bool alert_on_error,
    scoped_refptr<ExtensionsService> frontend) {
  LOG(INFO) << "Updating extension " << id << " " << extension_path.value();

  frontend_ = frontend;
  alert_on_error_ = alert_on_error;

  InstallOrUpdateExtension(extension_path, false, id, true);
}

void ExtensionsServiceBackend::InstallOrUpdateExtension(
    const FilePath& extension_path, bool from_gallery,
    const std::string& expected_id, bool silent) {
  // NOTE: We don't need to keep a reference to this, it deletes itself when it
  // is done.
  new UnpackerClient(this, extension_path, expected_id, silent, from_gallery);
}

void ExtensionsServiceBackend::OnExtensionUnpacked(
    const FilePath& crx_path, const FilePath& unpacked_path,
    Extension* extension, const std::string expected_id, bool silent,
    bool from_gallery) {
  // Take ownership of the extension object.
  scoped_ptr<Extension> extension_deleter(extension);

  Extension::Location location = Extension::INTERNAL;
  LookupExternalExtension(extension->id(), NULL, &location);
  extension->set_location(location);

  bool allow_install = false;
  if (extensions_enabled_)
    allow_install = true;

  // Always allow themes.
  if (extension->IsTheme())
    allow_install = true;

  // Always allow externally installed extensions (partners use this).
  if (Extension::IsExternalLocation(location))
    allow_install = true;

  if (!allow_install) {
    ReportExtensionInstallError(crx_path, "Extensions are not enabled.");
    return;
  }

  // TODO(extensions): Make better extensions UI. http://crbug.com/12116

  // We also skip the dialog for a few special cases:
  // - themes (because we show the preview infobar for them)
  // - externally registered extensions
  // - during tests (!frontend->show_extension_prompts())
  // - autoupdate (silent).
  bool show_dialog = true;
  if (extension->IsTheme())
    show_dialog = false;

  if (Extension::IsExternalLocation(location))
    show_dialog = false;

  if (silent || !frontend_->show_extensions_prompts())
    show_dialog = false;

  if (show_dialog) {
#if defined(OS_WIN)
    if (win_util::MessageBox(GetForegroundWindow(),
            L"Are you sure you want to install this extension?\n\n"
            L"You should only install extensions from sources you trust.",
            l10n_util::GetString(IDS_PRODUCT_NAME).c_str(),
            MB_OKCANCEL) != IDOK) {
      return;
    }
#elif defined(OS_MACOSX)
    // Using CoreFoundation to do this dialog is unimaginably lame but will do
    // until the UI is redone.
    scoped_cftyperef<CFStringRef> product_name(
        base::SysWideToCFStringRef(l10n_util::GetString(IDS_PRODUCT_NAME)));
    CFOptionFlags response;
    CFUserNotificationDisplayAlert(
        0, kCFUserNotificationCautionAlertLevel, NULL, NULL, NULL,
        product_name,
        CFSTR("Are you sure you want to install this extension?\n\n"
             "This is a temporary message and it will be removed when "
             "extensions UI is finalized."),
        NULL, CFSTR("Cancel"), NULL, &response);

    if (response == kCFUserNotificationAlternateResponse) {
      ReportExtensionInstallError(crx_path,
          "User did not allow extension to be installed.");
      return;
    }
#endif  // OS_*
  }

  // If an expected id was provided, make sure it matches.
  if (!expected_id.empty() && expected_id != extension->id()) {
    ReportExtensionInstallError(crx_path,
        StringPrintf("ID in new extension manifest (%s) does not match "
                     "expected id (%s)", extension->id().c_str(),
                     expected_id.c_str()));
    return;
  }

  FilePath version_dir;
  Extension::InstallType install_type = Extension::INSTALL_ERROR;
  std::string error_msg;
  if (!extension_file_util::InstallExtension(unpacked_path, install_directory_,
                                             extension->id(),
                                             extension->VersionString(),
                                             &version_dir,
                                             &install_type, &error_msg)) {
    ReportExtensionInstallError(crx_path, error_msg);
    return;
  }

  if (install_type == Extension::DOWNGRADE) {
    ReportExtensionInstallError(crx_path, "Attempted to downgrade extension.");
    return;
  }

  extension->set_path(version_dir);

  if (install_type == Extension::REINSTALL) {
    // The client may use this as a signal (to switch themes, for instance).
    ReportExtensionOverinstallAttempted(extension->id(), crx_path);
    return;
  }

  frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      frontend_, &ExtensionsService::OnExtensionInstalled, crx_path,
      extension, install_type));

  // Only one extension, but ReportExtensionsLoaded can handle multiple,
  // so we need to construct a list.
  ExtensionList* extensions = new ExtensionList;

  // Hand off ownership of the extension to the frontend.
  extensions->push_back(extension_deleter.release());
  ReportExtensionsLoaded(extensions);
}

void ExtensionsServiceBackend::ReportExtensionInstallError(
    const FilePath& extension_path,  const std::string &error) {

  // TODO(erikkay): note that this isn't guaranteed to work properly on Linux.
  std::string path_str = WideToASCII(extension_path.ToWStringHack());
  std::string message =
      StringPrintf("Could not install extension from '%s'. %s",
                   path_str.c_str(), error.c_str());
  ExtensionErrorReporter::GetInstance()->ReportError(message, alert_on_error_);

  frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      frontend_,
      &ExtensionsService::OnExtenionInstallError,
      extension_path));
}

void ExtensionsServiceBackend::ReportExtensionOverinstallAttempted(
    const std::string& id, const FilePath& path) {
  frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      frontend_, &ExtensionsService::OnExtensionOverinstallAttempted, id,
      path));
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
    const std::string& id, const Version* version, const FilePath& path) {
  InstallOrUpdateExtension(path,
                           false,  // not from gallery
                           id,  // expected id
                           true);  // silent
}
