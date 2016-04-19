// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_patcher_operation_out_of_process.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/grit/generated_resources.h"
#include "components/component_updater/component_updater_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff.h"
#include "ipc/ipc_message_macros.h"
#include "ui/base/l10n/l10n_util.h"

namespace component_updater {

class PatchHost : public content::UtilityProcessHostClient {
 public:
  PatchHost(base::Callback<void(int result)> callback,
            scoped_refptr<base::SequencedTaskRunner> task_runner);

  void StartProcess(std::unique_ptr<IPC::Message> message);

 private:
  ~PatchHost() override;

  void OnPatchFinished(int result);

  // Overrides of content::UtilityProcessHostClient.
  bool OnMessageReceived(const IPC::Message& message) override;

  void OnProcessCrashed(int exit_code) override;

  base::Callback<void(int result)> callback_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

PatchHost::PatchHost(base::Callback<void(int result)> callback,
                     scoped_refptr<base::SequencedTaskRunner> task_runner)
    : callback_(callback), task_runner_(task_runner) {
}

PatchHost::~PatchHost() {
}

void PatchHost::StartProcess(std::unique_ptr<IPC::Message> message) {
  // The DeltaUpdateOpPatchHost is not responsible for deleting the
  // UtilityProcessHost object.
  content::UtilityProcessHost* host = content::UtilityProcessHost::Create(
      this, base::ThreadTaskRunnerHandle::Get().get());
  host->SetName(l10n_util::GetStringUTF16(
      IDS_UTILITY_PROCESS_COMPONENT_PATCHER_NAME));
  host->DisableSandbox();
  host->Send(message.release());
}

bool PatchHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PatchHost, message)
  IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_PatchFile_Finished, OnPatchFinished)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PatchHost::OnPatchFinished(int result) {
  if (task_runner_.get()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(callback_, result));
    task_runner_ = NULL;
  }
}

void PatchHost::OnProcessCrashed(int exit_code) {
  if (task_runner_.get()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(callback_, -1));
    task_runner_ = NULL;
  }
}

ChromeOutOfProcessPatcher::ChromeOutOfProcessPatcher() {
}

ChromeOutOfProcessPatcher::~ChromeOutOfProcessPatcher() {
}

void ChromeOutOfProcessPatcher::Patch(
    const std::string& operation,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::FilePath& input_abs_path,
    const base::FilePath& patch_abs_path,
    const base::FilePath& output_abs_path,
    base::Callback<void(int result)> callback) {
  host_ = new PatchHost(callback, task_runner);
  std::unique_ptr<IPC::Message> patch_message;
  if (operation == update_client::kBsdiff) {
    patch_message.reset(new ChromeUtilityMsg_PatchFileBsdiff(
        input_abs_path, patch_abs_path, output_abs_path));
  } else if (operation == update_client::kCourgette) {
    patch_message.reset(new ChromeUtilityMsg_PatchFileCourgette(
        input_abs_path, patch_abs_path, output_abs_path));
  } else {
    NOTREACHED();
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &PatchHost::StartProcess, host_, base::Passed(&patch_message)));
}

}  // namespace component_updater
