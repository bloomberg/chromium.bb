// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_select_file_dialog_factory_win.h"

#include <Windows.h>
#include <commdlg.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/metro.h"
#include "chrome/common/chrome_utility_messages.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "ipc/ipc_message_macros.h"
#include "ui/base/win/open_file_name_win.h"
#include "ui/shell_dialogs/select_file_dialog_win.h"

namespace {

// Receives the GetOpenFileName result from the utility process.
class GetOpenFileNameClient : public content::UtilityProcessHostClient {
 public:
  GetOpenFileNameClient();

  // Blocks until the GetOpenFileName result is received (including failure to
  // launch or a crash of the utility process).
  void WaitForCompletion();

  // Returns the selected directory.
  const base::FilePath& directory() const { return directory_; }

  // Returns the list of selected filenames. Each should be interpreted as a
  // child of directory().
  const std::vector<base::FilePath>& filenames() const { return filenames_; }

  // UtilityProcessHostClient implementation
  virtual void OnProcessCrashed(int exit_code) OVERRIDE;
  virtual void OnProcessLaunchFailed() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 protected:
  virtual ~GetOpenFileNameClient();

 private:
  void OnResult(const base::FilePath& directory,
                const std::vector<base::FilePath>& filenames);
  void OnFailure();

  base::FilePath directory_;
  std::vector<base::FilePath> filenames_;
  base::WaitableEvent event_;

  DISALLOW_COPY_AND_ASSIGN(GetOpenFileNameClient);
};

GetOpenFileNameClient::GetOpenFileNameClient() : event_(true, false) {
}

void GetOpenFileNameClient::WaitForCompletion() {
  event_.Wait();
}

void GetOpenFileNameClient::OnProcessCrashed(int exit_code) {
  event_.Signal();
}

void GetOpenFileNameClient::OnProcessLaunchFailed() {
  event_.Signal();
}

bool GetOpenFileNameClient::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GetOpenFileNameClient, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_GetOpenFileName_Failed,
                        OnFailure)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_GetOpenFileName_Result,
                        OnResult)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

GetOpenFileNameClient::~GetOpenFileNameClient() {}

void GetOpenFileNameClient::OnResult(
    const base::FilePath& directory,
    const std::vector<base::FilePath>& filenames) {
  directory_ = directory;
  filenames_ = filenames;
  event_.Signal();
}

void GetOpenFileNameClient::OnFailure() {
  event_.Signal();
}

// Initiates IPC with a new utility process using |client|. Instructs the
// utility process to call GetOpenFileName with |ofn|. |current_task_runner|
// must be the currently executing task runner.
void DoInvokeGetOpenFileName(
    OPENFILENAME* ofn,
    scoped_refptr<GetOpenFileNameClient> client,
    const scoped_refptr<base::SequencedTaskRunner>& current_task_runner) {
  DCHECK(current_task_runner->RunsTasksOnCurrentThread());

  base::WeakPtr<content::UtilityProcessHost> utility_process_host(
      content::UtilityProcessHost::Create(client, current_task_runner)
      ->AsWeakPtr());
  utility_process_host->DisableSandbox();
  utility_process_host->Send(new ChromeUtilityMsg_GetOpenFileName(
      ofn->hwndOwner,
      ofn->Flags,
      ui::win::OpenFileName::GetFilters(ofn),
      base::FilePath(ofn->lpstrInitialDir ? ofn->lpstrInitialDir
                                          : base::string16()),
      base::FilePath(ofn->lpstrFile)));
}

// Invokes GetOpenFileName in a utility process. Blocks until the result is
// received. Uses |blocking_task_runner| for IPC.
bool GetOpenFileNameInUtilityProcess(
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
    OPENFILENAME* ofn) {
  scoped_refptr<GetOpenFileNameClient> client(new GetOpenFileNameClient);
  blocking_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&DoInvokeGetOpenFileName,
                 base::Unretained(ofn), client, blocking_task_runner));
  client->WaitForCompletion();

  if (!client->filenames().size())
    return false;

  ui::win::OpenFileName::SetResult(
      client->directory(), client->filenames(), ofn);
  return true;
}

// Implements GetOpenFileName for CreateWinSelectFileDialog by delegating either
// to Metro or a utility process.
bool GetOpenFileNameImpl(
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
    OPENFILENAME* ofn) {
  HMODULE metro_module = base::win::GetMetroModule();
  if (metro_module != NULL) {
    typedef BOOL (*MetroGetOpenFileName)(OPENFILENAME*);
    MetroGetOpenFileName metro_get_open_file_name =
        reinterpret_cast<MetroGetOpenFileName>(
            ::GetProcAddress(metro_module, "MetroGetOpenFileName"));
    if (metro_get_open_file_name == NULL) {
      NOTREACHED();
      return false;
    }

    return metro_get_open_file_name(ofn) == TRUE;
  }

  if (base::FieldTrialList::FindFullName("IsolateShellOperations") ==
      "Enabled") {
    return GetOpenFileNameInUtilityProcess(blocking_task_runner, ofn);
  }

  return ::GetOpenFileName(ofn) == TRUE;
}

}  // namespace

ChromeSelectFileDialogFactory::ChromeSelectFileDialogFactory(
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner)
    : blocking_task_runner_(blocking_task_runner) {
}

ChromeSelectFileDialogFactory::~ChromeSelectFileDialogFactory() {}

ui::SelectFileDialog* ChromeSelectFileDialogFactory::Create(
    ui::SelectFileDialog::Listener* listener,
    ui::SelectFilePolicy* policy) {
  return ui::CreateWinSelectFileDialog(
      listener,
      policy,
      base::Bind(GetOpenFileNameImpl, blocking_task_runner_));
}
