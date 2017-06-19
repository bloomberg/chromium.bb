// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_patcher_operation_out_of_process.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "chrome/grit/generated_resources.h"
#include "components/update_client/component_patcher_operation.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

namespace component_updater {

ChromeOutOfProcessPatcher::ChromeOutOfProcessPatcher() = default;

ChromeOutOfProcessPatcher::~ChromeOutOfProcessPatcher() = default;

void ChromeOutOfProcessPatcher::Patch(
    const std::string& operation,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::FilePath& input_path,
    const base::FilePath& patch_path,
    const base::FilePath& output_path,
    const base::Callback<void(int result)>& callback) {
  DCHECK(task_runner);
  DCHECK(!callback.is_null());

  task_runner_ = task_runner;
  callback_ = callback;

  base::File input_file(input_path,
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::File patch_file(patch_path,
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::File output_file(output_path, base::File::FLAG_CREATE |
                                          base::File::FLAG_WRITE |
                                          base::File::FLAG_EXCLUSIVE_WRITE);

  if (!input_file.IsValid() || !patch_file.IsValid() ||
      !output_file.IsValid()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(callback_, -1));
    return;
  }

  DCHECK(!utility_process_mojo_client_);

  utility_process_mojo_client_ = base::MakeUnique<
      content::UtilityProcessMojoClient<chrome::mojom::FilePatcher>>(
      l10n_util::GetStringUTF16(IDS_UTILITY_PROCESS_COMPONENT_PATCHER_NAME));
  utility_process_mojo_client_->set_error_callback(
      base::Bind(&ChromeOutOfProcessPatcher::PatchDone, this, -1));

  utility_process_mojo_client_->Start();  // Start the utility process.

  if (operation == update_client::kBsdiff) {
    utility_process_mojo_client_->service()->PatchFileBsdiff(
        std::move(input_file), std::move(patch_file), std::move(output_file),
        base::Bind(&ChromeOutOfProcessPatcher::PatchDone, this));
  } else if (operation == update_client::kCourgette) {
    utility_process_mojo_client_->service()->PatchFileCourgette(
        std::move(input_file), std::move(patch_file), std::move(output_file),
        base::Bind(&ChromeOutOfProcessPatcher::PatchDone, this));
  } else {
    NOTREACHED();
  }
}

void ChromeOutOfProcessPatcher::PatchDone(int result) {
  utility_process_mojo_client_.reset();  // Terminate the utility process.
  task_runner_->PostTask(FROM_HERE, base::Bind(callback_, result));
}

}  // namespace component_updater
