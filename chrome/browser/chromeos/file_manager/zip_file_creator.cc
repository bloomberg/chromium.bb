// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/zip_file_creator.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util_proxy.h"
#include "base/memory/scoped_handle.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"
#include "grit/generated_resources.h"

using content::BrowserThread;
using content::UtilityProcessHost;

namespace file_manager {

ZipFileCreator::ZipFileCreator(
    Observer* observer,
    const base::FilePath& src_dir,
    const std::vector<base::FilePath>& src_relative_paths,
    const base::FilePath& dest_file)
    : thread_identifier_(BrowserThread::ID_COUNT),
      observer_(observer),
      src_dir_(src_dir),
      src_relative_paths_(src_relative_paths),
      dest_file_(dest_file),
      got_response_(false) {
}

void ZipFileCreator::Start() {
  CHECK(BrowserThread::GetCurrentThreadIdentifier(&thread_identifier_));
  BrowserThread::GetBlockingPool()->PostTask(
      FROM_HERE,
      base::Bind(&ZipFileCreator::OpenFileHandleOnBlockingThreadPool, this));
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
  // Don't report crashes if they happen after we got a response.
  if (got_response_)
    return;

  // Utility process crashed while trying to create the zip file.
  ReportDone(false);
}

void ZipFileCreator::OpenFileHandleOnBlockingThreadPool() {
  // Create the destination zip file only if it does not already exist.
  base::File dest_file(dest_file_,
                       base::File::FLAG_CREATE | base::File::FLAG_WRITE);

  if (!dest_file.IsValid()) {
    LOG(ERROR) << "Failed to create dest zip file " << dest_file_.value();

    BrowserThread::GetMessageLoopProxyForThread(thread_identifier_)->PostTask(
        FROM_HERE,
        base::Bind(&ZipFileCreator::ReportDone, this, false));
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ZipFileCreator::StartProcessOnIOThread, this,
                 Passed(&dest_file)));
}

void ZipFileCreator::StartProcessOnIOThread(base::File dest_file) {
  base::FileDescriptor dest_fd(dest_file.Pass());

  UtilityProcessHost* host = UtilityProcessHost::Create(
      this,
      BrowserThread::GetMessageLoopProxyForThread(thread_identifier_).get());
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
  // Skip check for unittests.
  if (thread_identifier_ != BrowserThread::ID_COUNT)
    DCHECK(BrowserThread::CurrentlyOn(thread_identifier_));

  // Guard against calling observer multiple times.
  if (got_response_)
    return;

  got_response_ = true;
  observer_->OnZipDone(success);
}

}  // namespace file_manager
