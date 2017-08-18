// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/chrome_select_file_dialog_factory.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/win/win_util.h"
#include "chrome/common/shell_handler_win.mojom.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/utility_process_mojo_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/win/open_file_name_win.h"
#include "ui/shell_dialogs/select_file_dialog_win.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace {

constexpr base::Feature kIsolateShellOperations{
    "IsolateShellOperations", base::FEATURE_DISABLED_BY_DEFAULT};

using UtilityProcessClient =
    content::UtilityProcessMojoClient<chrome::mojom::ShellHandler>;

std::unique_ptr<UtilityProcessClient> StartUtilityProcess() {
  auto utility_process_client = base::MakeUnique<UtilityProcessClient>(
      l10n_util::GetStringUTF16(IDS_UTILITY_PROCESS_FILE_DIALOG_NAME));

  // TODO(crbug.com/618459): should we change the mojo utility client
  // to allow an empty error callback?  Currently, the client DCHECKs
  // if no error callback is set when Start() is called.
  utility_process_client->set_error_callback(base::Bind(&base::DoNothing));

  utility_process_client->set_disable_sandbox();

  utility_process_client->Start();

  return utility_process_client;
}

}  // namespace

ChromeSelectFileDialogFactory::ChromeSelectFileDialogFactory() = default;

ChromeSelectFileDialogFactory::~ChromeSelectFileDialogFactory() = default;

ui::SelectFileDialog* ChromeSelectFileDialogFactory::Create(
    ui::SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  return ui::CreateWinSelectFileDialog(
      listener, std::move(policy),
      base::Bind(&ChromeSelectFileDialogFactory::BlockingGetOpenFileName),
      base::Bind(&ChromeSelectFileDialogFactory::BlockingGetSaveFileName));
}

// static
bool ChromeSelectFileDialogFactory::BlockingGetOpenFileName(OPENFILENAME* ofn) {
  if (!base::FeatureList::IsEnabled(kIsolateShellOperations))
    return ::GetOpenFileName(ofn) == TRUE;

  auto utility_process_client = StartUtilityProcess();

  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  std::vector<base::FilePath> files;
  base::FilePath directory;

  utility_process_client->service()->CallGetOpenFileName(
      base::win::HandleToUint32(ofn->hwndOwner),
      static_cast<uint32_t>(ofn->Flags & ~OFN_ENABLEHOOK),
      ui::win::OpenFileName::GetFilters(ofn),
      ofn->lpstrInitialDir ? base::FilePath(ofn->lpstrInitialDir)
                           : base::FilePath(),
      base::FilePath(ofn->lpstrFile), &directory, &files);

  if (files.empty())
    return false;

  ui::win::OpenFileName::SetResult(directory, files, ofn);
  return true;
}

// static
bool ChromeSelectFileDialogFactory::BlockingGetSaveFileName(OPENFILENAME* ofn) {
  if (!base::FeatureList::IsEnabled(kIsolateShellOperations))
    return ::GetSaveFileName(ofn) == TRUE;

  auto utility_process_client = StartUtilityProcess();

  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  uint32_t filter_index = 0;
  base::FilePath path;

  utility_process_client->service()->CallGetSaveFileName(
      base::win::HandleToUint32(ofn->hwndOwner),
      static_cast<uint32_t>(ofn->Flags & ~OFN_ENABLEHOOK),
      ui::win::OpenFileName::GetFilters(ofn), ofn->nFilterIndex,
      ofn->lpstrInitialDir ? base::FilePath(ofn->lpstrInitialDir)
                           : base::FilePath(),
      base::FilePath(ofn->lpstrFile),
      ofn->lpstrDefExt ? base::string16(ofn->lpstrDefExt) : base::string16(),
      &path, &filter_index);

  if (path.empty())
    return false;

  base::wcslcpy(ofn->lpstrFile, path.value().c_str(), ofn->nMaxFile);
  ofn->nFilterIndex = static_cast<DWORD>(filter_index);
  return true;
}
