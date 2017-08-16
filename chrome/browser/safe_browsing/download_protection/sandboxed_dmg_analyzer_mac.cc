// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/sandboxed_dmg_analyzer_mac.h"

#include <utility>

#include "base/bind.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

namespace safe_browsing {

SandboxedDMGAnalyzer::SandboxedDMGAnalyzer(const base::FilePath& dmg_file,
                                           const ResultCallback& callback)
    : file_path_(dmg_file), callback_(callback) {
  DCHECK(callback);
}

void SandboxedDMGAnalyzer::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BACKGROUND,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::Bind(&SandboxedDMGAnalyzer::PrepareFileToAnalyze, this));
}

SandboxedDMGAnalyzer::~SandboxedDMGAnalyzer() = default;

void SandboxedDMGAnalyzer::PrepareFileToAnalyze() {
  base::File file(file_path_, base::File::FLAG_OPEN | base::File::FLAG_READ);

  if (!file.IsValid()) {
    DLOG(ERROR) << "Could not open file: " << file_path_.value();
    ReportFileFailure();
    return;
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&SandboxedDMGAnalyzer::AnalyzeFile, this,
                 base::Passed(&file)));
}

void SandboxedDMGAnalyzer::ReportFileFailure() {
  DCHECK(!utility_process_mojo_client_);

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(callback_, ArchiveAnalyzerResults()));
}

void SandboxedDMGAnalyzer::AnalyzeFile(base::File file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!utility_process_mojo_client_);

  utility_process_mojo_client_ = base::MakeUnique<
      content::UtilityProcessMojoClient<chrome::mojom::SafeArchiveAnalyzer>>(
      l10n_util::GetStringUTF16(
          IDS_UTILITY_PROCESS_SAFE_BROWSING_ZIP_FILE_ANALYZER_NAME));
  utility_process_mojo_client_->set_error_callback(base::Bind(
      &SandboxedDMGAnalyzer::AnalyzeFileDone, this, ArchiveAnalyzerResults()));

  utility_process_mojo_client_->Start();

  utility_process_mojo_client_->service()->AnalyzeDmgFile(
      std::move(file),
      base::Bind(&SandboxedDMGAnalyzer::AnalyzeFileDone, this));
}

void SandboxedDMGAnalyzer::AnalyzeFileDone(
    const ArchiveAnalyzerResults& results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  utility_process_mojo_client_.reset();
  callback_.Run(results);
}

}  // namespace safe_browsing
