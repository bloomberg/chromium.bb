// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/select_file_dialog_extension.h"

#include "base/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

class SelectFileDialogExtensionTest : public testing::Test {
 public:
  static SelectFileDialogExtension* CreateDialog(
      SelectFileDialog::Listener* listener,
      int32 tab_id) {
    SelectFileDialogExtension* dialog = new SelectFileDialogExtension(listener);
    // Simulate the dialog opening.
    EXPECT_FALSE(SelectFileDialogExtension::PendingExists(tab_id));
    dialog->AddPending(tab_id);
    EXPECT_TRUE(SelectFileDialogExtension::PendingExists(tab_id));
    return dialog;
  }
};

// Client of a FileManagerDialog that deletes itself whenever the dialog
// is closed.
class SelfDeletingClient : public SelectFileDialog::Listener {
 public:
  explicit SelfDeletingClient(int32 tab_id) {
    dialog_ = SelectFileDialogExtensionTest::CreateDialog(this, tab_id);
  }

  virtual ~SelfDeletingClient() {
    if (dialog_.get())
      dialog_->ListenerDestroyed();
  }

  SelectFileDialogExtension* dialog() const { return dialog_.get(); }

  // SelectFileDialog::Listener implementation
  virtual void FileSelected(const FilePath& path,
                            int index, void* params) OVERRIDE {
    delete this;
  }

 private:
  scoped_refptr<SelectFileDialogExtension> dialog_;
};

TEST_F(SelectFileDialogExtensionTest, SelfDeleting) {
  const int32 kTabId = 123;
  SelfDeletingClient* client = new SelfDeletingClient(kTabId);
  // Ensure we don't crash or trip an Address Sanitizer warning about
  // use-after-free.
  SelectFileDialogExtension::OnFileSelected(kTabId, FilePath(), 0);
  // Simulate closing the dialog so the listener gets invoked.
  client->dialog()->ExtensionDialogClosing(NULL);
}
