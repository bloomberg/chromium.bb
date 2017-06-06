// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_file_dialog.h"

#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "net/base/filename_util.h"

namespace media_router {

MediaRouterFileDialog::MediaRouterFileDialog(
    MediaRouterFileDialogDelegate* delegate)
    : delegate_(delegate) {}

MediaRouterFileDialog::~MediaRouterFileDialog() = default;

GURL MediaRouterFileDialog::GetLastSelectedFileUrl() {
  if (!selected_file_.has_value())
    return GURL();

  return net::FilePathToFileURL(selected_file_->local_path);
}

base::string16 MediaRouterFileDialog::GetLastSelectedFileName() {
  if (!selected_file_.has_value())
    return base::string16();

  return selected_file_.value().file_path.BaseName().LossyDisplayName();
}

base::string16 MediaRouterFileDialog::GetDetailedErrorMessage() {
  return detailed_error_message_.value_or(base::string16());
}

void MediaRouterFileDialog::OpenFileDialog(Browser* browser) {
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(
                browser->tab_strip_model()->GetActiveWebContents()));

  const base::FilePath directory =
      browser->profile()->last_selected_directory();

  gfx::NativeWindow parent_window = browser->window()->GetNativeWindow();

  ui::SelectFileDialog::FileTypeInfo file_types;
  file_types.allowed_paths = ui::SelectFileDialog::FileTypeInfo::ANY_PATH;

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, base::string16(), directory,
      &file_types, 0, base::FilePath::StringType(), parent_window, nullptr);
}

void MediaRouterFileDialog::FileSelected(const base::FilePath& path,
                                         int index,
                                         void* params) {
  FileSelectedWithExtraInfo(ui::SelectedFileInfo(path, path), index, params);
}

void MediaRouterFileDialog::FileSelectedWithExtraInfo(
    const ui::SelectedFileInfo& file_info,
    int index,
    void* params) {
  // TODO(offenwanger): Validate file.
  selected_file_ = file_info;
  delegate_->FileDialogFileSelected(file_info);
}

void MediaRouterFileDialog::FileSelectionCanceled(void* params) {
  delegate_->FileDialogSelectionFailed(CANCELED);
}

}  // namespace media_router
