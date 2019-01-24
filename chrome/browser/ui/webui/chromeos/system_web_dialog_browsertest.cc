// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/shell_test_api.test-mojom-test-utils.h"
#include "ash/public/interfaces/shell_test_api.test-mojom.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/mus/change_completion_waiter.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

constexpr char kTestUser[] = "test-user@gmail.com";
constexpr char kTestUserGaiaId[] = "1234567890";

// Returns whether a system modal window (e.g. modal dialog) is open. Blocks
// until the ash service responds.
bool IsSystemModalWindowOpen() {
  // Wait for window visibility to stabilize.
  aura::test::WaitForAllChangesToComplete();

  // Connect to the ash test interface.
  ash::mojom::ShellTestApiPtr shell_test_api;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &shell_test_api);
  ash::mojom::ShellTestApiAsyncWaiter waiter(shell_test_api.get());
  bool modal_open = false;
  waiter.IsSystemModalWindowOpen(&modal_open);
  return modal_open;
}

class MockSystemWebDialog : public SystemWebDialogDelegate {
 public:
  explicit MockSystemWebDialog(const char* id = nullptr)
      : SystemWebDialogDelegate(GURL(chrome::kChromeUIVersionURL),
                                base::string16()) {
    if (id)
      id_ = std::string(id);
  }
  ~MockSystemWebDialog() override = default;

  const std::string& Id() override { return id_; }
  std::string GetDialogArgs() const override { return std::string(); }

 private:
  std::string id_;
  DISALLOW_COPY_AND_ASSIGN(MockSystemWebDialog);
};

}  // namespace

class SystemWebDialogLoginTest : public LoginManagerTest {
 public:
  SystemWebDialogLoginTest()
      : LoginManagerTest(false, true /* should_initialize_webui */) {}
  ~SystemWebDialogLoginTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemWebDialogLoginTest);
};

// Verifies that system dialogs are modal before login (e.g. during OOBE).
IN_PROC_BROWSER_TEST_F(SystemWebDialogLoginTest, ModalTest) {
  auto* dialog = new MockSystemWebDialog();
  dialog->ShowSystemDialog();
  EXPECT_TRUE(IsSystemModalWindowOpen());
}

IN_PROC_BROWSER_TEST_F(SystemWebDialogLoginTest, PRE_NonModalTest) {
  RegisterUser(AccountId::FromUserEmailGaiaId(kTestUser, kTestUserGaiaId));
  StartupUtils::MarkOobeCompleted();
}

// Verifies that system dialogs are not modal and always-on-top after login.
IN_PROC_BROWSER_TEST_F(SystemWebDialogLoginTest, NonModalTest) {
  LoginUser(AccountId::FromUserEmailGaiaId(kTestUser, kTestUserGaiaId));
  auto* dialog = new MockSystemWebDialog();
  dialog->ShowSystemDialog();
  EXPECT_FALSE(IsSystemModalWindowOpen());
  aura::Window* window_to_test = dialog->dialog_window();
  // In Mash, the AlwaysOnTop property will be set on the parent.
  if (::features::IsUsingWindowService())
    window_to_test = window_to_test->parent();
  EXPECT_TRUE(window_to_test->GetProperty(aura::client::kAlwaysOnTopKey));
}

using SystemWebDialogTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(SystemWebDialogTest, InstanceTest) {
  const char* kDialogId = "dialog_id";
  SystemWebDialogDelegate* dialog = new MockSystemWebDialog(kDialogId);
  dialog->ShowSystemDialog();
  SystemWebDialogDelegate* found_dialog =
      SystemWebDialogDelegate::FindInstance(kDialogId);
  EXPECT_EQ(dialog, found_dialog);
  // Closing (deleting) the dialog causes a crash in WebDialogView when the main
  // loop is run. TODO(stevenjb): Investigate, fix, and test closing the dialog.
  // https://crbug.com/855344.
}

}  // namespace chromeos
