// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/password_manager_handler.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/passwords/password_manager_presenter.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

using password_manager::MockPasswordStore;

namespace {

class TestSelectFileDialogFactory final : public ui::SelectFileDialogFactory {
 public:
  TestSelectFileDialogFactory() {}
  ~TestSelectFileDialogFactory() override {}
  ui::SelectFileDialog* Create(ui::SelectFileDialog::Listener* listener,
                               ui::SelectFilePolicy* policy) override {
    delete policy;  // Ignore the policy, replace it with a test one.
    return new TestSelectFileDialog(listener, new TestSelectFilePolicy);
  }

 private:
  class TestSelectFilePolicy : public ui::SelectFilePolicy {
   public:
    bool CanOpenSelectFileDialog() override { return true; }
    void SelectFileDenied() override {}
  };

  class TestSelectFileDialog : public ui::SelectFileDialog {
   public:
    TestSelectFileDialog(Listener* listener, ui::SelectFilePolicy* policy)
        : ui::SelectFileDialog(listener, policy) {}

   protected:
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
    ~TestSelectFileDialog() override {}
  };
};

class CallbackTestWebUI : public content::TestWebUI {
 public:
  CallbackTestWebUI() {}
  ~CallbackTestWebUI() override {}
  void RegisterMessageCallback(const std::string& message,
                               const MessageCallback& callback) override;
  void ProcessWebUIMessage(const GURL& source_url,
                           const std::string& message,
                           const base::ListValue& args) override;

  MOCK_CONST_METHOD0(GetWebContents, content::WebContents*());

 private:
  std::map<std::string, MessageCallback> message_callbacks_;
};

void CallbackTestWebUI::RegisterMessageCallback(
    const std::string& message,
    const MessageCallback& callback) {
  message_callbacks_.insert(std::make_pair(message, callback));
}

void CallbackTestWebUI::ProcessWebUIMessage(const GURL& source_url,
                                            const std::string& message,
                                            const base::ListValue& args) {
  std::map<std::string, MessageCallback>::const_iterator callback =
      message_callbacks_.find(message);
  if (callback != message_callbacks_.end()) {
    callback->second.Run(&args);
  }
}

class TestPasswordManagerHandler : public options::PasswordManagerHandler {
 public:
  TestPasswordManagerHandler(
      std::unique_ptr<PasswordManagerPresenter> presenter,
      CallbackTestWebUI* web_ui)
      : PasswordManagerHandler(std::move(presenter)) {
    set_web_ui(web_ui);
  }
  ~TestPasswordManagerHandler() override {}
#if !defined(OS_ANDROID)
  gfx::NativeWindow GetNativeWindow() const override;
#endif

  MOCK_METHOD3(FileSelected, void(const base::FilePath& path, int, void*));
};
#if !defined(OS_ANDROID)
gfx::NativeWindow TestPasswordManagerHandler::GetNativeWindow() const {
  return NULL;
}
#endif

class MockPasswordManagerPresenter : public PasswordManagerPresenter {
 public:
  explicit MockPasswordManagerPresenter(PasswordUIView* handler)
      : PasswordManagerPresenter(handler) {}
  ~MockPasswordManagerPresenter() override {}

  MOCK_METHOD0(IsUserAuthenticated, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordManagerPresenter);
};

class DummyPasswordManagerHandler : public PasswordUIView {
 public:
  explicit DummyPasswordManagerHandler(Profile* profile)
      : profile_(profile), password_manager_presenter_(this) {
    password_manager_presenter_.Initialize();
  }
  ~DummyPasswordManagerHandler() override {}
  Profile* GetProfile() override;

  void ShowPassword(size_t,
                    const std::string&,
                    const std::string&,
                    const base::string16&) override {}
  void SetPasswordList(
      const std::vector<std::unique_ptr<autofill::PasswordForm>>&) override {}
  void SetPasswordExceptionList(
      const std::vector<std::unique_ptr<autofill::PasswordForm>>&) override {}

#if !defined(OS_ANDROID)
  gfx::NativeWindow GetNativeWindow() const override;
#endif
 private:
  Profile* profile_;
  PasswordManagerPresenter password_manager_presenter_;

  DISALLOW_COPY_AND_ASSIGN(DummyPasswordManagerHandler);
};

#if !defined(OS_ANDROID)
gfx::NativeWindow DummyPasswordManagerHandler::GetNativeWindow() const {
  return NULL;
}
#endif

Profile* DummyPasswordManagerHandler::GetProfile() {
  return profile_;
}

}  // namespace

class PasswordManagerHandlerTest : public ChromeRenderViewHostTestHarness {
 protected:
  PasswordManagerHandlerTest() {}
  ~PasswordManagerHandlerTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    dummy_handler_.reset(new DummyPasswordManagerHandler(profile()));
    presenter_raw_ = new MockPasswordManagerPresenter(dummy_handler_.get());
    handler_.reset(new TestPasswordManagerHandler(
        base::WrapUnique(presenter_raw_), &web_ui_));
    handler_->RegisterMessages();
    ui::SelectFileDialog::SetFactory(new TestSelectFileDialogFactory);
    handler_->InitializeHandler();
    web_ui_.set_web_contents(web_contents());
  }

  void TearDown() override {
    handler_.reset();
    dummy_handler_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void ExportPassword() {
    base::ListValue tmp;
    web_ui_.ProcessWebUIMessage(GURL(), "exportPassword", tmp);
  }

  void ImportPassword() {
    base::ListValue tmp;
    web_ui_.ProcessWebUIMessage(GURL(), "importPassword", tmp);
  }

  PasswordManagerPresenter* presenter_raw_;
  CallbackTestWebUI web_ui_;
  std::unique_ptr<DummyPasswordManagerHandler> dummy_handler_;
  std::unique_ptr<TestPasswordManagerHandler> handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordManagerHandlerTest);
};

MATCHER(IsEmptyPath, "") {
  return arg.empty();
}

TEST_F(PasswordManagerHandlerTest, PasswordImport) {
  EXPECT_CALL(web_ui_, GetWebContents())
      .WillRepeatedly(testing::Return(web_contents()));
  EXPECT_CALL(
      *handler_,
      FileSelected(IsEmptyPath(), 1,
                   reinterpret_cast<void*>(
                       TestPasswordManagerHandler::IMPORT_FILE_SELECTED)));
  ImportPassword();
}

TEST_F(PasswordManagerHandlerTest, PasswordExport) {
  const base::FilePath file_path;
  EXPECT_CALL(*(static_cast<MockPasswordManagerPresenter*>(presenter_raw_)),
              IsUserAuthenticated())
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(
      *handler_,
      FileSelected(IsEmptyPath(), 1,
                   reinterpret_cast<void*>(
                       TestPasswordManagerHandler::EXPORT_FILE_SELECTED)));
  ExportPassword();
}
