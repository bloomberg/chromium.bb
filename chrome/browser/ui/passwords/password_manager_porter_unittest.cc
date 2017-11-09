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
#include "chrome/browser/ui/passwords/password_manager_presenter.h"
#include "chrome/browser/ui/passwords/password_ui_view.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/ui/credential_provider_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"

using ::testing::_;

namespace {
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

class TestCredentialProviderInterface
    : public password_manager::CredentialProviderInterface {
 public:
  TestCredentialProviderInterface() {}

  void SetPasswordList(
      const std::vector<std::unique_ptr<autofill::PasswordForm>>&
          password_list) {
    password_list_.clear();
    for (const auto& form : password_list) {
      password_list_.push_back(std::make_unique<autofill::PasswordForm>(*form));
    }
  }

  // password_manager::CredentialProviderInterface:
  std::vector<std::unique_ptr<autofill::PasswordForm>> GetAllPasswords()
      override {
    std::vector<std::unique_ptr<autofill::PasswordForm>> ret_val;
    for (const auto& form : password_list_) {
      ret_val.push_back(std::make_unique<autofill::PasswordForm>(*form));
    }
    return ret_val;
  }

 private:
  std::vector<std::unique_ptr<autofill::PasswordForm>> password_list_;

  DISALLOW_COPY_AND_ASSIGN(TestCredentialProviderInterface);
};

class TestPasswordManagerPorter : public PasswordManagerPorter {
 public:
  TestPasswordManagerPorter(
      TestCredentialProviderInterface* password_manager_presenter)
      : PasswordManagerPorter(password_manager_presenter) {}

  MOCK_METHOD1(ImportPasswordsFromPath, void(const base::FilePath& path));

  MOCK_METHOD1(ExportPasswordsToPath, void(const base::FilePath& path));

 private:
  DISALLOW_COPY_AND_ASSIGN(TestPasswordManagerPorter);
};

class PasswordManagerPorterTest : public testing::Test {
 protected:
  PasswordManagerPorterTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}
  ~PasswordManagerPorterTest() override = default;

  void SetUp() override {
    credential_provider_interface_.reset(new TestCredentialProviderInterface());
    password_manager_porter_.reset(
        new TestPasswordManagerPorter(credential_provider_interface_.get()));
    profile_.reset(new TestingProfile());
    web_contents_.reset(content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr));
    // SelectFileDialog::SetFactory is responsible for freeing the memory
    // associated with a new factory.
    ASSERT_TRUE(base::CreateTemporaryFile(&temp_file_));
    temp_file_ = temp_file_.ReplaceExtension(FILE_PATH_LITERAL("csv"));
    ui::SelectFileDialog::SetFactory(
        new TestSelectFileDialogFactory(temp_file_));
  }

  void TearDown() override { ASSERT_TRUE(base::DeleteFile(temp_file_, false)); }

  TestPasswordManagerPorter* password_manager_porter() const {
    return password_manager_porter_.get();
  }

  content::WebContents* web_contents() const { return web_contents_.get(); }

  std::unique_ptr<TestCredentialProviderInterface>
      credential_provider_interface_;
  // A file path that will be selected by any file selection dialog.
  base::FilePath temp_file_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  // TODO(crbug.com/689520) This is needed for mojo not to crash on destruction.
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestPasswordManagerPorter> password_manager_porter_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  scoped_refptr<password_manager::PasswordStore> store_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerPorterTest);
};

// Password importing and exporting using a |SelectFileDialog| is not yet
// supported on Android.
#if !defined(OS_ANDROID)

std::vector<std::unique_ptr<autofill::PasswordForm>>
ConstructTestPasswordForms() {
  auto password_form = std::make_unique<autofill::PasswordForm>();
  password_form->origin = GURL("http://accounts.google.com/a/LoginAuth");
  password_form->username_value = base::ASCIIToUTF16("test@gmail.com");
  password_form->password_value = base::ASCIIToUTF16("test1");

  std::vector<std::unique_ptr<autofill::PasswordForm>> password_forms;
  password_forms.push_back(std::move(password_form));
  return password_forms;
}

TEST_F(PasswordManagerPorterTest, PasswordImport) {
  EXPECT_CALL(*password_manager_porter(), ImportPasswordsFromPath(_));

  password_manager_porter()->set_web_contents(web_contents());
  password_manager_porter()->Load();
}

TEST_F(PasswordManagerPorterTest, PasswordExport) {
#if defined(OS_WIN)
  const char kLineEnding[] = "\r\n";
#else
  const char kLineEnding[] = "\n";
#endif
  const std::string kExpectedCSVOutput = base::StringPrintf(
      "name,url,username,password%s"
      "accounts.google.com,http://accounts.google.com/a/"
      "LoginAuth,test@gmail.com,test1%s",
      kLineEnding, kLineEnding);

  credential_provider_interface_->SetPasswordList(ConstructTestPasswordForms());
  PasswordManagerPorter porter(credential_provider_interface_.get());
  porter.set_web_contents(web_contents());

  porter.Store();

  scoped_task_environment_.RunUntilIdle();

  std::string output;
  base::ReadFileToString(temp_file_, &output);
  EXPECT_EQ(kExpectedCSVOutput, output);
}

#endif

}  // namespace
