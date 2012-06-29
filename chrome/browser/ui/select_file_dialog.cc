// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/select_file_dialog.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/tab_contents/simple_alert_infobar_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "ui/base/dialogs/selected_file_info.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebContents;

SelectFileDialog::FileTypeInfo::FileTypeInfo() : include_all_files(false) {}

SelectFileDialog::FileTypeInfo::~FileTypeInfo() {}

void SelectFileDialog::Listener::FileSelectedWithExtraInfo(
    const ui::SelectedFileInfo& file,
    int index,
    void* params) {
  FileSelected(file.path, index, params);
}

void SelectFileDialog::Listener::MultiFilesSelectedWithExtraInfo(
    const std::vector<ui::SelectedFileInfo>& files,
    void* params) {
  std::vector<FilePath> file_paths;
  for (size_t i = 0; i < files.size(); ++i)
    file_paths.push_back(files[i].path);

  MultiFilesSelected(file_paths, params);
}

SelectFileDialog::SelectFileDialog(Listener* listener)
    : listener_(listener) {
  DCHECK(listener_);
}

SelectFileDialog::~SelectFileDialog() {}

bool SelectFileDialog::CanOpenSelectFileDialog() {
  DCHECK(g_browser_process);

  // local_state() can return NULL for tests.
  if (!g_browser_process->local_state())
    return false;

  return !g_browser_process->local_state()->FindPreference(
             prefs::kAllowFileSelectionDialogs) ||
         g_browser_process->local_state()->GetBoolean(
             prefs::kAllowFileSelectionDialogs);
}

void SelectFileDialog::SelectFile(Type type,
                                  const string16& title,
                                  const FilePath& default_path,
                                  const FileTypeInfo* file_types,
                                  int file_type_index,
                                  const FilePath::StringType& default_extension,
                                  WebContents* source_contents,
                                  gfx::NativeWindow owning_window,
                                  void* params) {
  DCHECK(listener_);

  if (!CanOpenSelectFileDialog()) {
    // Show the InfoBar saying that file-selection dialogs are disabled.
    if (source_contents) {
      TabContents* tab_contents = TabContents::FromWebContents(source_contents);
      InfoBarTabHelper* infobar_helper = tab_contents->infobar_tab_helper();
      infobar_helper->AddInfoBar(new SimpleAlertInfoBarDelegate(
          infobar_helper,
          NULL,
          l10n_util::GetStringUTF16(IDS_FILE_SELECTION_DIALOG_INFOBAR),
          true));
    } else {
      LOG(WARNING) << "File-selection dialogs are disabled but no WebContents "
                   << "is given to display the InfoBar.";
    }

    // Inform the listener that no file was selected.
    // Post a task rather than calling FileSelectionCanceled directly to ensure
    // that the listener is called asynchronously.
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&SelectFileDialog::CancelFileSelection, this,
                              params));
    return;
  }
  // Call the platform specific implementation of the file selection dialog.
  SelectFileImpl(type, title, default_path, file_types, file_type_index,
                 default_extension, owning_window, params);
}

bool SelectFileDialog::HasMultipleFileTypeChoices() {
  return HasMultipleFileTypeChoicesImpl();
}

void SelectFileDialog::CancelFileSelection(void* params) {
  if (listener_)
    listener_->FileSelectionCanceled(params);
}
