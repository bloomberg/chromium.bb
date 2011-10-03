// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_manager_dialog.h"

#include "base/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

class FileManagerDialogTest : public testing::Test {
 public:
  static FileManagerDialog* CreateDialog(SelectFileDialog::Listener* listener,
                                         int32 tab_id) {
    FileManagerDialog* dialog = new FileManagerDialog(listener);
    // Simulate the dialog opening.
    dialog->AddPending(tab_id);
    return dialog;
  }
};

// Client of a FileManagerDialog that deletes itself whenever the dialog
// is closed.
class SelfDeletingClient : public SelectFileDialog::Listener {
 public:
  explicit SelfDeletingClient(int32 tab_id) {
    dialog_ = FileManagerDialogTest::CreateDialog(this, tab_id);
  }

  virtual ~SelfDeletingClient() {
    if (dialog_.get())
      dialog_->ListenerDestroyed();
  }

  // SelectFileDialog::Listener implementation
  virtual void FileSelected(const FilePath& path,
                            int index, void* params) OVERRIDE {
    delete this;
  }

 private:
  scoped_refptr<FileManagerDialog> dialog_;
};

TEST_F(FileManagerDialogTest, FileManagerDialogMemory) {
  const int32 kTabId = 123;
  // Registers itself with an internal map, so we don't need the pointer,
  // and it would be unused anyway.
  new SelfDeletingClient(kTabId);
  // Ensure we don't crash or trip an Address Sanitizer warning about
  // use-after-free.
  FileManagerDialog::OnFileSelected(kTabId, FilePath(), 0);
}
