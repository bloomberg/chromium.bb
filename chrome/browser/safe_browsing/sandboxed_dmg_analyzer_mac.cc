// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/sandboxed_dmg_analyzer_mac.h"

#include <utility>

#include "base/bind.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/safe_browsing/zip_analyzer_results.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace safe_browsing {

SandboxedDMGAnalyzer::SandboxedDMGAnalyzer(const base::FilePath& dmg_file,
                                           const ResultsCallback& callback)
    : file_path_(dmg_file), callback_(callback), callback_called_(false) {
}

SandboxedDMGAnalyzer::~SandboxedDMGAnalyzer() {}

void SandboxedDMGAnalyzer::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, base::TaskTraits()
                     .MayBlock()
                     .WithPriority(base::TaskPriority::BACKGROUND)
                     .WithShutdownBehavior(
                         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN),
      base::Bind(&SandboxedDMGAnalyzer::OpenDMGFile, this));
}

void SandboxedDMGAnalyzer::OpenDMGFile() {
  file_.Initialize(file_path_, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file_.IsValid()) {
    DLOG(ERROR) << "Could not open DMG file at path " << file_path_.value();
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SandboxedDMGAnalyzer::OnAnalysisFinished, this,
                   zip_analyzer::Results()));
    return;
  }

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SandboxedDMGAnalyzer::StartAnalysis,
                                     this));
}

void SandboxedDMGAnalyzer::StartAnalysis() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  content::UtilityProcessHost* utility_process_host =
      content::UtilityProcessHost::Create(
          this, BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));

  utility_process_host->SetName(l10n_util::GetStringUTF16(
      IDS_UTILITY_PROCESS_SAFE_BROWSING_ZIP_FILE_ANALYZER_NAME));
  utility_process_host->Send(
      new ChromeUtilityMsg_AnalyzeDmgFileForDownloadProtection(
          IPC::TakePlatformFileForTransit(std::move(file_))));
}

void SandboxedDMGAnalyzer::OnProcessCrashed(int exit_code) {
  OnAnalysisFinished(zip_analyzer::Results());
}

void SandboxedDMGAnalyzer::OnProcessLaunchFailed(int error_code) {
  OnAnalysisFinished(zip_analyzer::Results());
}

bool SandboxedDMGAnalyzer::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SandboxedDMGAnalyzer, message)
    IPC_MESSAGE_HANDLER(
        ChromeUtilityHostMsg_AnalyzeDmgFileForDownloadProtection_Finished,
        OnAnalysisFinished)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SandboxedDMGAnalyzer::OnAnalysisFinished(
    const zip_analyzer::Results& results) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (callback_called_)
    return;

  callback_called_ = true;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback_, results));
}

}  // namespace safe_browsing
