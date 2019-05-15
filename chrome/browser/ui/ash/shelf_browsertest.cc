// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/shelf_test_api.test-mojom-test-utils.h"
#include "ash/public/interfaces/shelf_test_api.test-mojom.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

class ShelfBrowserTest : public InProcessBrowserTest {
 public:
  ShelfBrowserTest() = default;
  ~ShelfBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Connect to the ash test interface for the shelf.
    content::ServiceManagerConnection::GetForProcess()
        ->GetConnector()
        ->BindInterface(ash::mojom::kServiceName, &shelf_test_api_);
  }

 protected:
  ash::mojom::ShelfTestApiPtr shelf_test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfBrowserTest);
};

class ShelfGuestSessionBrowserTest : public ShelfBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(chromeos::switches::kGuestSession);
    command_line->AppendSwitch(::switches::kIncognito);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "hash");
    command_line->AppendSwitchASCII(
        chromeos::switches::kLoginUser,
        user_manager::GuestAccountId().GetUserEmail());
  }
};

// Tests that in guest session, shelf alignment could be initialized to bottom
// aligned, instead of bottom locked (crbug.com/699661).
IN_PROC_BROWSER_TEST_F(ShelfGuestSessionBrowserTest, ShelfAlignment) {
  // Check the alignment pref for the primary display.
  ash::ShelfAlignment alignment = ash::GetShelfAlignmentPref(
      browser()->profile()->GetPrefs(),
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_EQ(ash::SHELF_ALIGNMENT_BOTTOM, alignment);

  // Check the locked state, which is not exposed via prefs.
  ash::mojom::ShelfTestApiAsyncWaiter shelf(shelf_test_api_.get());
  bool shelf_bottom_locked = false;
  shelf.IsAlignmentBottomLocked(&shelf_bottom_locked);
  EXPECT_FALSE(shelf_bottom_locked);
}
