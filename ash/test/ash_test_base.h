// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_TEST_BASE_H_
#define ASH_TEST_ASH_TEST_BASE_H_

#include <string>

#include "ash/shell.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/test_views_delegate.h"

namespace ash {
namespace internal {
class MultiDisplayManager;
}  // internal

namespace test {

class TestShellDelegate;

class AshTestViewsDelegate : public views::TestViewsDelegate {
 public:
  // Overriden from TestViewsDelegate.
  virtual content::WebContents* CreateWebContents(
      content::BrowserContext* browser_context,
      content::SiteInstance* site_instance) OVERRIDE;
};

class AshTestBase : public testing::Test {
 public:
  AshTestBase();
  virtual ~AshTestBase();

  MessageLoopForUI* message_loop() { return &message_loop_; }

  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Change the primary display's configuration to use |bounds|
  // and |scale|.
  void ChangeDisplayConfig(float scale, const gfx::Rect& bounds);

  // Update the display configuration as given in |display_specs|.
  // See ash::test::MultiDisplayManagerTestApi::UpdateDisplay for more details.
  void UpdateDisplay(const std::string& display_specs);

 protected:
  void RunAllPendingInMessageLoop();

  // Utility methods to emulate user logged in or not and
  // session started or not cases.
  void SetSessionStarted(bool session_started);
  void SetUserLoggedIn(bool user_logged_in);

 private:
  MessageLoopForUI message_loop_;

  TestShellDelegate* test_shell_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AshTestBase);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_ASH_TEST_BASE_H_
