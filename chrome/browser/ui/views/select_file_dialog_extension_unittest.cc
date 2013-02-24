// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/select_file_dialog_extension.h"

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace {

const int32 kDefaultTabId = 0;

}  // namespace

// Must be a class so it can be a friend of SelectFileDialogExtension.
class SelectFileDialogExtensionTest : public testing::Test {
 public:
  SelectFileDialogExtensionTest() {}
  virtual ~SelectFileDialogExtensionTest() {}

  static SelectFileDialogExtension* CreateDialog(
      ui::SelectFileDialog::Listener* listener) {
    SelectFileDialogExtension* dialog = new SelectFileDialogExtension(listener,
                                                                      NULL);
    // Simulate the dialog opening.
    EXPECT_FALSE(SelectFileDialogExtension::PendingExists(kDefaultTabId));
    dialog->AddPending(kDefaultTabId);
    EXPECT_TRUE(SelectFileDialogExtension::PendingExists(kDefaultTabId));
    return dialog;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SelectFileDialogExtensionTest);
};

// Test listener for a SelectFileDialog.
class TestListener : public ui::SelectFileDialog::Listener {
 public:
  TestListener() : selected_(false), file_index_(-1) {}
  virtual ~TestListener() {}

  bool selected() const { return selected_; }
  int file_index() const { return file_index_; }

  // ui::SelectFileDialog::Listener implementation
  virtual void FileSelected(const base::FilePath& path,
                            int index,
                            void* params) OVERRIDE {
    selected_ = true;
    file_index_ = index;
  }

 private:
  bool selected_;
  int file_index_;

  DISALLOW_COPY_AND_ASSIGN(TestListener);
};

// Client of a SelectFileDialog that deletes itself whenever the dialog
// is closed. This is a common pattern in UI code.
class SelfDeletingClient : public ui::SelectFileDialog::Listener {
 public:
  SelfDeletingClient() {
    dialog_ = SelectFileDialogExtensionTest::CreateDialog(this);
  }

  virtual ~SelfDeletingClient() {
    if (dialog_.get())
      dialog_->ListenerDestroyed();
  }

  SelectFileDialogExtension* dialog() const { return dialog_.get(); }

  // ui::SelectFileDialog::Listener implementation
  virtual void FileSelected(const base::FilePath& path,
                            int index,
                            void* params) OVERRIDE {
    delete this;
  }

 private:
  scoped_refptr<SelectFileDialogExtension> dialog_;
};

TEST_F(SelectFileDialogExtensionTest, FileSelected) {
  const int kFileIndex = 5;
  scoped_ptr<TestListener> listener(new TestListener);
  scoped_refptr<SelectFileDialogExtension> dialog =
      CreateDialog(listener.get());
  // Simulate selecting a file.
  ui::SelectedFileInfo info;
  SelectFileDialogExtension::OnFileSelected(kDefaultTabId, info, kFileIndex);
  // Simulate closing the dialog so the listener gets invoked.
  dialog->ExtensionDialogClosing(NULL);
  EXPECT_TRUE(listener->selected());
  EXPECT_EQ(kFileIndex, listener->file_index());
}

TEST_F(SelectFileDialogExtensionTest, FileSelectionCanceled) {
  scoped_ptr<TestListener> listener(new TestListener);
  scoped_refptr<SelectFileDialogExtension> dialog =
      CreateDialog(listener.get());
  // Simulate cancelling the dialog.
  SelectFileDialogExtension::OnFileSelectionCanceled(kDefaultTabId);
  // Simulate closing the dialog so the listener gets invoked.
  dialog->ExtensionDialogClosing(NULL);
  EXPECT_FALSE(listener->selected());
  EXPECT_EQ(-1, listener->file_index());
}

TEST_F(SelectFileDialogExtensionTest, SelfDeleting) {
  SelfDeletingClient* client = new SelfDeletingClient();
  // Ensure we don't crash or trip an Address Sanitizer warning about
  // use-after-free.
  ui::SelectedFileInfo file_info;
  SelectFileDialogExtension::OnFileSelected(kDefaultTabId, file_info, 0);
  // Simulate closing the dialog so the listener gets invoked.
  client->dialog()->ExtensionDialogClosing(NULL);
}
