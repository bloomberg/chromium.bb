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
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "ipc/ipc_message_macros.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/win/open_file_name_win.h"
#include "ui/shell_dialogs/select_file_dialog_win.h"

namespace {

bool ShouldIsolateShellOperations() {
  return base::FieldTrialList::FindFullName("IsolateShellOperations") ==
         "Enabled";
}

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
  void OnProcessCrashed(int exit_code) override;
  void OnProcessLaunchFailed(int error_code) override;
  bool OnMessageReceived(const IPC::Message& message) override;

 protected:
  ~GetOpenFileNameClient() override;

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

void GetOpenFileNameClient::OnProcessLaunchFailed(int error_code) {
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
  utility_process_host->SetName(l10n_util::GetStringUTF16(
      IDS_UTILITY_PROCESS_FILE_DIALOG_NAME));
  utility_process_host->DisableSandbox();
  utility_process_host->Send(new ChromeUtilityMsg_GetOpenFileName(
      ofn->hwndOwner,
      ofn->Flags & ~OFN_ENABLEHOOK,  // We can't send a hook function over IPC.
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

// Implements GetOpenFileName for CreateWinSelectFileDialog by delegating to a
// utility process.
bool GetOpenFileNameImpl(
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
    OPENFILENAME* ofn) {
  if (ShouldIsolateShellOperations())
    return GetOpenFileNameInUtilityProcess(blocking_task_runner, ofn);

  return ::GetOpenFileName(ofn) == TRUE;
}

class GetSaveFileNameClient : public content::UtilityProcessHostClient {
 public:
  GetSaveFileNameClient();

  // Blocks until the GetSaveFileName result is received (including failure to
  // launch or a crash of the utility process).
  void WaitForCompletion();

  // Returns the selected path.
  const base::FilePath& path() const { return path_; }

  // Returns the index of the user-selected filter.
  int one_based_filter_index() const { return one_based_filter_index_; }

  // UtilityProcessHostClient implementation
  void OnProcessCrashed(int exit_code) override;
  void OnProcessLaunchFailed(int error_code) override;
  bool OnMessageReceived(const IPC::Message& message) override;

 protected:
  ~GetSaveFileNameClient() override;

 private:
  void OnResult(const base::FilePath& path, int one_based_filter_index);
  void OnFailure();

  base::FilePath path_;
  int one_based_filter_index_;
  base::WaitableEvent event_;

  DISALLOW_COPY_AND_ASSIGN(GetSaveFileNameClient);
};

GetSaveFileNameClient::GetSaveFileNameClient()
    : one_based_filter_index_(0), event_(true, false) {
}

void GetSaveFileNameClient::WaitForCompletion() {
  event_.Wait();
}

void GetSaveFileNameClient::OnProcessCrashed(int exit_code) {
  event_.Signal();
}

void GetSaveFileNameClient::OnProcessLaunchFailed(int error_code) {
  event_.Signal();
}

bool GetSaveFileNameClient::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GetSaveFileNameClient, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_GetSaveFileName_Failed,
                        OnFailure)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_GetSaveFileName_Result,
                        OnResult)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

GetSaveFileNameClient::~GetSaveFileNameClient() {}

void GetSaveFileNameClient::OnResult(const base::FilePath& path,
                                     int one_based_filter_index) {
  path_ = path;
  one_based_filter_index_ = one_based_filter_index;
  event_.Signal();
}

void GetSaveFileNameClient::OnFailure() {
  event_.Signal();
}

// Initiates IPC with a new utility process using |client|. Instructs the
// utility process to call GetSaveFileName with |ofn|. |current_task_runner|
// must be the currently executing task runner.
void DoInvokeGetSaveFileName(
    OPENFILENAME* ofn,
    scoped_refptr<GetSaveFileNameClient> client,
    const scoped_refptr<base::SequencedTaskRunner>& current_task_runner) {
  DCHECK(current_task_runner->RunsTasksOnCurrentThread());

  base::WeakPtr<content::UtilityProcessHost> utility_process_host(
      content::UtilityProcessHost::Create(client, current_task_runner)
      ->AsWeakPtr());
  utility_process_host->SetName(l10n_util::GetStringUTF16(
      IDS_UTILITY_PROCESS_FILE_DIALOG_NAME));
  utility_process_host->DisableSandbox();
  ChromeUtilityMsg_GetSaveFileName_Params params;
  params.owner = ofn->hwndOwner;
  // We can't pass the hook function over IPC.
  params.flags = ofn->Flags & ~OFN_ENABLEHOOK;
  params.filters = ui::win::OpenFileName::GetFilters(ofn);
  params.one_based_filter_index = ofn->nFilterIndex;
  params.suggested_filename = base::FilePath(ofn->lpstrFile);
  params.initial_directory = base::FilePath(
      ofn->lpstrInitialDir ? ofn->lpstrInitialDir : base::string16());
  params.default_extension =
      ofn->lpstrDefExt ? base::string16(ofn->lpstrDefExt) : base::string16();

  utility_process_host->Send(new ChromeUtilityMsg_GetSaveFileName(params));
}

// Invokes GetSaveFileName in a utility process. Blocks until the result is
// received. Uses |blocking_task_runner| for IPC.
bool GetSaveFileNameInUtilityProcess(
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
    OPENFILENAME* ofn) {
  scoped_refptr<GetSaveFileNameClient> client(new GetSaveFileNameClient);
  blocking_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&DoInvokeGetSaveFileName,
                 base::Unretained(ofn), client, blocking_task_runner));
  client->WaitForCompletion();

  if (client->path().empty())
    return false;

  base::wcslcpy(ofn->lpstrFile, client->path().value().c_str(), ofn->nMaxFile);
  ofn->nFilterIndex = client->one_based_filter_index();

  return true;
}

// Implements GetSaveFileName for CreateWinSelectFileDialog by delegating to a
// utility process.
bool GetSaveFileNameImpl(
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
    OPENFILENAME* ofn) {
  if (ShouldIsolateShellOperations())
    return GetSaveFileNameInUtilityProcess(blocking_task_runner, ofn);

  return ::GetSaveFileName(ofn) == TRUE;
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
      base::Bind(GetOpenFileNameImpl, blocking_task_runner_),
      base::Bind(GetSaveFileNameImpl, blocking_task_runner_));
}
