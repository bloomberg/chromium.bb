// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/caps_lock_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/input_method/mock_xkeyboard.h"
using namespace chromeos::input_method;
#endif

namespace {

#if defined(OS_CHROMEOS)
class CapsLockHandlerTest : public InProcessBrowserTest {
 public:
  CapsLockHandlerTest()
      : initial_caps_lock_state_(false) {
  }
  virtual void SetUp() OVERRIDE {
    handler_.reset(new CapsLockHandler(&xkeyboard_));
    // Force CapsLockHandler::HandleToggleCapsLock() to toggle the lock state.
    handler_->set_is_running_on_chromeos_for_test(true);
  }
  virtual void TearDown() OVERRIDE {
    handler_.reset();
  }

 protected:
  const bool initial_caps_lock_state_;
  MockXKeyboard xkeyboard_;
  scoped_ptr<CapsLockHandler> handler_;

  DISALLOW_COPY_AND_ASSIGN(CapsLockHandlerTest);
};
#endif

}  // namespace

#if defined(OS_CHROMEOS)
// Check if HandleToggleCapsLock() really changes the lock state.
IN_PROC_BROWSER_TEST_F(CapsLockHandlerTest, TestCapsLock) {
  EXPECT_EQ(initial_caps_lock_state_, handler_->caps_lock_is_on_for_test());
  EXPECT_TRUE(handler_->HandleToggleCapsLock());
  EXPECT_EQ(!initial_caps_lock_state_, xkeyboard_.CapsLockIsEnabled());
  handler_->OnCapsLockChange(!initial_caps_lock_state_);
  EXPECT_EQ(!initial_caps_lock_state_, handler_->caps_lock_is_on_for_test());
  EXPECT_TRUE(handler_->HandleToggleCapsLock());
  handler_->OnCapsLockChange(initial_caps_lock_state_);
  EXPECT_EQ(initial_caps_lock_state_, xkeyboard_.CapsLockIsEnabled());
  EXPECT_EQ(initial_caps_lock_state_, handler_->caps_lock_is_on_for_test());
}
#endif
