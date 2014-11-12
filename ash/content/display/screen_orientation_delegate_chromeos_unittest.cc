// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_switches.h"
#include "ash/content/display/screen_orientation_delegate_chromeos.h"
#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_context.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationLockType.h"
#include "ui/gfx/display.h"
#include "ui/views/test/webview_test_helper.h"
#include "ui/views/view.h"
#include "ui/views/views_delegate.h"

namespace ash {

namespace {

gfx::Display::Rotation Rotation() {
  return Shell::GetInstance()
      ->display_manager()
      ->GetDisplayInfo(gfx::Display::InternalDisplayId())
      .rotation();
}

bool RotationLocked() {
  return Shell::GetInstance()->maximize_mode_controller()->rotation_locked();
}

}  // namespace

class ScreenOrientationDelegateTest : public test::AshTestBase {
 public:
  ScreenOrientationDelegateTest();
  virtual ~ScreenOrientationDelegateTest();

  ScreenOrientationDelegate* delegate() { return screen_orientation_delegate_; }

  // Creates and initializes and empty content::WebContents that is backed by a
  // content::BrowserContext and that has an aura::Window.
  content::WebContents* CreateWebContents();

  // Creates a secondary content::WebContents, with a separate
  // content::BrowserContext.
  content::WebContents* CreateSecondaryWebContents();

  // test::AshTestBase:
  void SetUp() override;

 private:
  ScreenOrientationDelegate* screen_orientation_delegate_;

  // Optional content::BrowserContext used for two window tests.
  scoped_ptr<content::BrowserContext> secondary_browser_context_;

  // Setups underlying content layer so that content::WebContents can be
  // generated.
  scoped_ptr<views::WebViewTestHelper> webview_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationDelegateTest);
};

ScreenOrientationDelegateTest::ScreenOrientationDelegateTest() {
  webview_test_helper_.reset(new views::WebViewTestHelper());
}

ScreenOrientationDelegateTest::~ScreenOrientationDelegateTest() {
}

content::WebContents* ScreenOrientationDelegateTest::CreateWebContents() {
  return views::ViewsDelegate::views_delegate->CreateWebContents(
      ash_test_helper()->test_shell_delegate()->GetActiveBrowserContext(),
      nullptr);
}

content::WebContents*
ScreenOrientationDelegateTest::CreateSecondaryWebContents() {
  secondary_browser_context_.reset(new content::TestBrowserContext());
  return views::ViewsDelegate::views_delegate->CreateWebContents(
      secondary_browser_context_.get(), nullptr);
}

void ScreenOrientationDelegateTest::SetUp() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshUseFirstDisplayAsInternal);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshEnableTouchViewTesting);
  test::AshTestBase::SetUp();
  screen_orientation_delegate_ =
      Shell::GetInstance()->screen_orientation_delegate();
}

// Tests that a content::WebContents can lock rotation.
TEST_F(ScreenOrientationDelegateTest, LockOrientation) {
  scoped_ptr<content::WebContents> content(CreateWebContents());
  ASSERT_NE(nullptr, content->GetNativeView());
  ASSERT_EQ(gfx::Display::ROTATE_0, Rotation());
  ASSERT_FALSE(RotationLocked());

  delegate()->Lock(content.get(), blink::WebScreenOrientationLockLandscape);
  EXPECT_EQ(gfx::Display::ROTATE_0, Rotation());
  EXPECT_TRUE(RotationLocked());
}

// Tests that a content::WebContents can unlock rotation.
TEST_F(ScreenOrientationDelegateTest, Unlock) {
  scoped_ptr<content::WebContents> content(CreateWebContents());
  ASSERT_NE(nullptr, content->GetNativeView());
  ASSERT_EQ(gfx::Display::ROTATE_0, Rotation());
  ASSERT_FALSE(RotationLocked());

  delegate()->Lock(content.get(), blink::WebScreenOrientationLockLandscape);
  EXPECT_EQ(gfx::Display::ROTATE_0, Rotation());
  EXPECT_TRUE(RotationLocked());

  delegate()->Unlock(content.get());
  EXPECT_FALSE(RotationLocked());
}

// Tests that a content::WebContents is able to change the orientation of the
// display after having locked rotation.
TEST_F(ScreenOrientationDelegateTest, OrientationChanges) {
  scoped_ptr<content::WebContents> content(CreateWebContents());
  ASSERT_NE(nullptr, content->GetNativeView());
  ASSERT_EQ(gfx::Display::ROTATE_0, Rotation());
  ASSERT_FALSE(RotationLocked());

  delegate()->Lock(content.get(), blink::WebScreenOrientationLockPortrait);
  EXPECT_EQ(gfx::Display::ROTATE_90, Rotation());
  EXPECT_TRUE(RotationLocked());

  delegate()->Lock(content.get(), blink::WebScreenOrientationLockLandscape);
  EXPECT_EQ(gfx::Display::ROTATE_0, Rotation());
}

// Tests that a user initiated rotation lock cannot be unlocked by a
// content::WebContents.
TEST_F(ScreenOrientationDelegateTest, UserLockRejectsUnlock) {
  Shell::GetInstance()->maximize_mode_controller()->SetRotationLocked(true);

  scoped_ptr<content::WebContents> content(CreateWebContents());
  delegate()->Unlock(content.get());
  EXPECT_TRUE(RotationLocked());
}

// Tests that orientation can only be set by the first content::WebContents that
// has set a rotation lock.
TEST_F(ScreenOrientationDelegateTest, SecondContentCannotChangeOrientation) {
  scoped_ptr<content::WebContents> content1(CreateWebContents());
  scoped_ptr<content::WebContents> content2(CreateSecondaryWebContents());
  ASSERT_NE(content1->GetNativeView(), content2->GetNativeView());

  delegate()->Lock(content1.get(), blink::WebScreenOrientationLockLandscape);
  delegate()->Lock(content2.get(), blink::WebScreenOrientationLockPortrait);
  EXPECT_EQ(gfx::Display::ROTATE_0, Rotation());
}

// Tests that only the content::WebContents that set a rotation lock can perform
// an unlock.
TEST_F(ScreenOrientationDelegateTest, SecondContentCannotUnlock) {
  scoped_ptr<content::WebContents> content1(CreateWebContents());
  scoped_ptr<content::WebContents> content2(CreateSecondaryWebContents());
  ASSERT_NE(content1->GetNativeView(), content2->GetNativeView());

  delegate()->Lock(content1.get(), blink::WebScreenOrientationLockLandscape);
  delegate()->Unlock(content2.get());
  EXPECT_TRUE(RotationLocked());
}

// Tests that alternate content::WebContents can set a rotation lock after a
// preexisting lock has been released.
TEST_F(ScreenOrientationDelegateTest, AfterUnlockSecondContentCanLock) {
  scoped_ptr<content::WebContents> content1(CreateWebContents());
  scoped_ptr<content::WebContents> content2(CreateSecondaryWebContents());
  ASSERT_NE(content1->GetNativeView(), content2->GetNativeView());

  delegate()->Lock(content1.get(), blink::WebScreenOrientationLockLandscape);
  delegate()->Unlock(content1.get());
  delegate()->Lock(content2.get(), blink::WebScreenOrientationLockPortrait);
  EXPECT_EQ(gfx::Display::ROTATE_90, Rotation());
  EXPECT_TRUE(RotationLocked());
}

}  // namespace ash
