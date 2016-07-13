// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/chrome_select_file_dialog_factory.h"

#include <Windows.h>
#include <commdlg.h>

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/win_util.h"
#include "chrome/common/shell_handler_win.mojom.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/utility_process_mojo_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/win/open_file_name_win.h"
#include "ui/shell_dialogs/select_file_dialog_win.h"

namespace {

constexpr base::Feature kIsolateShellOperations{
    "IsolateShellOperations", base::FEATURE_DISABLED_BY_DEFAULT};

// Implements GetOpenFileName() and GetSaveFileName() for
// CreateWinSelectFileDialog by delegating to a utility process.
class OpenFileNameClient {
 public:
  OpenFileNameClient();

  // Invokes ::GetOpenFileName() and stores the result into |directory| and
  // |filenames|. Returns false on failure.
  bool BlockingGetOpenFileName(OPENFILENAME* ofn);

  // Invokes ::GetSaveFileName() and stores the result into |path| and
  // |one_based_filter_index|. Returns false on failure.
  bool BlockingGetSaveFileName(OPENFILENAME* ofn);

 private:
  void StartClient();

  void InvokeGetOpenFileNameOnIOThread(OPENFILENAME* ofn);
  void InvokeGetSaveFileNameOnIOThread(OPENFILENAME* ofn);

  // Callbacks for Mojo invokation.
  void OnDidGetOpenFileNames(const base::FilePath& directory,
                             mojo::Array<base::FilePath> filenames);
  void OnDidGetSaveFileName(const base::FilePath& path,
                            uint32_t one_based_filter_index);

  void OnConnectionError();

  // Must only be accessed on the IO thread.
  std::unique_ptr<content::UtilityProcessMojoClient<mojom::ShellHandler>>
      client_;

  // This is used to block until the result is received.
  base::WaitableEvent result_received_event_;

  // Result variables for GetOpenFileName.
  base::FilePath directory_;
  std::vector<base::FilePath> filenames_;

  // Result variables for GetSaveFileName.
  base::FilePath path_;
  DWORD one_based_filter_index_ = 0;

  DISALLOW_COPY_AND_ASSIGN(OpenFileNameClient);
};

OpenFileNameClient::OpenFileNameClient()
    : result_received_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED) {}

bool OpenFileNameClient::BlockingGetOpenFileName(OPENFILENAME* ofn) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&OpenFileNameClient::InvokeGetOpenFileNameOnIOThread,
                 base::Unretained(this), ofn));

  result_received_event_.Wait();

  if (filenames_.empty())
    return false;

  ui::win::OpenFileName::SetResult(directory_, filenames_, ofn);
  return true;
}

bool OpenFileNameClient::BlockingGetSaveFileName(OPENFILENAME* ofn) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&OpenFileNameClient::InvokeGetSaveFileNameOnIOThread,
                 base::Unretained(this), ofn));

  result_received_event_.Wait();

  if (path_.empty())
    return false;

  base::wcslcpy(ofn->lpstrFile, path_.value().c_str(), ofn->nMaxFile);
  ofn->nFilterIndex = one_based_filter_index_;
  return true;
}

void OpenFileNameClient::StartClient() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  client_.reset(new content::UtilityProcessMojoClient<mojom::ShellHandler>(
      l10n_util::GetStringUTF16(IDS_UTILITY_PROCESS_FILE_DIALOG_NAME)));

  client_->set_disable_sandbox();
  client_->set_error_callback(base::Bind(&OpenFileNameClient::OnConnectionError,
                                         base::Unretained(this)));

  client_->Start();
}

void OpenFileNameClient::InvokeGetOpenFileNameOnIOThread(OPENFILENAME* ofn) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  StartClient();
  client_->service()->DoGetOpenFileName(
      base::win::HandleToUint32(ofn->hwndOwner),
      static_cast<uint32_t>(ofn->Flags), ui::win::OpenFileName::GetFilters(ofn),
      ofn->lpstrInitialDir ? base::FilePath(ofn->lpstrInitialDir)
                           : base::FilePath(),
      base::FilePath(ofn->lpstrFile),
      base::Bind(&OpenFileNameClient::OnDidGetOpenFileNames,
                 base::Unretained(this)));
}

void OpenFileNameClient::InvokeGetSaveFileNameOnIOThread(OPENFILENAME* ofn) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  StartClient();
  client_->service()->DoGetSaveFileName(
      base::win::HandleToUint32(ofn->hwndOwner),
      static_cast<uint32_t>(ofn->Flags), ui::win::OpenFileName::GetFilters(ofn),
      ofn->nFilterIndex,
      ofn->lpstrInitialDir ? base::FilePath(ofn->lpstrInitialDir)
                           : base::FilePath(),
      base::FilePath(ofn->lpstrFile),
      ofn->lpstrDefExt ? base::FilePath(ofn->lpstrDefExt) : base::FilePath(),
      base::Bind(&OpenFileNameClient::OnDidGetSaveFileName,
                 base::Unretained(this)));
}

void OpenFileNameClient::OnDidGetOpenFileNames(
    const base::FilePath& directory,
    mojo::Array<base::FilePath> filenames) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  client_.reset();
  directory_ = directory;
  filenames_ = filenames.storage();

  result_received_event_.Signal();
}

void OpenFileNameClient::OnDidGetSaveFileName(const base::FilePath& path,
                                              uint32_t one_based_filter_index) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  client_.reset();
  path_ = path;
  one_based_filter_index_ = one_based_filter_index;

  result_received_event_.Signal();
}

void OpenFileNameClient::OnConnectionError() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  client_.reset();
  result_received_event_.Signal();
}

bool GetOpenFileNameImpl(OPENFILENAME* ofn) {
  if (base::FeatureList::IsEnabled(kIsolateShellOperations))
    return OpenFileNameClient().BlockingGetOpenFileName(ofn);

  return ::GetOpenFileName(ofn) == TRUE;
}

bool GetSaveFileNameImpl(OPENFILENAME* ofn) {
  if (base::FeatureList::IsEnabled(kIsolateShellOperations))
    return OpenFileNameClient().BlockingGetSaveFileName(ofn);

  return ::GetSaveFileName(ofn) == TRUE;
}

}  // namespace

ChromeSelectFileDialogFactory::ChromeSelectFileDialogFactory() = default;

ChromeSelectFileDialogFactory::~ChromeSelectFileDialogFactory() = default;

ui::SelectFileDialog* ChromeSelectFileDialogFactory::Create(
    ui::SelectFileDialog::Listener* listener,
    ui::SelectFilePolicy* policy) {
  return ui::CreateWinSelectFileDialog(listener, policy,
                                       base::Bind(GetOpenFileNameImpl),
                                       base::Bind(GetSaveFileNameImpl));
}
