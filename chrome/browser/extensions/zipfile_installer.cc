// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/zipfile_installer.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/common/extension_unpacker.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kExtensionHandlerTempDirError[] =
    "Could not create temporary directory for zipped extension.";
const char kExtensionHandlerFileUnzipError[] =
    "Could not unzip extension for install.";

base::Optional<base::FilePath> PrepareAndGetUnzipDir(
    const base::FilePath& zip_file) {
  base::AssertBlockingAllowed();

  base::FilePath dir_temp;
  base::PathService::Get(base::DIR_TEMP, &dir_temp);

  base::FilePath::StringType dir_name =
      zip_file.RemoveExtension().BaseName().value() + FILE_PATH_LITERAL("_");

  base::FilePath unzip_dir;
  if (!base::CreateTemporaryDirInDir(dir_temp, dir_name, &unzip_dir))
    return base::Optional<base::FilePath>();

  return unzip_dir;
}

}  // namespace

namespace extensions {

// static
scoped_refptr<ZipFileInstaller> ZipFileInstaller::Create(
    ExtensionService* service) {
  DCHECK(service);
  return base::WrapRefCounted(new ZipFileInstaller(service));
}

void ZipFileInstaller::LoadFromZipFile(const base::FilePath& zip_file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!zip_file.empty());

  zip_file_ = zip_file;

  base::PostTaskAndReplyWithResult(
      GetExtensionFileTaskRunner().get(), FROM_HERE,
      base::BindOnce(&PrepareAndGetUnzipDir, zip_file),
      base::BindOnce(&ZipFileInstaller::Unzip, this));
}

ZipFileInstaller::ZipFileInstaller(ExtensionService* service)
    : be_noisy_on_failure_(true),
      extension_service_weak_(service->AsWeakPtr()) {}

ZipFileInstaller::~ZipFileInstaller() = default;

void ZipFileInstaller::Unzip(base::Optional<base::FilePath> unzip_dir) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!unzip_dir) {
    ReportFailure(std::string(kExtensionHandlerTempDirError));
    return;
  }
  DCHECK(!utility_process_mojo_client_);

  utility_process_mojo_client_ = base::MakeUnique<
      content::UtilityProcessMojoClient<mojom::ExtensionUnpacker>>(
      l10n_util::GetStringUTF16(IDS_UTILITY_PROCESS_ZIP_FILE_INSTALLER_NAME));
  utility_process_mojo_client_->set_error_callback(
      base::Bind(&ZipFileInstaller::UnzipDone, this, *unzip_dir, false));

  utility_process_mojo_client_->set_exposed_directory(*unzip_dir);

  utility_process_mojo_client_->Start();

  utility_process_mojo_client_->service()->Unzip(
      zip_file_, *unzip_dir,
      base::Bind(&ZipFileInstaller::UnzipDone, this, *unzip_dir));
}

void ZipFileInstaller::UnzipDone(const base::FilePath& unzip_dir,
                                 bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  utility_process_mojo_client_.reset();

  if (!success) {
    ReportFailure(kExtensionHandlerFileUnzipError);
    return;
  }

  if (extension_service_weak_)
    UnpackedInstaller::Create(extension_service_weak_.get())->Load(unzip_dir);
}

void ZipFileInstaller::ReportFailure(const std::string& error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (extension_service_weak_) {
    ExtensionErrorReporter::GetInstance()->ReportLoadError(
        zip_file_, error, extension_service_weak_->profile(),
        be_noisy_on_failure_);
  }
}

}  // namespace extensions
