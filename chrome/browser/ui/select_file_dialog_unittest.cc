// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string16.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/select_file_dialog.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_AURA) && defined(OS_WIN)
// http://crbug.com/105200
#define MAYBE_ExpectAsynchronousListenerCall DISABLED_ExpectAsynchronousListenerCall
#else
#define MAYBE_ExpectAsynchronousListenerCall ExpectAsynchronousListenerCall
#endif

using content::BrowserThread;

class FileSelectionUser : public SelectFileDialog::Listener {
 public:
  FileSelectionUser()
      : file_selection_initialisation_in_progress(false) {
  }

  ~FileSelectionUser() {
    if (select_file_dialog_.get())
      select_file_dialog_->ListenerDestroyed();
  }

  void StartFileSelection() {
    CHECK(!select_file_dialog_.get());
    select_file_dialog_ = SelectFileDialog::Create(this);

    const FilePath file_path;
    const string16 title=string16();

    file_selection_initialisation_in_progress = true;
    select_file_dialog_->SelectFile(SelectFileDialog::SELECT_OPEN_FILE,
                                    title,
                                    file_path,
                                    NULL,
                                    0,
                                    FILE_PATH_LITERAL(""),
                                    NULL,
                                    NULL,
                                    NULL);
    file_selection_initialisation_in_progress = false;
  }

  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path,
                            int index, void* params){
    ASSERT_FALSE(file_selection_initialisation_in_progress);
  }
  virtual void MultiFilesSelected(
      const std::vector<FilePath>& files,
      void* params) {
    ASSERT_FALSE(file_selection_initialisation_in_progress);
  }
  virtual void FileSelectionCanceled(void* params) {
    ASSERT_FALSE(file_selection_initialisation_in_progress);
  }

 private:
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  bool file_selection_initialisation_in_progress;
};

typedef testing::Test FileSelectionDialogTest;

// Tests if SelectFileDialog::SelectFile returns asynchronously with
// file-selection dialogs disabled by policy.
TEST_F(FileSelectionDialogTest, MAYBE_ExpectAsynchronousListenerCall) {
  MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);

  ScopedTestingLocalState local_state(
      static_cast<TestingBrowserProcess*>(g_browser_process));

  scoped_ptr<FileSelectionUser> file_selection_user(new FileSelectionUser());

  // Disallow file-selection dialogs.
  local_state.Get()->SetManagedPref(
      prefs::kAllowFileSelectionDialogs,
      Value::CreateBooleanValue(false));

  file_selection_user->StartFileSelection();
}
