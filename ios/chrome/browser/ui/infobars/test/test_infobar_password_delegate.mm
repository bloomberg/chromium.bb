// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/test/test_infobar_password_delegate.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/infobars/core/infobar.h"
#include "components/password_manager/core/browser/credential_manager_password_form_manager.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::PasswordForm;
using base::ASCIIToUTF16;

namespace {

class MockDelegate
    : public password_manager::CredentialManagerPasswordFormManagerDelegate {
 public:
  MOCK_METHOD0(OnProvisionalSaveComplete, void());
};

class MockFormSaver : public password_manager::StubFormSaver {
 public:
  MockFormSaver() = default;
  ~MockFormSaver() override = default;

  // FormSaver:
  MOCK_METHOD3(Save,
               void(PasswordForm pending,
                    const std::vector<const PasswordForm*>& matches,
                    const base::string16& old_password));
  MOCK_METHOD3(Update,
               void(PasswordForm pending,
                    const std::vector<const PasswordForm*>& matches,
                    const base::string16& old_password));

  // Convenience downcasting method.
  static MockFormSaver& Get(
      password_manager::PasswordFormManager* form_manager) {
    return *static_cast<MockFormSaver*>(form_manager->form_saver());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFormSaver);
};

std::unique_ptr<password_manager::CredentialManagerPasswordFormManager>
CreateFormManager() {
  PasswordForm form_to_save;
  form_to_save.origin = GURL("https://example.com/path");
  form_to_save.signon_realm = "https://example.com/";
  form_to_save.username_value = ASCIIToUTF16("user1");
  form_to_save.password_value = ASCIIToUTF16("pass1");
  form_to_save.scheme = PasswordForm::Scheme::kHtml;
  form_to_save.type = PasswordForm::Type::kApi;
  MockDelegate delegate;
  password_manager::StubPasswordManagerClient client;

  std::unique_ptr<password_manager::FakeFormFetcher> fetcher(
      new password_manager::FakeFormFetcher());
  std::unique_ptr<MockFormSaver> saver(new MockFormSaver());
  return std::make_unique<
      password_manager::CredentialManagerPasswordFormManager>(
      &client, std::make_unique<PasswordForm>(form_to_save), &delegate,
      std::make_unique<MockFormSaver>(),
      std::make_unique<password_manager::FakeFormFetcher>());
}

}  // namespace

TestInfobarPasswordDelegate::TestInfobarPasswordDelegate(
    NSString* infobar_message)
    : IOSChromeSavePasswordInfoBarDelegate(false, false, CreateFormManager()),
      infobar_message_(infobar_message) {}

bool TestInfobarPasswordDelegate::Create(
    infobars::InfoBarManager* infobar_manager) {
  DCHECK(infobar_manager);
  return !!infobar_manager->AddInfoBar(infobar_manager->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(this)));
}

TestInfobarPasswordDelegate::InfoBarIdentifier
TestInfobarPasswordDelegate::GetIdentifier() const {
  return TEST_INFOBAR;
}

base::string16 TestInfobarPasswordDelegate::GetMessageText() const {
  return base::SysNSStringToUTF16(infobar_message_);
}

int TestInfobarPasswordDelegate::GetButtons() const {
  return ConfirmInfoBarDelegate::BUTTON_OK;
}
