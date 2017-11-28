// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_manager_porter.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/passwords/destination_file_system.h"
#include "chrome/browser/ui/passwords/password_manager_presenter.h"
#include "chrome/browser/ui/passwords/password_ui_view.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/export/password_manager_exporter.h"
#include "components/password_manager/core/browser/ui/credential_provider_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"

using ::testing::_;
using ::testing::StrictMock;

namespace {

#if defined(OS_WIN)
const base::FilePath::CharType kNullFileName[] = FILE_PATH_LITERAL("/nul");
#else
const base::FilePath::CharType kNullFileName[] = FILE_PATH_LITERAL("/dev/null");
#endif

class TestSelectFileDialog : public ui::SelectFileDialog {
 public:
  TestSelectFileDialog(Listener* listener,
                       std::unique_ptr<ui::SelectFilePolicy> policy,
                       const base::FilePath& forced_path)
      : ui::SelectFileDialog(listener, std::move(policy)),
        forced_path_(forced_path) {}

 protected:
  ~TestSelectFileDialog() override {}

  void SelectFileImpl(Type type,
                      const base::string16& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override {
    listener_->FileSelected(forced_path_, file_type_index, params);
  }
  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }
  void ListenerDestroyed() override {}
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  // The path that will be selected by this dialog.
  base::FilePath forced_path_;

  DISALLOW_COPY_AND_ASSIGN(TestSelectFileDialog);
};

class TestSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  explicit TestSelectFileDialogFactory(const base::FilePath& forced_path)
      : forced_path_(forced_path) {}

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new TestSelectFileDialog(
        listener, std::make_unique<TestSelectFilePolicy>(), forced_path_);
  }

 private:
  class TestSelectFilePolicy : public ui::SelectFilePolicy {
   public:
    bool CanOpenSelectFileDialog() override { return true; }
    void SelectFileDenied() override {}

   private:
    DISALLOW_ASSIGN(TestSelectFilePolicy);
  };

  // The path that will be selected by created dialogs.
  base::FilePath forced_path_;

  DISALLOW_ASSIGN(TestSelectFileDialogFactory);
};

class TestPasswordManagerPorter : public PasswordManagerPorter {
 public:
  TestPasswordManagerPorter(
      std::unique_ptr<password_manager::PasswordManagerExporter> exporter)
      : PasswordManagerPorter(std::move(exporter)) {}

  MOCK_METHOD1(ImportPasswordsFromPath, void(const base::FilePath& path));

  MOCK_METHOD1(ExportPasswordsToPath, void(const base::FilePath& path));

 private:
  DISALLOW_COPY_AND_ASSIGN(TestPasswordManagerPorter);
};

class MockPasswordManagerExporter
    : public password_manager::PasswordManagerExporter {
 public:
  MockPasswordManagerExporter()
      : password_manager::PasswordManagerExporter(nullptr) {}
  ~MockPasswordManagerExporter() override = default;

  MOCK_METHOD0(PreparePasswordsForExport, void());
  MOCK_METHOD0(Cancel, void());

  void SetDestination(
      std::unique_ptr<password_manager::Destination> destination) override {
    SetDestinationPtr(destination.get());
  }
  MOCK_METHOD1(SetDestinationPtr, void(password_manager::Destination*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordManagerExporter);
};

class PasswordManagerPorterTest : public testing::Test {
 protected:
  PasswordManagerPorterTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}
  ~PasswordManagerPorterTest() override = default;

  void SetUp() override {
    password_manager_porter_.reset(new TestPasswordManagerPorter(nullptr));
    profile_.reset(new TestingProfile());
    web_contents_.reset(content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr));
    // SelectFileDialog::SetFactory is responsible for freeing the memory
    // associated with a new factory.
    selected_file_ = base::FilePath(kNullFileName);
    ui::SelectFileDialog::SetFactory(
        new TestSelectFileDialogFactory(selected_file_));
  }

  void TearDown() override {}

  TestPasswordManagerPorter* password_manager_porter() const {
    return password_manager_porter_.get();
  }

  content::WebContents* web_contents() const { return web_contents_.get(); }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  // The file that our fake file selector returns.
  // This file should not actually be used by the test.
  base::FilePath selected_file_;

 private:
  // TODO(crbug.com/689520) This is needed for mojo not to crash on destruction.
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestPasswordManagerPorter> password_manager_porter_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerPorterTest);
};

// Password importing and exporting using a |SelectFileDialog| is not yet
// supported on Android.
#if !defined(OS_ANDROID)

TEST_F(PasswordManagerPorterTest, PasswordImport) {
  EXPECT_CALL(*password_manager_porter(), ImportPasswordsFromPath(_));

  password_manager_porter()->set_web_contents(web_contents());
  password_manager_porter()->Load();
}

// Matches a DestinationFileSystem* that is set to |path|.
MATCHER_P(DestinationPointsToPath, path, "") {
  DestinationFileSystem* destination = static_cast<DestinationFileSystem*>(arg);
  return destination->GetDestinationPathForTesting() == path;
}

TEST_F(PasswordManagerPorterTest, PasswordExport) {
  // PasswordManagerPorter will take ownership of this, but we keep a pointer.
  MockPasswordManagerExporter* mock_password_manager_exporter_ =
      new StrictMock<MockPasswordManagerExporter>();
  PasswordManagerPorter porter((std::unique_ptr<MockPasswordManagerExporter>(
      mock_password_manager_exporter_)));

  EXPECT_CALL(*mock_password_manager_exporter_, PreparePasswordsForExport());
  EXPECT_CALL(*mock_password_manager_exporter_,
              SetDestinationPtr(DestinationPointsToPath(selected_file_)));

  porter.set_web_contents(web_contents());
  porter.Store();
}

#endif

}  // namespace
