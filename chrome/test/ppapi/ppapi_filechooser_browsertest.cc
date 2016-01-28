// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/ppapi/ppapi_test.h"
#include "ppapi/shared_impl/test_utils.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/selected_file_info.h"

#if defined(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/safe_browsing_db/test_database_manager.h"
#endif

using safe_browsing::SafeBrowsingService;

namespace {

class TestSelectFileDialogFactory final : public ui::SelectFileDialogFactory {
 public:
  using SelectedFileInfoList = std::vector<ui::SelectedFileInfo>;

  enum Mode {
    RESPOND_WITH_FILE_LIST,
    CANCEL,
    REPLACE_BASENAME,
    NOT_REACHED,
  };

  TestSelectFileDialogFactory(Mode mode,
                              const SelectedFileInfoList& selected_file_info)
      : selected_file_info_(selected_file_info), mode_(mode) {
    // Only safe because this class is 'final'
    ui::SelectFileDialog::SetFactory(this);
  }

  // SelectFileDialogFactory
  ui::SelectFileDialog* Create(ui::SelectFileDialog::Listener* listener,
                               ui::SelectFilePolicy* policy) override {
    return new SelectFileDialog(listener, policy, selected_file_info_, mode_);
  }

 private:
  class SelectFileDialog : public ui::SelectFileDialog {
   public:
    SelectFileDialog(Listener* listener,
                     ui::SelectFilePolicy* policy,
                     const SelectedFileInfoList& selected_file_info,
                     Mode mode)
        : ui::SelectFileDialog(listener, policy),
          selected_file_info_(selected_file_info),
          mode_(mode) {}

   protected:
    // ui::SelectFileDialog
    void SelectFileImpl(Type type,
                        const base::string16& title,
                        const base::FilePath& default_path,
                        const FileTypeInfo* file_types,
                        int file_type_index,
                        const base::FilePath::StringType& default_extension,
                        gfx::NativeWindow owning_window,
                        void* params) override {
      switch (mode_) {
        case RESPOND_WITH_FILE_LIST:
          break;

        case CANCEL:
          EXPECT_EQ(0u, selected_file_info_.size());
          break;

        case REPLACE_BASENAME:
          EXPECT_EQ(1u, selected_file_info_.size());
          for (auto& selected_file : selected_file_info_) {
            selected_file =
                ui::SelectedFileInfo(selected_file.file_path.DirName().Append(
                                         default_path.BaseName()),
                                     selected_file.local_path.DirName().Append(
                                         default_path.BaseName()));
          }
          break;

        case NOT_REACHED:
          NOTREACHED();
          break;
      }

      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&SelectFileDialog::RespondToFileSelectionRequest, this,
                     params));
    }
    bool HasMultipleFileTypeChoicesImpl() override { return false; }

    // BaseShellDialog
    bool IsRunning(gfx::NativeWindow owning_window) const override {
      return false;
    }
    void ListenerDestroyed() override {}

   private:
    void RespondToFileSelectionRequest(void* params) {
      if (selected_file_info_.size() == 0)
        listener_->FileSelectionCanceled(params);
      else if (selected_file_info_.size() == 1)
        listener_->FileSelectedWithExtraInfo(selected_file_info_.front(), 0,
                                             params);
      else
        listener_->MultiFilesSelectedWithExtraInfo(selected_file_info_, params);
    }

    SelectedFileInfoList selected_file_info_;
    Mode mode_;
  };

  std::vector<ui::SelectedFileInfo> selected_file_info_;
  Mode mode_;
};

class FakeDatabaseManager
    : public safe_browsing::TestSafeBrowsingDatabaseManager {
 public:
  bool IsSupported() const override { return true; }
  bool MatchDownloadWhitelistUrl(const GURL& url) override {
    // This matches the behavior in RunTestViaHTTP().
    return url.SchemeIsHTTPOrHTTPS() && url.has_path() &&
           url.path().find("/test_case.html") == 0;
  }

 protected:
  ~FakeDatabaseManager() override {}
};

class TestSafeBrowsingService : public SafeBrowsingService {
 public:
  safe_browsing::SafeBrowsingDatabaseManager* CreateDatabaseManager() override {
    return new FakeDatabaseManager();
  }

 protected:
  ~TestSafeBrowsingService() override {}

  safe_browsing::SafeBrowsingProtocolManagerDelegate*
      GetProtocolManagerDelegate() override {
    // Our FakeDatabaseManager doesn't implement this delegate.
    return NULL;
  }
};

class TestSafeBrowsingServiceFactory
    : public safe_browsing::SafeBrowsingServiceFactory {
 public:
  SafeBrowsingService* CreateSafeBrowsingService() override {
    SafeBrowsingService* service = new TestSafeBrowsingService();
    return service;
  }
};

class PPAPIFileChooserTest : public OutOfProcessPPAPITest {};

class PPAPIFileChooserTestWithSBService : public PPAPIFileChooserTest {
 public:
  void SetUp() override {
    SafeBrowsingService::RegisterFactory(&safe_browsing_service_factory_);
    PPAPIFileChooserTest::SetUp();
  }
  void TearDown() override {
    PPAPIFileChooserTest::TearDown();
    SafeBrowsingService::RegisterFactory(nullptr);
  }

 private:
  TestSafeBrowsingServiceFactory safe_browsing_service_factory_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(PPAPIFileChooserTest, FileChooser_Open_Success) {
  const char kContents[] = "Hello from browser";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath existing_filename = temp_dir.path().AppendASCII("foo");
  ASSERT_EQ(
      static_cast<int>(sizeof(kContents) - 1),
      base::WriteFile(existing_filename, kContents, sizeof(kContents) - 1));

  TestSelectFileDialogFactory::SelectedFileInfoList file_info_list;
  file_info_list.push_back(
      ui::SelectedFileInfo(existing_filename, existing_filename));
  TestSelectFileDialogFactory test_dialog_factory(
      TestSelectFileDialogFactory::RESPOND_WITH_FILE_LIST, file_info_list);
  RunTestViaHTTP("FileChooser_OpenSimple");
}

IN_PROC_BROWSER_TEST_F(PPAPIFileChooserTest, FileChooser_Open_Cancel) {
  TestSelectFileDialogFactory test_dialog_factory(
      TestSelectFileDialogFactory::CANCEL,
      TestSelectFileDialogFactory::SelectedFileInfoList());
  RunTestViaHTTP("FileChooser_OpenCancel");
}

IN_PROC_BROWSER_TEST_F(PPAPIFileChooserTest, FileChooser_SaveAs_Success) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath suggested_filename = temp_dir.path().AppendASCII("foo");

  TestSelectFileDialogFactory::SelectedFileInfoList file_info_list;
  file_info_list.push_back(
      ui::SelectedFileInfo(suggested_filename, suggested_filename));
  TestSelectFileDialogFactory test_dialog_factory(
      TestSelectFileDialogFactory::RESPOND_WITH_FILE_LIST, file_info_list);

  RunTestViaHTTP("FileChooser_SaveAsSafeDefaultName");
  ASSERT_TRUE(base::PathExists(suggested_filename));
}

IN_PROC_BROWSER_TEST_F(PPAPIFileChooserTest,
                       FileChooser_SaveAs_SafeDefaultName) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath suggested_filename = temp_dir.path().AppendASCII("foo");

  TestSelectFileDialogFactory::SelectedFileInfoList file_info_list;
  file_info_list.push_back(
      ui::SelectedFileInfo(suggested_filename, suggested_filename));
  TestSelectFileDialogFactory test_dialog_factory(
      TestSelectFileDialogFactory::REPLACE_BASENAME, file_info_list);

  RunTestViaHTTP("FileChooser_SaveAsSafeDefaultName");
  base::FilePath actual_filename = temp_dir.path().AppendASCII("innocuous.txt");

  ASSERT_TRUE(base::PathExists(actual_filename));
  std::string file_contents;
  ASSERT_TRUE(base::ReadFileToString(actual_filename, &file_contents, 100));
  EXPECT_EQ("Hello from PPAPI", file_contents);
}

IN_PROC_BROWSER_TEST_F(PPAPIFileChooserTest,
                       FileChooser_SaveAs_UnsafeDefaultName) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath suggested_filename = temp_dir.path().AppendASCII("foo");

  TestSelectFileDialogFactory::SelectedFileInfoList file_info_list;
  file_info_list.push_back(
      ui::SelectedFileInfo(suggested_filename, suggested_filename));
  TestSelectFileDialogFactory test_dialog_factory(
      TestSelectFileDialogFactory::REPLACE_BASENAME, file_info_list);

  RunTestViaHTTP("FileChooser_SaveAsUnsafeDefaultName");
  base::FilePath actual_filename = temp_dir.path().AppendASCII("unsafe.txt-");

  ASSERT_TRUE(base::PathExists(actual_filename));
  std::string file_contents;
  ASSERT_TRUE(base::ReadFileToString(actual_filename, &file_contents, 100));
  EXPECT_EQ("Hello from PPAPI", file_contents);
}

IN_PROC_BROWSER_TEST_F(PPAPIFileChooserTest, FileChooser_SaveAs_Cancel) {
  TestSelectFileDialogFactory test_dialog_factory(
      TestSelectFileDialogFactory::CANCEL,
      TestSelectFileDialogFactory::SelectedFileInfoList());
  RunTestViaHTTP("FileChooser_SaveAsCancel");
}

#if defined(FULL_SAFE_BROWSING)
// These tests only make sense when SafeBrowsing is enabled. They verify
// that files written via the FileChooser_Trusted API are properly passed
// through Safe Browsing.

IN_PROC_BROWSER_TEST_F(PPAPIFileChooserTest,
                       FileChooser_SaveAs_DangerousExecutable_Allowed) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowUncheckedDangerousDownloads);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath suggested_filename = temp_dir.path().AppendASCII("foo");

  TestSelectFileDialogFactory::SelectedFileInfoList file_info_list;
  file_info_list.push_back(
      ui::SelectedFileInfo(suggested_filename, suggested_filename));
  TestSelectFileDialogFactory test_dialog_factory(
      TestSelectFileDialogFactory::REPLACE_BASENAME, file_info_list);

  RunTestViaHTTP("FileChooser_SaveAsDangerousExecutableAllowed");
  base::FilePath actual_filename = temp_dir.path().AppendASCII("dangerous.exe");

  ASSERT_TRUE(base::PathExists(actual_filename));
  std::string file_contents;
  ASSERT_TRUE(base::ReadFileToString(actual_filename, &file_contents, 100));
  EXPECT_EQ("Hello from PPAPI", file_contents);
}

IN_PROC_BROWSER_TEST_F(PPAPIFileChooserTest,
                       FileChooser_SaveAs_DangerousExecutable_Disallowed) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisallowUncheckedDangerousDownloads);
  TestSelectFileDialogFactory test_dialog_factory(
      TestSelectFileDialogFactory::NOT_REACHED,
      TestSelectFileDialogFactory::SelectedFileInfoList());
  RunTestViaHTTP("FileChooser_SaveAsDangerousExecutableDisallowed");
}

IN_PROC_BROWSER_TEST_F(PPAPIFileChooserTest,
                       FileChooser_SaveAs_DangerousExtensionList_Disallowed) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisallowUncheckedDangerousDownloads);
  TestSelectFileDialogFactory test_dialog_factory(
      TestSelectFileDialogFactory::NOT_REACHED,
      TestSelectFileDialogFactory::SelectedFileInfoList());
  RunTestViaHTTP("FileChooser_SaveAsDangerousExtensionListDisallowed");
}

// The kDisallowUncheckedDangerousDownloads switch (whose behavior is verified
// by the FileChooser_SaveAs_DangerousExecutable_Disallowed test above) should
// block the file being downloaded. However, the FakeDatabaseManager reports
// that the requestors document URL matches the Safe Browsing whitelist. Hence
// the download succeeds.
IN_PROC_BROWSER_TEST_F(PPAPIFileChooserTestWithSBService,
                       FileChooser_SaveAs_DangerousExecutable_Whitelist) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisallowUncheckedDangerousDownloads);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath suggested_filename = temp_dir.path().AppendASCII("foo");

  TestSelectFileDialogFactory::SelectedFileInfoList file_info_list;
  file_info_list.push_back(
      ui::SelectedFileInfo(suggested_filename, suggested_filename));
  TestSelectFileDialogFactory test_dialog_factory(
      TestSelectFileDialogFactory::REPLACE_BASENAME, file_info_list);

  RunTestViaHTTP("FileChooser_SaveAsDangerousExecutableAllowed");
  base::FilePath actual_filename = temp_dir.path().AppendASCII("dangerous.exe");

  ASSERT_TRUE(base::PathExists(actual_filename));
  std::string file_contents;
  ASSERT_TRUE(base::ReadFileToString(actual_filename, &file_contents, 100));
  EXPECT_EQ("Hello from PPAPI", file_contents);
}

#endif  // FULL_SAFE_BROWSING
