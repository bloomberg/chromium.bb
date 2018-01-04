// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"

#include "ash/shell.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/core/account_id/account_id.h"
#include "url/gurl.h"

namespace {

constexpr char kTestUser[] = "test-user@gmail.com";
constexpr char kTestUserGaiaId[] = "1234567890";

class SystemWebDialogTest : public chromeos::LoginManagerTest {
 public:
  SystemWebDialogTest() : LoginManagerTest(false) {}
  ~SystemWebDialogTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemWebDialogTest);
};

class MockSystemWebDialog : public chromeos::SystemWebDialogDelegate {
 public:
  MockSystemWebDialog()
      : SystemWebDialogDelegate(GURL(chrome::kChromeUIVersionURL),
                                base::string16()) {}
  ~MockSystemWebDialog() override = default;

  std::string GetDialogArgs() const override { return std::string(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSystemWebDialog);
};

}  // namespace

// Verifies that system dialogs are modal before login (e.g. during OOBE).
IN_PROC_BROWSER_TEST_F(SystemWebDialogTest, ModalTest) {
  chromeos::SystemWebDialogDelegate* dialog = new MockSystemWebDialog();
  dialog->ShowSystemDialog();
  // TODO(jamescook): Fix for mash. http://crbug.com/798797
  EXPECT_TRUE(ash::Shell::IsSystemModalWindowOpen());
}

IN_PROC_BROWSER_TEST_F(SystemWebDialogTest, PRE_NonModalTest) {
  RegisterUser(AccountId::FromUserEmailGaiaId(kTestUser, kTestUserGaiaId));
  chromeos::StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(SystemWebDialogTest, NonModalTest) {
  LoginUser(AccountId::FromUserEmailGaiaId(kTestUser, kTestUserGaiaId));
  chromeos::SystemWebDialogDelegate* dialog = new MockSystemWebDialog();
  dialog->ShowSystemDialog();
  // TODO(jamescook): Fix for mash. http://crbug.com/798797
  EXPECT_FALSE(ash::Shell::IsSystemModalWindowOpen());
}
