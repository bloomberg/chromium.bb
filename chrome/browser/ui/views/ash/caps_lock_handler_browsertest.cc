// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/caps_lock_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
using namespace chromeos::input_method;
#endif

namespace {

#if defined(OS_CHROMEOS)
class DummyXKeyboard : public XKeyboard {
 public:
  explicit DummyXKeyboard(bool initial_caps_lock_state)
      : caps_lock_is_enabled_(initial_caps_lock_state) {}
  virtual ~DummyXKeyboard() {}

  // Overridden from chrome::input_method::XKeyboard:
  virtual bool SetCurrentKeyboardLayoutByName(
      const std::string& layout_name) OVERRIDE {
    return true;
  }
  virtual bool RemapModifierKeys(const ModifierMap& modifier_map) OVERRIDE {
    return true;
  }
  virtual bool ReapplyCurrentKeyboardLayout() OVERRIDE {
    return true;
  }
  virtual void ReapplyCurrentModifierLockStatus() OVERRIDE {}
  virtual void SetLockedModifiers(
      ModifierLockStatus new_caps_lock_status,
      ModifierLockStatus new_num_lock_status) OVERRIDE {}
  virtual void SetNumLockEnabled(bool enable_num_lock) OVERRIDE {}
  virtual void SetCapsLockEnabled(bool enable_caps_lock) OVERRIDE {
    caps_lock_is_enabled_ = enable_caps_lock;
  }
  virtual bool NumLockIsEnabled() OVERRIDE {
    return true;
  }
  virtual bool CapsLockIsEnabled() OVERRIDE {
    return caps_lock_is_enabled_;
  }
  virtual std::string CreateFullXkbLayoutName(
      const std::string& layout_name,
      const ModifierMap& modifire_map) OVERRIDE {
    return "";
  }
  virtual unsigned int GetNumLockMask() OVERRIDE {
    return 0;
  }
  virtual void GetLockedModifiers(bool* out_caps_lock_enabled,
                                  bool* out_num_lock_enabled) OVERRIDE {}

 private:
  bool caps_lock_is_enabled_;

  DISALLOW_COPY_AND_ASSIGN(DummyXKeyboard);
};

class CapsLockHandlerTest : public InProcessBrowserTest {
 public:
  CapsLockHandlerTest()
      : initial_caps_lock_state_(false),
        xkeyboard_(initial_caps_lock_state_) {
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
  DummyXKeyboard xkeyboard_;
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
