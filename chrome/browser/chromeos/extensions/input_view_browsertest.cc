// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "base/files/file_path.h"
#include "chrome/browser/chromeos/extensions/virtual_keyboard_browsertest.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_notification_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/ime/input_method_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/ime/input_method.h"

namespace {

const base::FilePath::CharType kExtensionName[] = "GoogleKeyboardInput-xkb.crx";

const base::FilePath::CharType kInputViewTestDir[] =
    "chromeos/virtual_keyboard/inputview/";
const base::FilePath::CharType kBaseKeyboardTestFramework[] = "test_base.js";

const char kDefaultLayout[] = "us";
const char kCompactLayout[] = "us.compact.qwerty";

struct InputViewConfig : public VirtualKeyboardBrowserTestConfig {
  explicit InputViewConfig(std::string id, std::string layout) {
    base_framework_ = kBaseKeyboardTestFramework;
    extension_id_ = id;
    test_dir_ = kInputViewTestDir;
    url_ = std::string(extensions::kExtensionScheme) +
        url::kStandardSchemeSeparator + id + "/inputview.html?id=" +
        layout;
  }
};

}  // namespace

class InputViewBrowserTest : public VirtualKeyboardBrowserTest {
 public:
  // Installs the IME Extension keyboard |kExtensionName|.
  std::string InstallIMEExtension() {
    // Loads extension.
    base::FilePath path = ui_test_utils::GetTestFilePath(
        base::FilePath(kInputViewTestDir), base::FilePath(kExtensionName));
    ExtensionService* service = extensions::ExtensionSystem::Get(
        browser()->profile())->extension_service();
    scoped_refptr<extensions::CrxInstaller> installer =
        extensions::CrxInstaller::CreateSilent(service);

    ExtensionTestNotificationObserver observer(browser());
    observer.Watch(chrome::NOTIFICATION_CRX_INSTALLER_DONE,
                   content::Source<extensions::CrxInstaller>(installer.get()));
    installer->set_allow_silent_install(true);
    installer->set_creation_flags(extensions::Extension::FROM_WEBSTORE);
    installer->InstallCrx(path);
    // Wait for CRX to be installed.
    observer.Wait();
    std::string extensionId = installer->extension()->id();
    if (!service->GetExtensionById(extensionId, false))
      return "";
    return extensionId;
  }
};

IN_PROC_BROWSER_TEST_F(InputViewBrowserTest, TypingTest) {
  std::string id = InstallIMEExtension();
  ASSERT_FALSE(id.empty());
  RunTest(base::FilePath("typing_test.js"),
          InputViewConfig(id, kDefaultLayout));
}

IN_PROC_BROWSER_TEST_F(InputViewBrowserTest, CompactTypingTest) {
  std::string id = InstallIMEExtension();
  ASSERT_FALSE(id.empty());
  RunTest(base::FilePath("typing_test.js"),
          InputViewConfig(id, kCompactLayout));
}

IN_PROC_BROWSER_TEST_F(InputViewBrowserTest, CompactLongpressTest) {
  std::string id = InstallIMEExtension();
  ASSERT_FALSE(id.empty());
  RunTest(base::FilePath("longpress_test.js"),
          InputViewConfig(id, kCompactLayout));
}

// Disabled for leaking memory: http://crbug.com/380537
IN_PROC_BROWSER_TEST_F(InputViewBrowserTest, DISABLED_KeysetTransitionTest) {
  std::string id = InstallIMEExtension();
  ASSERT_FALSE(id.empty());
  RunTest(base::FilePath("keyset_transition_test.js"),
          InputViewConfig(id, kDefaultLayout));
}

// Disabled for leaking memory: http://crbug.com/380537
IN_PROC_BROWSER_TEST_F(InputViewBrowserTest,
                       DISABLED_CompactKeysetTransitionTest) {
  std::string id = InstallIMEExtension();
  ASSERT_FALSE(id.empty());
  RunTest(base::FilePath("keyset_transition_test.js"),
          InputViewConfig(id, kCompactLayout));
}
