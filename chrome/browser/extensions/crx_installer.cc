// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/crx_installer.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/singleton.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/convert_user_script.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

// Helper function to delete files. This is used to avoid ugly casts which
// would be necessary with PostMessage since file_util::Delete is overloaded.
static void DeleteFileHelper(const FilePath& path, bool recursive) {
  file_util::Delete(path, recursive);
}

struct WhitelistedInstallData {
  WhitelistedInstallData() {}
  std::list<std::string> ids;
};

}

// static
void CrxInstaller::SetWhitelistedInstallId(const std::string& id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  Singleton<WhitelistedInstallData>::get()->ids.push_back(id);
}

// static
bool CrxInstaller::ClearWhitelistedInstallId(const std::string& id) {
  std::list<std::string>& ids = Singleton<WhitelistedInstallData>::get()->ids;
  std::list<std::string>::iterator iter = ids.begin();
  for (; iter != ids.end(); ++iter) {
    if (*iter == id) {
      break;
    }
  }

  if (iter != ids.end()) {
    ids.erase(iter);
    return true;
  }

  return false;
}

CrxInstaller::CrxInstaller(const FilePath& install_directory,
                           ExtensionsService* frontend,
                           ExtensionInstallUI* client)
    : install_directory_(install_directory),
      install_source_(Extension::INTERNAL),
      delete_source_(false),
      allow_privilege_increase_(false),
      limit_web_extent_to_download_host_(false),
      create_app_shortcut_(false),
      frontend_(frontend),
      client_(client),
      apps_require_extension_mime_type_(false),
      allow_silent_install_(false) {
  extensions_enabled_ = frontend_->extensions_enabled();
}

CrxInstaller::~CrxInstaller() {
  // Delete the temp directory and crx file as necessary. Note that the
  // destructor might be called on any thread, so we post a task to the file
  // thread to make sure the delete happens there.
  if (!temp_dir_.value().empty()) {
    ChromeThread::PostTask(
        ChromeThread::FILE, FROM_HERE,
        NewRunnableFunction(&DeleteFileHelper, temp_dir_, true));
  }

  if (delete_source_) {
    ChromeThread::PostTask(
        ChromeThread::FILE, FROM_HERE,
        NewRunnableFunction(&DeleteFileHelper, source_file_, false));
  }

  // Make sure the UI is deleted on the ui thread.
  ChromeThread::DeleteSoon(ChromeThread::UI, FROM_HERE, client_);
  client_ = NULL;
}

void CrxInstaller::InstallCrx(const FilePath& source_file) {
  source_file_ = source_file;

  FilePath user_data_temp_dir;
  CHECK(PathService::Get(chrome::DIR_USER_DATA_TEMP, &user_data_temp_dir));

  scoped_refptr<SandboxedExtensionUnpacker> unpacker(
      new SandboxedExtensionUnpacker(
          source_file,
          user_data_temp_dir,
          g_browser_process->resource_dispatcher_host(),
          this));

  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          unpacker.get(), &SandboxedExtensionUnpacker::Start));
}

void CrxInstaller::InstallUserScript(const FilePath& source_file,
                                     const GURL& original_url) {
  DCHECK(!original_url.is_empty());

  source_file_ = source_file;
  original_url_ = original_url;

  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &CrxInstaller::ConvertUserScriptOnFileThread));
}

void CrxInstaller::ConvertUserScriptOnFileThread() {
  std::string error;
  Extension* extension = ConvertUserScriptToExtension(source_file_,
                                                      original_url_, &error);
  if (!extension) {
    ReportFailureFromFileThread(error);
    return;
  }

  OnUnpackSuccess(extension->path(), extension->path(), extension);
}

void CrxInstaller::OnUnpackFailure(const std::string& error_message) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  ReportFailureFromFileThread(error_message);
}

void CrxInstaller::OnUnpackSuccess(const FilePath& temp_dir,
                                   const FilePath& extension_dir,
                                   Extension* extension) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  // Note: We take ownership of |extension| and |temp_dir|.
  extension_.reset(extension);
  temp_dir_ = temp_dir;

  // If the extension was downloaded, apps_require_extension_mime_type_
  // will be set.  In this case, check that if the extension is an app,
  // it was served with the right mime type.  Make an exception for file
  // URLs, which come from the users computer and have no headers.
  if (extension->is_app() &&
      !original_url_.SchemeIsFile() &&
      apps_require_extension_mime_type_ &&
      original_mime_type_ != Extension::kMimeType) {
    ReportFailureFromFileThread(StringPrintf(
        "Applications must be served with content type %s.",
        Extension::kMimeType));
    return;
  }

  // We don't have to delete the unpack dir explicity since it is a child of
  // the temp dir.
  unpacked_extension_root_ = extension_dir;

  // Determine whether to allow installation. We always allow themes and
  // external installs.
  if (!extensions_enabled_ && !extension->is_theme() &&
      !Extension::IsExternalLocation(install_source_)) {
    ReportFailureFromFileThread("Extensions are not enabled.");
    return;
  }

  // Make sure the expected id matches.
  // TODO(aa): Also support expected version?
  if (!expected_id_.empty() && expected_id_ != extension->id()) {
    ReportFailureFromFileThread(StringPrintf(
        "ID in new extension manifest (%s) does not match expected id (%s)",
        extension->id().c_str(),
        expected_id_.c_str()));
    return;
  }

  // Require that apps are served from the domain they claim in their extent,
  // or some ancestor domain.
  if (extension_->is_app() && limit_web_extent_to_download_host_) {
    URLPattern pattern(UserScript::kValidUserScriptSchemes);
    pattern.set_host(original_url_.host());
    pattern.set_match_subdomains(true);

    for (size_t i = 0; i < extension_->web_extent().patterns().size(); ++i) {
      if (!pattern.MatchesHost(extension_->web_extent().patterns()[i].host())) {
        ReportFailureFromFileThread(StringPrintf(
            "Apps must be served from the host that they affect."));
        return;
      }
    }
  }

  if (client_ || extension_->GetFullLaunchURL().is_valid()) {
    Extension::DecodeIcon(extension_.get(), Extension::EXTENSION_ICON_LARGE,
                          &install_icon_);
  }

  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this, &CrxInstaller::ConfirmInstall));
}

void CrxInstaller::ConfirmInstall() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (frontend_->extension_prefs()->IsExtensionBlacklisted(extension_->id())) {
    LOG(INFO) << "This extension: " << extension_->id()
      << " is blacklisted. Install failed.";
    ReportFailureFromUIThread("This extension is blacklisted.");
    return;
  }

  if (!frontend_->extension_prefs()->IsExtensionAllowedByPolicy(
      extension_->id())) {
    ReportFailureFromUIThread("This extension is blacklisted by admin policy.");
    return;
  }

  GURL overlapping_url;
  Extension* overlapping_extension =
      frontend_->GetExtensionByOverlappingWebExtent(extension_->web_extent());
  if (overlapping_extension) {
    ReportFailureFromUIThread(l10n_util::GetStringFUTF8(
        IDS_EXTENSION_OVERLAPPING_WEB_EXTENT,
        UTF8ToUTF16(overlapping_extension->name())));
    return;
  }

  current_version_ =
      frontend_->extension_prefs()->GetVersionString(extension_->id());

  if (client_ &&
      (!allow_silent_install_ ||
       !ClearWhitelistedInstallId(extension_->id()))) {
    AddRef();  // Balanced in Proceed() and Abort().
    client_->ConfirmInstall(this, extension_.get());
  } else {
    ChromeThread::PostTask(
        ChromeThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &CrxInstaller::CompleteInstall));
  }
  return;
}

void CrxInstaller::InstallUIProceed(bool create_app_shortcut) {
  if (create_app_shortcut) {
    DCHECK(extension_->GetFullLaunchURL().is_valid());
    create_app_shortcut_ = true;
  }

  ChromeThread::PostTask(
        ChromeThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &CrxInstaller::CompleteInstall));

  Release();  // balanced in ConfirmInstall().
}

void CrxInstaller::InstallUIAbort() {
  // Kill the theme loading bubble.
  NotificationService* service = NotificationService::current();
  service->Notify(NotificationType::NO_THEME_DETECTED,
                  Source<CrxInstaller>(this),
                  NotificationService::NoDetails());
  Release();  // balanced in ConfirmInstall().

  // We're done. Since we don't post any more tasks to ourself, our ref count
  // should go to zero and we die. The destructor will clean up the temp dir.
}

void CrxInstaller::CompleteInstall() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  if (!current_version_.empty()) {
    scoped_ptr<Version> current_version(
        Version::GetVersionFromString(current_version_));
    if (current_version->CompareTo(*(extension_->version())) > 0) {
      ReportFailureFromFileThread("Attempted to downgrade extension.");
      return;
    }
  }

  FilePath version_dir = extension_file_util::InstallExtension(
      unpacked_extension_root_,
      extension_->id(),
      extension_->VersionString(),
      install_directory_);
  if (version_dir.empty()) {
    ReportFailureFromFileThread(
        l10n_util::GetStringUTF8(
            IDS_EXTENSION_MOVE_DIRECTORY_TO_PROFILE_FAILED));
    return;
  }

  if (create_app_shortcut_) {
    SkBitmap icon = install_icon_.get() ? *install_icon_ :
        *ResourceBundle::GetSharedInstance().GetBitmapNamed(
            IDR_EXTENSION_DEFAULT_ICON);

    ShellIntegration::ShortcutInfo shortcut_info;
    shortcut_info.url = extension_->GetFullLaunchURL();
    shortcut_info.extension_id = UTF8ToUTF16(extension_->id());
    shortcut_info.title = UTF8ToUTF16(extension_->name());
    shortcut_info.description = UTF8ToUTF16(extension_->description());
    shortcut_info.favicon = icon;
    shortcut_info.create_on_desktop = true;

    // TODO(aa): Seems nasty to be reusing the old webapps code this way. What
    // baggage am I inheriting?
    web_app::CreateShortcut(frontend_->profile()->GetPath(), shortcut_info,
                            NULL);
  }

  // This is lame, but we must reload the extension because absolute paths
  // inside the content scripts are established inside InitFromValue() and we
  // just moved the extension.
  // TODO(aa): All paths to resources inside extensions should be created
  // lazily and based on the Extension's root path at that moment.
  std::string error;
  extension_.reset(extension_file_util::LoadExtension(version_dir, true,
                                                      &error));
  DCHECK(error.empty());
  extension_->set_location(install_source_);

  ReportSuccessFromFileThread();
}

void CrxInstaller::ReportFailureFromFileThread(const std::string& error) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this, &CrxInstaller::ReportFailureFromUIThread, error));
}

void CrxInstaller::ReportFailureFromUIThread(const std::string& error) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  NotificationService* service = NotificationService::current();
  service->Notify(NotificationType::EXTENSION_INSTALL_ERROR,
                  Source<CrxInstaller>(this),
                  Details<const std::string>(&error));

  // This isn't really necessary, it is only used because unit tests expect to
  // see errors get reported via this interface.
  //
  // TODO(aa): Need to go through unit tests and clean them up too, probably get
  // rid of this line.
  ExtensionErrorReporter::GetInstance()->ReportError(error, false);  // quiet

  if (client_)
    client_->OnInstallFailure(error);
}

void CrxInstaller::ReportSuccessFromFileThread() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this, &CrxInstaller::ReportSuccessFromUIThread));
}

void CrxInstaller::ReportSuccessFromUIThread() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  // If there is a client, tell the client about installation.
  if (client_)
    client_->OnInstallSuccess(extension_.get());

  // Tell the frontend about the installation and hand off ownership of
  // extension_ to it.
  frontend_->OnExtensionInstalled(extension_.release(),
                                  allow_privilege_increase_);

  // We're done. We don't post any more tasks to ourselves so we are deleted
  // soon.
}
