// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/zip_file_creator.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Creates the destination zip file only if it does not already exist.
base::File OpenFileHandleAsync(const base::FilePath& zip_path) {
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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, base::TaskTraits().MayBlock(),
      base::Bind(&OpenFileHandleAsync, dest_file_),
      base::Bind(&ZipFileCreator::CreateZipFile, this));
}

ZipFileCreator::~ZipFileCreator() = default;

void ZipFileCreator::CreateZipFile(base::File file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!utility_process_mojo_client_);

  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to create dest zip file " << dest_file_.value();
    ReportDone(false);
    return;
  }

  utility_process_mojo_client_ = base::MakeUnique<
      content::UtilityProcessMojoClient<chrome::mojom::ZipFileCreator>>(
      l10n_util::GetStringUTF16(IDS_UTILITY_PROCESS_ZIP_FILE_CREATOR_NAME));
  utility_process_mojo_client_->set_error_callback(
      base::Bind(&ZipFileCreator::ReportDone, this, false));

  utility_process_mojo_client_->set_exposed_directory(src_dir_);

  utility_process_mojo_client_->Start();

  utility_process_mojo_client_->service()->CreateZipFile(
      src_dir_, src_relative_paths_, std::move(file),
      base::Bind(&ZipFileCreator::ReportDone, this));
}

void ZipFileCreator::ReportDone(bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  utility_process_mojo_client_.reset();
  base::ResetAndReturn(&callback_).Run(success);
}

}  // namespace file_manager
