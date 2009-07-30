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
#include "grit/chromium_strings.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#elif defined(OS_MACOSX)
#include "base/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"
#include <CoreFoundation/CFUserNotification.h>
#endif

CrxInstaller::CrxInstaller(const FilePath& crx_path,
                           const FilePath& install_directory,
                           Extension::Location install_source,
                           const std::string& expected_id,
                           bool extensions_enabled,
                           bool is_from_gallery,
                           bool show_prompts,
                           bool delete_crx,
                           MessageLoop* file_loop,
                           ExtensionsService* frontend)
    : crx_path_(crx_path),
      install_directory_(install_directory),
      install_source_(install_source),
      expected_id_(expected_id),
      extensions_enabled_(extensions_enabled),
      is_from_gallery_(is_from_gallery),
      show_prompts_(show_prompts),
      delete_crx_(delete_crx),
      file_loop_(file_loop),
      ui_loop_(MessageLoop::current()) {

  // Note: this is a refptr so that we keep the frontend alive long enough to
  // get our response.
  frontend_ = frontend;
  unpacker_ = new SandboxedExtensionUnpacker(
      crx_path, g_browser_process->resource_dispatcher_host(), this);

  file_loop->PostTask(FROM_HERE, NewRunnableMethod(unpacker_,
      &SandboxedExtensionUnpacker::Start));
}

void CrxInstaller::OnUnpackFailure(const std::string& error_message) {
  ReportFailureFromFileThread(error_message);
}

void CrxInstaller::OnUnpackSuccess(const FilePath& temp_dir,
                                   const FilePath& extension_dir,
                                   Extension* extension) {
  // Note: We take ownership of |extension| and |temp_dir|.
  extension_.reset(extension);
  temp_dir_ = temp_dir;

  // temp_dir_deleter is stack allocated instead of a member of CrxInstaller, so
  // that delete always happens on the file thread.
  ScopedTempDir temp_dir_deleter;
  temp_dir_deleter.Set(temp_dir);

  // The unpack dir we don't have to delete explicity since it is a child of
  // the temp dir.
  unpacked_extension_root_ = extension_dir;
  DCHECK(file_util::ContainsPath(temp_dir_, unpacked_extension_root_));

  // If we were supposed to delete the source file, we can do that now.
  if (delete_crx_)
    file_util::Delete(crx_path_, false);  // non-recursive

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
    ReportFailureFromFileThread(
        StringPrintf("ID in new extension manifest (%s) does not match "
                     "expected id (%s)",
                     extension->id().c_str(),
                     expected_id_.c_str()));
    return;
  }

  // Show the confirm UI if necessary.
  // NOTE: We also special case themes to not have a dialog, because we show
  // a special infobar UI for them instead.
  if (show_prompts_ && !extension->IsTheme()) {
    if (!ConfirmInstall())
      return;  // error reported by ConfirmInstall()
  }

  CompleteInstall();
}

bool CrxInstaller::ConfirmInstall() {
#if defined(OS_WIN)
  if (win_util::MessageBox(GetForegroundWindow(),
          L"Are you sure you want to install this extension?\n\n"
          L"You should only install extensions from sources you trust.",
          l10n_util::GetString(IDS_PRODUCT_NAME).c_str(),
          MB_OKCANCEL) != IDOK) {
    ReportFailureFromFileThread("User did not allow extension to be "
                                "installed.");
    return false;
  }
#elif defined(OS_MACOSX)
  // Using CoreFoundation to do this dialog is unimaginably lame but will do
  // until the UI is redone.
  scoped_cftyperef<CFStringRef> product_name(
      base::SysWideToCFStringRef(l10n_util::GetString(IDS_PRODUCT_NAME)));
  CFOptionFlags response;
  if (kCFUserNotificationAlternateResponse == CFUserNotificationDisplayAlert(
      0, kCFUserNotificationCautionAlertLevel, NULL, NULL, NULL,
      product_name,
      CFSTR("Are you sure you want to install this extension?\n\n"
           "This is a temporary message and it will be removed when "
           "extensions UI is finalized."),
      NULL, CFSTR("Cancel"), NULL, &response)) {
    ReportFailureFromFileThread("User did not allow extension to be "
                                "installed.");
    return false;
  }
#endif  // OS_*

  return true;
}

void CrxInstaller::CompleteInstall() {
  FilePath version_dir;
  Extension::InstallType install_type = Extension::INSTALL_ERROR;
  std::string error_msg;
  if (!extension_file_util::InstallExtension(unpacked_extension_root_,
                                             install_directory_,
                                             extension_->id(),
                                             extension_->VersionString(),
                                             &version_dir,
                                             &install_type, &error_msg)) {
    ReportFailureFromFileThread(error_msg);
    return;
  }

  if (install_type == Extension::DOWNGRADE) {
    ReportFailureFromFileThread("Attempted to downgrade extension.");
    return;
  }

  extension_->set_path(version_dir);
  extension_->set_location(install_source_);

  if (install_type == Extension::REINSTALL) {
    // We use this as a signal to switch themes.
    ReportOverinstallFromFileThread();
    return;
  }

  ReportSuccessFromFileThread();
}

void CrxInstaller::ReportFailureFromFileThread(const std::string& error) {
  ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &CrxInstaller::ReportFailureFromUIThread, error));
}

void CrxInstaller::ReportFailureFromUIThread(const std::string& error) {
  ExtensionErrorReporter::GetInstance()->ReportError(error, show_prompts_);
}

void CrxInstaller::ReportOverinstallFromFileThread() {
  ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(frontend_.get(),
      &ExtensionsService::OnExtensionOverinstallAttempted, extension_->id()));
}

void CrxInstaller::ReportSuccessFromFileThread() {
  // Tell the frontend about the installation and hand off ownership of
  // extension_ to it.
  ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(frontend_.get(),
      &ExtensionsService::OnExtensionInstalled, extension_.release()));
}
