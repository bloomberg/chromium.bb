// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_patcher_operation_out_of_process.h"

#include <vector>

#include "base/bind.h"
#include "chrome/common/chrome_utility_messages.h"
#include "components/component_updater/component_updater_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff.h"
#include "ipc/ipc_message_macros.h"

namespace component_updater {

class PatchHost : public content::UtilityProcessHostClient {
 public:
  PatchHost(base::Callback<void(int result)> callback,
            scoped_refptr<base::SequencedTaskRunner> task_runner);

  void StartProcess(scoped_ptr<IPC::Message> message);

 private:
  virtual ~PatchHost();

  void OnPatchFinished(int result);

  // Overrides of content::UtilityProcessHostClient.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  virtual void OnProcessCrashed(int exit_code) OVERRIDE;

  base::Callback<void(int result)> callback_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

PatchHost::PatchHost(base::Callback<void(int result)> callback,
                     scoped_refptr<base::SequencedTaskRunner> task_runner)
    : callback_(callback), task_runner_(task_runner) {
}

PatchHost::~PatchHost() {
}

void PatchHost::StartProcess(scoped_ptr<IPC::Message> message) {
  // The DeltaUpdateOpPatchHost is not responsible for deleting the
  // UtilityProcessHost object.
  content::UtilityProcessHost* host = content::UtilityProcessHost::Create(
      this, base::MessageLoopProxy::current().get());
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
  task_runner_->PostTask(FROM_HERE, base::Bind(callback_, result));
  task_runner_ = NULL;
}

void PatchHost::OnProcessCrashed(int exit_code) {
  task_runner_->PostTask(FROM_HERE, base::Bind(callback_, -1));
  task_runner_ = NULL;
}

ChromeOutOfProcessPatcher::ChromeOutOfProcessPatcher() {
}

ChromeOutOfProcessPatcher::~ChromeOutOfProcessPatcher() {
}

void ChromeOutOfProcessPatcher::Patch(
    const std::string& operation,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::FilePath& input_abs_path,
    base::FilePath& patch_abs_path,
    base::FilePath& output_abs_path,
    base::Callback<void(int result)> callback) {
  host_ = new PatchHost(callback, task_runner);
  scoped_ptr<IPC::Message> patch_message;
  if (operation == kBsdiff) {
    patch_message.reset(new ChromeUtilityMsg_PatchFileBsdiff(
        input_abs_path, patch_abs_path, output_abs_path));
  } else if (operation == kCourgette) {
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
