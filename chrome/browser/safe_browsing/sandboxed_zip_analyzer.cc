// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/sandboxed_zip_analyzer.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/safe_browsing/zip_analyzer_results.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace safe_browsing {

SandboxedZipAnalyzer::SandboxedZipAnalyzer(
    const base::FilePath& zip_file,
    const ResultCallback& result_callback)
    : zip_file_name_(zip_file),
      callback_(result_callback),
      callback_called_(false) {
}

void SandboxedZipAnalyzer::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Starting the analyzer will block on opening the zip file, so run this
  // on a worker thread.  The task does not need to block shutdown.
  base::PostTaskWithTraits(
      FROM_HERE, base::TaskTraits()
                     .MayBlock()
                     .WithPriority(base::TaskPriority::BACKGROUND)
                     .WithShutdownBehavior(
                         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN),
      base::Bind(&SandboxedZipAnalyzer::AnalyzeInSandbox, this));
}

SandboxedZipAnalyzer::~SandboxedZipAnalyzer() {
  // If we're using UtilityProcessHost, we may not be destroyed on
  // the UI or IO thread.
  CloseTemporaryFile();
}

void SandboxedZipAnalyzer::CloseTemporaryFile() {
  if (!temp_file_.IsValid())
    return;
  // Close the temporary file in the blocking pool since doing so will delete
  // the file.
  base::PostTaskWithTraits(
      FROM_HERE, base::TaskTraits()
                     .MayBlock()
                     .WithPriority(base::TaskPriority::BACKGROUND)
                     .WithShutdownBehavior(
                         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN),
      base::Bind(&base::File::Close,
                 base::Owned(new base::File(std::move(temp_file_)))));
}

void SandboxedZipAnalyzer::AnalyzeInSandbox() {
  // This zip file will be closed on the IO thread once it has been handed
  // off to the child process.
  zip_file_.Initialize(zip_file_name_,
                       base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!zip_file_.IsValid()) {
    DVLOG(1) << "Could not open zip file: " << zip_file_name_.value();
    if (!BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE,
            base::Bind(&SandboxedZipAnalyzer::OnAnalyzeZipFileFinished, this,
                       zip_analyzer::Results()))) {
      NOTREACHED();
    }
    return;
  }

  // This temp file will be closed in the blocking pool when results from the
  // analyzer return.
  base::FilePath temp_path;
  if (base::CreateTemporaryFile(&temp_path)) {
    temp_file_.Initialize(temp_path, (base::File::FLAG_CREATE_ALWAYS |
                                      base::File::FLAG_READ |
                                      base::File::FLAG_WRITE |
                                      base::File::FLAG_TEMPORARY |
                                      base::File::FLAG_DELETE_ON_CLOSE));
  }
  DVLOG_IF(1, !temp_file_.IsValid())
      << "Could not open temporary output file: " << temp_path.value();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SandboxedZipAnalyzer::StartProcessOnIOThread, this));
}

void SandboxedZipAnalyzer::OnProcessCrashed(int exit_code) {
  OnAnalyzeZipFileFinished(zip_analyzer::Results());
}

void SandboxedZipAnalyzer::OnProcessLaunchFailed(int error_code) {
  OnAnalyzeZipFileFinished(zip_analyzer::Results());
}

bool SandboxedZipAnalyzer::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SandboxedZipAnalyzer, message)
    IPC_MESSAGE_HANDLER(
        ChromeUtilityHostMsg_AnalyzeZipFileForDownloadProtection_Finished,
        OnAnalyzeZipFileFinished)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SandboxedZipAnalyzer::StartProcessOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  utility_process_host_ =
      content::UtilityProcessHost::Create(
          this, BrowserThread::GetTaskRunnerForThread(BrowserThread::IO).get())
          ->AsWeakPtr();
  utility_process_host_->SetName(l10n_util::GetStringUTF16(
      IDS_UTILITY_PROCESS_SAFE_BROWSING_ZIP_FILE_ANALYZER_NAME));
  utility_process_host_->Send(
      new ChromeUtilityMsg_AnalyzeZipFileForDownloadProtection(
          IPC::TakePlatformFileForTransit(std::move(zip_file_)),
          IPC::GetPlatformFileForTransit(temp_file_.GetPlatformFile(),
                                         false /* !close_source_handle */)));
}

void SandboxedZipAnalyzer::OnAnalyzeZipFileFinished(
    const zip_analyzer::Results& results) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (callback_called_)
    return;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback_, results));
  callback_called_ = true;
  CloseTemporaryFile();
}

}  // namespace safe_browsing
