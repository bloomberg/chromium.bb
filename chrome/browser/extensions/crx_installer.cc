// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/crx_installer.h"

#include "app/l10n_util.h"
#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "grit/chromium_strings.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/glue/image_decoder.h"

namespace {
  // Helper function to delete files. This is used to avoid ugly casts which
  // would be necessary with PostMessage since file_util::Delete is overloaded.
  static void DeleteFileHelper(const FilePath& path, bool recursive) {
    file_util::Delete(path, recursive);
  }
}

void CrxInstaller::Start(const FilePath& crx_path,
                         const FilePath& install_directory,
                         Extension::Location install_source,
                         const std::string& expected_id,
                         bool delete_crx,
                         bool allow_privilege_increase,
                         MessageLoop* file_loop,
                         ExtensionsService* frontend,
                         ExtensionInstallUI* client) {
  // Note: We don't keep a reference because this object manages its own
  // lifetime.
  new CrxInstaller(crx_path, install_directory, install_source, expected_id,
                   delete_crx, allow_privilege_increase, file_loop, frontend,
                   client);
}

CrxInstaller::CrxInstaller(const FilePath& crx_path,
                           const FilePath& install_directory,
                           Extension::Location install_source,
                           const std::string& expected_id,
                           bool delete_crx,
                           bool allow_privilege_increase,
                           MessageLoop* file_loop,
                           ExtensionsService* frontend,
                           ExtensionInstallUI* client)
    : crx_path_(crx_path),
      install_directory_(install_directory),
      install_source_(install_source),
      expected_id_(expected_id),
      delete_crx_(delete_crx),
      allow_privilege_increase_(allow_privilege_increase),
      file_loop_(file_loop),
      ui_loop_(MessageLoop::current()),
      frontend_(frontend),
      client_(client) {

  extensions_enabled_ = frontend_->extensions_enabled();

  unpacker_ = new SandboxedExtensionUnpacker(
      crx_path, g_browser_process->resource_dispatcher_host(), this);

  file_loop->PostTask(FROM_HERE, NewRunnableMethod(unpacker_,
      &SandboxedExtensionUnpacker::Start));
}

CrxInstaller::~CrxInstaller() {
  // Delete the temp directory and crx file as necessary. Note that the
  // destructor might be called on any thread, so we post a task to the file
  // thread to make sure the delete happens there.
  if (!temp_dir_.value().empty()) {
    file_loop_->PostTask(FROM_HERE, NewRunnableFunction(&DeleteFileHelper,
        temp_dir_, true));  // recursive delete
  }

  if (delete_crx_) {
    file_loop_->PostTask(FROM_HERE, NewRunnableFunction(&DeleteFileHelper,
        crx_path_, false));  // non-recursive delete
  }
}

void CrxInstaller::OnUnpackFailure(const std::string& error_message) {
  DCHECK(MessageLoop::current() == file_loop_);
  ReportFailureFromFileThread(error_message);
}

void CrxInstaller::OnUnpackSuccess(const FilePath& temp_dir,
                                   const FilePath& extension_dir,
                                   Extension* extension) {
  DCHECK(MessageLoop::current() == file_loop_);

  // Note: We take ownership of |extension| and |temp_dir|.
  extension_.reset(extension);
  temp_dir_ = temp_dir;

  // The unpack dir we don't have to delete explicity since it is a child of
  // the temp dir.
  unpacked_extension_root_ = extension_dir;
  DCHECK(file_util::ContainsPath(temp_dir_, unpacked_extension_root_));

  // Determine whether to allow installation. We always allow themes and
  // external installs.
  if (!extensions_enabled_ && !extension->IsTheme() &&
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

  if (client_.get()) {
    FilePath icon_path =
        extension_->GetIconPath(Extension::EXTENSION_ICON_LARGE).GetFilePath();
    DecodeInstallIcon(icon_path, &install_icon_);
  }
  ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &CrxInstaller::ConfirmInstall));
}

// static
void CrxInstaller::DecodeInstallIcon(const FilePath& large_icon_path,
                                     scoped_ptr<SkBitmap>* result) {
  if (large_icon_path.empty())
    return;

  std::string file_contents;
  if (!file_util::ReadFileToString(large_icon_path, &file_contents)) {
    LOG(ERROR) << "Could not read icon file: "
               << WideToUTF8(large_icon_path.ToWStringHack());
    return;
  }

  // Decode the image using WebKit's image decoder.
  const unsigned char* data =
      reinterpret_cast<const unsigned char*>(file_contents.data());
  webkit_glue::ImageDecoder decoder;
  scoped_ptr<SkBitmap> decoded(new SkBitmap());
  *decoded = decoder.Decode(data, file_contents.length());
  if (decoded->empty()) {
    LOG(ERROR) << "Could not decode icon file: "
               << WideToUTF8(large_icon_path.ToWStringHack());
    return;
  }

  if (decoded->width() != 128 || decoded->height() != 128) {
    LOG(ERROR) << "Icon file has unexpected size: "
               << IntToString(decoded->width()) << "x"
               << IntToString(decoded->height());
    return;
  }

  result->swap(decoded);
}

void CrxInstaller::ConfirmInstall() {
  DCHECK(MessageLoop::current() == ui_loop_);
  if (frontend_->extension_prefs()->IsExtensionBlacklisted(extension_->id())) {
    LOG(INFO) << "This extension: " << extension_->id()
      << " is blacklisted. Install failed.";
    if (client_.get()) {
      client_->OnInstallFailure("This extension is blacklisted.");
    }
    return;
  }

  current_version_ =
      frontend_->extension_prefs()->GetVersionString(extension_->id());

  if (client_.get()) {
    AddRef();  // balanced in ContinueInstall() and AbortInstall().
    client_->ConfirmInstall(this, extension_.get(), install_icon_.get());
  } else {
    file_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &CrxInstaller::CompleteInstall));
  }
  return;
}

void CrxInstaller::ContinueInstall() {
  file_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &CrxInstaller::CompleteInstall));

  Release();  // balanced in ConfirmInstall().
}

void CrxInstaller::AbortInstall() {
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
  DCHECK(MessageLoop::current() == file_loop_);

  FilePath version_dir;
  Extension::InstallType install_type =
      extension_file_util::CompareToInstalledVersion(
          install_directory_, extension_->id(), current_version_,
          extension_->VersionString(), &version_dir);

  if (install_type == Extension::DOWNGRADE) {
    ReportFailureFromFileThread("Attempted to downgrade extension.");
    return;
  }

  if (install_type == Extension::REINSTALL) {
    // We use this as a signal to switch themes.
    ReportOverinstallFromFileThread();
    return;
  }

  std::string error_msg;
  if (!extension_file_util::InstallExtension(unpacked_extension_root_,
                                             version_dir, &error_msg)) {
    ReportFailureFromFileThread(error_msg);
    return;
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
  DCHECK(MessageLoop::current() == file_loop_);
  ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &CrxInstaller::ReportFailureFromUIThread, error));
}

void CrxInstaller::ReportFailureFromUIThread(const std::string& error) {
  DCHECK(MessageLoop::current() == ui_loop_);

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

  if (client_.get())
    client_->OnInstallFailure(error);
}

void CrxInstaller::ReportOverinstallFromFileThread() {
  DCHECK(MessageLoop::current() == file_loop_);
  ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &CrxInstaller::ReportOverinstallFromUIThread));
}

void CrxInstaller::ReportOverinstallFromUIThread() {
  DCHECK(MessageLoop::current() == ui_loop_);

  if (client_.get())
    client_->OnOverinstallAttempted(extension_.get());

  frontend_->OnExtensionOverinstallAttempted(extension_->id());
}

void CrxInstaller::ReportSuccessFromFileThread() {
  DCHECK(MessageLoop::current() == file_loop_);
  ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &CrxInstaller::ReportSuccessFromUIThread));
}

void CrxInstaller::ReportSuccessFromUIThread() {
  DCHECK(MessageLoop::current() == ui_loop_);

  // If there is a client, tell the client about installation.
  if (client_.get())
    client_->OnInstallSuccess(extension_.get());

  // Tell the frontend about the installation and hand off ownership of
  // extension_ to it.
  frontend_->OnExtensionInstalled(extension_.release(),
                                  allow_privilege_increase_);

  // We're done. We don't post any more tasks to ourselves so we are deleted
  // soon.
}
