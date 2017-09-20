// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_manager_porter.h"

#include "build/build_config.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/passwords/password_manager_presenter.h"
#include "chrome/browser/ui/passwords/password_ui_view.h"
#include "chrome/test/base/testing_profile.h"
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
                       std::unique_ptr<ui::SelectFilePolicy> policy)
      : ui::SelectFileDialog(listener, std::move(policy)) {}

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
    listener_->FileSelected(default_path, file_type_index, params);
  }
  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }
  void ListenerDestroyed() override {}
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSelectFileDialog);
};

class TestSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new TestSelectFileDialog(listener,
                                    std::make_unique<TestSelectFilePolicy>());
  }

 private:
  class TestSelectFilePolicy : public ui::SelectFilePolicy {
   public:
    bool CanOpenSelectFileDialog() override { return true; }
    void SelectFileDenied() override {}

   private:
    DISALLOW_ASSIGN(TestSelectFilePolicy);
  };

  DISALLOW_ASSIGN(TestSelectFileDialogFactory);
};

class TestCredentialProviderInterface : public CredentialProviderInterface {
 public:
  TestCredentialProviderInterface() {}

  // CredentialProviderInterface:
  std::vector<std::unique_ptr<autofill::PasswordForm>> GetAllPasswords()
      override {
    return std::vector<std::unique_ptr<autofill::PasswordForm>>();
  }

 private:
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
}  // namespace

class PasswordManagerPorterTest : public testing::Test {
 protected:
  PasswordManagerPorterTest() {}

  void SetUp() override {
    credential_provider_interface_.reset(new TestCredentialProviderInterface());
    password_manager_porter_.reset(
        new TestPasswordManagerPorter(credential_provider_interface_.get()));
    profile_.reset(new TestingProfile());
    web_contents_.reset(content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr));
    // SelectFileDialog::SetFactory is responsible for freeing the memory
    // associated with a new factory.
    ui::SelectFileDialog::SetFactory(new TestSelectFileDialogFactory());
  }

  TestPasswordManagerPorter* password_manager_porter() const {
    return password_manager_porter_.get();
  }

  content::WebContents* web_contents() const { return web_contents_.get(); }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestCredentialProviderInterface>
      credential_provider_interface_;
  std::unique_ptr<TestPasswordManagerPorter> password_manager_porter_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  scoped_refptr<password_manager::PasswordStore> store_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerPorterTest);
};

// Password importing and exporting using a |SelectFileDialog| is not yet
// supported on Android.
#if !defined(OS_ANDROID)
TEST_F(PasswordManagerPorterTest, PasswordImport) {
  EXPECT_CALL(*password_manager_porter(), ImportPasswordsFromPath(_));

  password_manager_porter()->PresentFileSelector(
      web_contents(), PasswordManagerPorter::Type::PASSWORD_IMPORT);
}

TEST_F(PasswordManagerPorterTest, PasswordExport) {
  EXPECT_CALL(*password_manager_porter(), ExportPasswordsToPath(_));

  password_manager_porter()->PresentFileSelector(
      web_contents(), PasswordManagerPorter::Type::PASSWORD_EXPORT);
}
#endif
