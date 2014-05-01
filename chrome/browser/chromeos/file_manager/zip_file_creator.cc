// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/zip_file_creator.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/common/chrome_utility_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"

using content::BrowserThread;
using content::UtilityProcessHost;

namespace {

// Creates the destination zip file only if it does not already exist.
base::File OpenFileHandleOnBlockingThreadPool(const base::FilePath& zip_path) {
  return base::File(zip_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
}

}  // namespace

namespace file_manager {

ZipFileCreator::ZipFileCreator(
    const ResultCallback& callback,
    const base::FilePath& src_dir,
    const std::vector<base::FilePath>& src_relative_paths,
    const base::FilePath& dest_file)
    : callback_(callback),
      src_dir_(src_dir),
      src_relative_paths_(src_relative_paths),
      dest_file_(dest_file) {
  DCHECK(!callback_.is_null());
}

void ZipFileCreator::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&OpenFileHandleOnBlockingThreadPool, dest_file_),
      base::Bind(&ZipFileCreator::OnOpenFileHandle, this));
}

ZipFileCreator::~ZipFileCreator() {
}

bool ZipFileCreator::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ZipFileCreator, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_CreateZipFile_Succeeded,
                        OnCreateZipFileSucceeded)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_CreateZipFile_Failed,
                        OnCreateZipFileFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ZipFileCreator::OnProcessCrashed(int exit_code) {
  ReportDone(false);
}

void ZipFileCreator::OnOpenFileHandle(base::File file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to create dest zip file " << dest_file_.value();
    ReportDone(false);
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &ZipFileCreator::StartProcessOnIOThread, this, base::Passed(&file)));
}

void ZipFileCreator::StartProcessOnIOThread(base::File dest_file) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::FileDescriptor dest_fd(dest_file.Pass());

  UtilityProcessHost* host = UtilityProcessHost::Create(
      this,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI).get());
  host->SetExposedDir(src_dir_);
  host->Send(new ChromeUtilityMsg_CreateZipFile(src_dir_, src_relative_paths_,
                                                dest_fd));
}

void ZipFileCreator::OnCreateZipFileSucceeded() {
  ReportDone(true);
}

void ZipFileCreator::OnCreateZipFileFailed() {
  ReportDone(false);
}

void ZipFileCreator::ReportDone(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Guard against calling observer multiple times.
  if (!callback_.is_null())
    base::ResetAndReturn(&callback_).Run(success);
}

}  // namespace file_manager
