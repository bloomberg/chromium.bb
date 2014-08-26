// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/zipfile_installer.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"

using content::BrowserThread;
using content::UtilityProcessHost;

namespace {

const char kExtensionHandlerTempDirError[] =
    "Could not create temporary directory for zipped extension.";

}  // namespace

namespace extensions {

ZipFileInstaller::ZipFileInstaller(ExtensionService* extension_service)
    : be_noisy_on_failure_(true),
      extension_service_weak_(extension_service->AsWeakPtr()) {
}

void ZipFileInstaller::LoadFromZipFile(const base::FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  zip_path_ = path;
  BrowserThread::PostTask(BrowserThread::FILE,
                          FROM_HERE,
                          base::Bind(&ZipFileInstaller::PrepareTempDir, this));
}

void ZipFileInstaller::PrepareTempDir() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::FilePath temp_dir;
  PathService::Get(base::DIR_TEMP, &temp_dir);
  base::FilePath new_temp_dir;
  if (!base::CreateTemporaryDirInDir(
          temp_dir,
          zip_path_.RemoveExtension().BaseName().value() +
              FILE_PATH_LITERAL("_"),
          &new_temp_dir)) {
    OnUnzipFailed(std::string(kExtensionHandlerTempDirError));
    return;
  }
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ZipFileInstaller::StartWorkOnIOThread, this, new_temp_dir));
}

ZipFileInstaller::~ZipFileInstaller() {
}

// static
scoped_refptr<ZipFileInstaller> ZipFileInstaller::Create(
    ExtensionService* extension_service) {
  DCHECK(extension_service);
  return scoped_refptr<ZipFileInstaller>(
      new ZipFileInstaller(extension_service));
}

void ZipFileInstaller::StartWorkOnIOThread(const base::FilePath& temp_dir) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  UtilityProcessHost* host =
      UtilityProcessHost::Create(this, base::MessageLoopProxy::current().get());
  host->SetExposedDir(temp_dir);
  host->Send(new ChromeUtilityMsg_UnzipToDir(zip_path_, temp_dir));
}

void ZipFileInstaller::ReportSuccessOnUIThread(
    const base::FilePath& unzipped_path) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (extension_service_weak_.get())
    UnpackedInstaller::Create(extension_service_weak_.get())
        ->Load(unzipped_path);
}

void ZipFileInstaller::ReportErrorOnUIThread(const std::string& error) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (extension_service_weak_.get()) {
    ExtensionErrorReporter::GetInstance()->ReportLoadError(
        zip_path_,
        error,
        extension_service_weak_->profile(),
        be_noisy_on_failure_);
  }
}

void ZipFileInstaller::OnUnzipSucceeded(const base::FilePath& unzipped_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &ZipFileInstaller::ReportSuccessOnUIThread, this, unzipped_path));
}

void ZipFileInstaller::OnUnzipFailed(const std::string& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ZipFileInstaller::ReportErrorOnUIThread, this, error));
}

bool ZipFileInstaller::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ZipFileInstaller, message)
  IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_UnzipToDir_Succeeded,
                      OnUnzipSucceeded)
  IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_UnzipToDir_Failed, OnUnzipFailed)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

}  // namespace extensions
