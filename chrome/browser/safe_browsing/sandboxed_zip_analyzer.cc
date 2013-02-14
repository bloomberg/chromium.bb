// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/sandboxed_zip_analyzer.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/safe_browsing/zip_analyzer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"

using content::BrowserThread;

namespace safe_browsing {

SandboxedZipAnalyzer::SandboxedZipAnalyzer(
    const base::FilePath& zip_file,
    const ResultCallback& result_callback)
    : zip_file_(zip_file), callback_(result_callback) {}

void SandboxedZipAnalyzer::Start() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Starting the analyzer will block on opening the zip file, so run this
  // on a worker thread.  The task does not need to block shutdown.
  if (!BrowserThread::GetBlockingPool()->PostWorkerTaskWithShutdownBehavior(
          FROM_HERE,
          base::Bind(&SandboxedZipAnalyzer::AnalyzeInSandbox, this),
          base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN)) {
    NOTREACHED();
  }
}

SandboxedZipAnalyzer::~SandboxedZipAnalyzer() {
  // If we're using UtilityProcessHost, we may not be destroyed on
  // the UI or IO thread.
}

void SandboxedZipAnalyzer::AnalyzeInSandbox() {
  zip_platform_file_ = base::CreatePlatformFile(
      zip_file_,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
      NULL,   // created
      NULL);  // error_code
  if (zip_platform_file_ == base::kInvalidPlatformFileValue) {
    VLOG(1) << "Could not open zip file: " << zip_file_.value();
    if (!BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE,
            base::Bind(&SandboxedZipAnalyzer::OnAnalyzeZipFileFinished, this,
                       zip_analyzer::Results()))) {
      NOTREACHED();
    }
    return;
  }

  // TODO(asargent) we shouldn't need to do this branch here - instead
  // UtilityProcessHost should handle it for us. (http://crbug.com/19192)
  bool use_utility_process = content::ResourceDispatcherHost::Get() &&
      !CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess);
  if (use_utility_process) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &SandboxedZipAnalyzer::StartProcessOnIOThread, this));
    // The file will be closed on the IO thread once it has been handed
    // off to the child process.
  } else {
    zip_analyzer::Results results;
    zip_analyzer::AnalyzeZipFile(zip_platform_file_, &results);
    base::ClosePlatformFile(zip_platform_file_);
    if (!BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE,
            base::Bind(
                &SandboxedZipAnalyzer::OnAnalyzeZipFileFinished, this,
                results))) {
      NOTREACHED();
    }
  }
}

bool SandboxedZipAnalyzer::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SandboxedZipAnalyzer, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ProcessStarted,
                        OnUtilityProcessStarted)
    IPC_MESSAGE_HANDLER(
        ChromeUtilityHostMsg_AnalyzeZipFileForDownloadProtection_Finished,
        OnAnalyzeZipFileFinished)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SandboxedZipAnalyzer::OnAnalyzeZipFileFinished(
    const zip_analyzer::Results& results) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SandboxedZipAnalyzer::RunCallback, this, results));
}

void SandboxedZipAnalyzer::RunCallback(const zip_analyzer::Results& results) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  callback_.Run(results);
}

void SandboxedZipAnalyzer::StartProcessOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  utility_process_host_ = content::UtilityProcessHost::Create(
      this,
      BrowserThread::GetMessageLoopProxyForThread(
          BrowserThread::IO))->AsWeakPtr();
  utility_process_host_->Send(new ChromeUtilityMsg_StartupPing);
  // Wait for the startup notification before sending the main IPC to the
  // utility process, so that we can dup the file handle.
}

void SandboxedZipAnalyzer::OnUtilityProcessStarted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (utility_process_host_->GetData().handle == base::kNullProcessHandle) {
    DLOG(ERROR) << "Child process handle is null";
  }
  utility_process_host_->Send(
      new ChromeUtilityMsg_AnalyzeZipFileForDownloadProtection(
          IPC::GetFileHandleForProcess(
              zip_platform_file_,
              utility_process_host_->GetData().handle,
              true /* close_source_handle */)));
}

}  // namespace safe_browsing
