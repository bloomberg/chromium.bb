// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/command_line.h"
#include "base/memory/linked_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/switches.h"

namespace extensions {

namespace {

static const size_t kNumMocks = 3;
static const int kNumCharactersInPassword = 10;
static const char kPlaintextPassword[] = "plaintext";

linked_ptr<api::passwords_private::PasswordUiEntry> CreateEntry(size_t num) {
  api::passwords_private::PasswordUiEntry* entry =
      new api::passwords_private::PasswordUiEntry();
  std::stringstream ss;
  ss << "http://test" << num << ".com";
  entry->login_pair.origin_url = ss.str();
  ss.clear();
  ss << "testName" << num;
  entry->login_pair.username = ss.str();
  entry->num_characters_in_password = kNumCharactersInPassword;
  return make_linked_ptr(entry);
}

std::string CreateException(size_t num) {
  std::stringstream ss;
  ss << "http://exception" << num << ".com";
  return ss.str();
}

// A test PasswordsPrivateDelegate implementation which uses mock data.
// TestDelegate starts out with kNumMocks mocks of each type (saved password
// and password exception) and removes one mock each time RemoveSavedPassword()
// or RemovePasswordException() is called.
class TestDelegate : public PasswordsPrivateDelegate {
 public:
  TestDelegate() : observers_(new base::ObserverListThreadSafe<Observer>()) {
    // Create mock data.
    for (size_t i = 0; i < kNumMocks; i++) {
      current_entries_.push_back(CreateEntry(i));;
      current_exceptions_.push_back(CreateException(i));
    }
  }
  ~TestDelegate() override {}

  void AddObserver(Observer* observer) override {
    observers_->AddObserver(observer);
    SendSavedPasswordsList();
    SendPasswordExceptionsList();
  }

  void RemoveObserver(Observer* observer) override {
    observers_->RemoveObserver(observer);
  }

  void RemoveSavedPassword(
      const std::string& origin_url, const std::string& username) override {
    if (!current_entries_.size())
      return;

    // Since this is just mock data, remove the first entry regardless of
    // the data contained.
    current_entries_.erase(current_entries_.begin());
    SendSavedPasswordsList();
  }

  void RemovePasswordException(const std::string& exception_url) override {
    if (!current_exceptions_.size())
      return;

    // Since this is just mock data, remove the first entry regardless of
    // the data contained.
    current_exceptions_.erase(current_exceptions_.begin());
    SendPasswordExceptionsList();
  }

  void RequestShowPassword(const std::string& origin_url,
                           const std::string& username,
                           content::WebContents* web_contents) override {
    // Return a mocked password value.
    std::string plaintext_password(kPlaintextPassword);
    observers_->Notify(
        FROM_HERE,
        &Observer::OnPlaintextPasswordFetched,
        origin_url,
        username,
        plaintext_password);
  }

 private:
  void SendSavedPasswordsList() {
    observers_->Notify(
        FROM_HERE,
        &Observer::OnSavedPasswordsListChanged,
        current_entries_);
  }

  void SendPasswordExceptionsList() {
    observers_->Notify(
        FROM_HERE,
        &Observer::OnPasswordExceptionsListChanged,
        current_exceptions_);
  }

  // The current list of entries/exceptions. Cached here so that when new
  // observers are added, this delegate can send the current lists without
  // having to request them from |password_manager_presenter_| again.
  std::vector<linked_ptr<api::passwords_private::PasswordUiEntry>>
      current_entries_;
  std::vector<std::string> current_exceptions_;

  // The observers.
  scoped_refptr<base::ObserverListThreadSafe<Observer>> observers_;
};

class PasswordsPrivateApiTest : public ExtensionApiTest {
 public:
  PasswordsPrivateApiTest() {
    if (!s_test_delegate_) {
      s_test_delegate_ = new TestDelegate();
    }
  }
  ~PasswordsPrivateApiTest() override {}

  static scoped_ptr<KeyedService> GetPasswordsPrivateDelegate(
      content::BrowserContext* profile) {
    CHECK(s_test_delegate_);
    return make_scoped_ptr(s_test_delegate_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

  void SetUp() override {
    ExtensionApiTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    PasswordsPrivateDelegateFactory::GetInstance()->SetTestingFactory(
        profile(), &PasswordsPrivateApiTest::GetPasswordsPrivateDelegate);
    content::RunAllPendingInMessageLoop();
  }

 protected:
  bool RunPasswordsSubtest(const std::string& subtest) {
    return RunExtensionSubtest("passwords_private",
                               "main.html?" + subtest,
                               kFlagLoadAsComponent);
  }

 private:
  static TestDelegate* s_test_delegate_;

  DISALLOW_COPY_AND_ASSIGN(PasswordsPrivateApiTest);
};

// static
TestDelegate* PasswordsPrivateApiTest::s_test_delegate_ = nullptr;

}  // namespace

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, RemoveSavedPassword) {
  EXPECT_TRUE(RunPasswordsSubtest("removeSavedPassword")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, RemovePasswordException) {
  EXPECT_TRUE(RunPasswordsSubtest("removePasswordException")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, RequestPlaintextPassword) {
  EXPECT_TRUE(RunPasswordsSubtest("requestPlaintextPassword")) << message_;
}

}  // namespace extensions
