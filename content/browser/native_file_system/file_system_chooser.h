// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_FILE_SYSTEM_FILE_SYSTEM_CHOOSER_H_
#define CONTENT_BROWSER_NATIVE_FILE_SYSTEM_FILE_SYSTEM_CHOOSER_H_

#include "base/files/file.h"
#include "base/task_runner.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_manager.mojom.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace content {

// This is a ui::SelectFileDialog::Listener implementation that grants access to
// the selected files to a specific renderer process on success, and then calls
// a callback on a specific task runner. Furthermore the listener will delete
// itself when any of its listener methods are called.
// All of this class has to be called on the UI thread.
class CONTENT_EXPORT FileSystemChooser : public ui::SelectFileDialog::Listener {
 public:
  struct IsolatedFileSystemEntry {
    std::string file_system_id;
    // Path to the file as it would appear in the file system URL for this
    // entry. I.e. this includes the file_system_id and relative path to the
    // file within that file system.
    base::FilePath isolated_file_path;
  };

  using ResultCallback =
      base::OnceCallback<void(blink::mojom::NativeFileSystemErrorPtr,
                              std::vector<IsolatedFileSystemEntry>)>;

  static void CreateAndShow(
      int render_process_id,
      int frame_id,
      blink::mojom::ChooseFileSystemEntryType type,
      std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr> accepts,
      bool include_accepts_all,
      ResultCallback callback,
      scoped_refptr<base::TaskRunner> callback_runner);

  FileSystemChooser(blink::mojom::ChooseFileSystemEntryType type,
                    ResultCallback callback,
                    scoped_refptr<base::TaskRunner> callback_runner);

 private:
  ~FileSystemChooser() override;

  // ui::SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void MultiFilesSelected(const std::vector<base::FilePath>& files,
                          void* params) override;
  void FileSelectionCanceled(void* params) override;

  ResultCallback callback_;
  scoped_refptr<base::TaskRunner> callback_runner_;
  blink::mojom::ChooseFileSystemEntryType type_;

  scoped_refptr<ui::SelectFileDialog> dialog_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_FILE_SYSTEM_FILE_SYSTEM_CHOOSER_H_
