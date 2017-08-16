// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/sandboxed_zip_analyzer.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

namespace safe_browsing {

SandboxedZipAnalyzer::SandboxedZipAnalyzer(const base::FilePath& zip_file,
                                           const ResultCallback& callback)
    : file_path_(zip_file), callback_(callback) {
  DCHECK(callback);
}

void SandboxedZipAnalyzer::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BACKGROUND,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SandboxedZipAnalyzer::PrepareFileToAnalyze, this));
}

SandboxedZipAnalyzer::~SandboxedZipAnalyzer() = default;

void SandboxedZipAnalyzer::PrepareFileToAnalyze() {
  base::File file(file_path_, base::File::FLAG_OPEN | base::File::FLAG_READ);

  if (!file.IsValid()) {
    DLOG(ERROR) << "Could not open file: " << file_path_.value();
    ReportFileFailure();
    return;
  }

  base::FilePath temp_path;
  base::File temp_file;
  if (base::CreateTemporaryFile(&temp_path)) {
    temp_file.Initialize(
        temp_path, (base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                    base::File::FLAG_WRITE | base::File::FLAG_TEMPORARY |
                    base::File::FLAG_DELETE_ON_CLOSE));
  }

  if (!temp_file.IsValid()) {
    DLOG(ERROR) << "Could not open temp file: " << temp_path.value();
    ReportFileFailure();
    return;
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&SandboxedZipAnalyzer::AnalyzeFile, this,
                     base::Passed(&file), base::Passed(&temp_file)));
}

void SandboxedZipAnalyzer::ReportFileFailure() {
  DCHECK(!utility_process_mojo_client_);

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(callback_, ArchiveAnalyzerResults()));
}

void SandboxedZipAnalyzer::AnalyzeFile(base::File file, base::File temp_file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!utility_process_mojo_client_);

  utility_process_mojo_client_ = base::MakeUnique<
      content::UtilityProcessMojoClient<chrome::mojom::SafeArchiveAnalyzer>>(
      l10n_util::GetStringUTF16(
          IDS_UTILITY_PROCESS_SAFE_BROWSING_ZIP_FILE_ANALYZER_NAME));
  utility_process_mojo_client_->set_error_callback(base::Bind(
      &SandboxedZipAnalyzer::AnalyzeFileDone, this, ArchiveAnalyzerResults()));

  utility_process_mojo_client_->Start();

  utility_process_mojo_client_->service()->AnalyzeZipFile(
      std::move(file), std::move(temp_file),
      base::Bind(&SandboxedZipAnalyzer::AnalyzeFileDone, this));
}

void SandboxedZipAnalyzer::AnalyzeFileDone(
    const ArchiveAnalyzerResults& results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  utility_process_mojo_client_.reset();
  callback_.Run(results);
}

}  // namespace safe_browsing
