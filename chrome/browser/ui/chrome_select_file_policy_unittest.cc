// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chrome_select_file_policy.h"

#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if defined(USE_AURA)
// http://crbug.com/105200
#define MAYBE_ExpectAsynchronousListenerCall DISABLED_ExpectAsynchronousListenerCall
#else
#define MAYBE_ExpectAsynchronousListenerCall ExpectAsynchronousListenerCall
#endif

using content::BrowserThread;

namespace {

class FileSelectionUser : public ui::SelectFileDialog::Listener {
 public:
  FileSelectionUser()
      : file_selection_initialisation_in_progress(false) {
  }

  virtual ~FileSelectionUser() {
    if (select_file_dialog_.get())
      select_file_dialog_->ListenerDestroyed();
  }

  void StartFileSelection() {
    CHECK(!select_file_dialog_.get());
    select_file_dialog_ = ui::SelectFileDialog::Create(
        this, new ChromeSelectFilePolicy(NULL));

    const base::FilePath file_path;
    const base::string16 title = base::string16();

    file_selection_initialisation_in_progress = true;
    select_file_dialog_->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                    title,
                                    file_path,
                                    NULL,
                                    0,
                                    base::FilePath::StringType(),
                                    NULL,
                                    NULL);
    file_selection_initialisation_in_progress = false;
  }

  // ui::SelectFileDialog::Listener implementation.
  virtual void FileSelected(const base::FilePath& path,
                            int index, void* params) OVERRIDE {
    ASSERT_FALSE(file_selection_initialisation_in_progress);
  }
  virtual void MultiFilesSelected(
      const std::vector<base::FilePath>& files,
      void* params) OVERRIDE {
    ASSERT_FALSE(file_selection_initialisation_in_progress);
  }
  virtual void FileSelectionCanceled(void* params) OVERRIDE {
    ASSERT_FALSE(file_selection_initialisation_in_progress);
  }

 private:
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  bool file_selection_initialisation_in_progress;
};

}  // namespace

typedef testing::Test ChromeSelectFilePolicyTest;

// Tests if SelectFileDialog::SelectFile returns asynchronously with
// file-selection dialogs disabled by policy.
TEST_F(ChromeSelectFilePolicyTest, MAYBE_ExpectAsynchronousListenerCall) {
  base::MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);

  ScopedTestingLocalState local_state(
      TestingBrowserProcess::GetGlobal());

  scoped_ptr<FileSelectionUser> file_selection_user(new FileSelectionUser());

  // Disallow file-selection dialogs.
  local_state.Get()->SetManagedPref(prefs::kAllowFileSelectionDialogs,
                                    new base::FundamentalValue(false));

  file_selection_user->StartFileSelection();
}
