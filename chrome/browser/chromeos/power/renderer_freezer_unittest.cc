// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/renderer_freezer.h"

#include <string>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {
// Class that delegates used in testing can inherit from to record calls that
// are made by the code being tested.
class ActionRecorder {
 public:
  ActionRecorder() {}
  virtual ~ActionRecorder() {}

  // Returns a comma-separated string describing the actions that were
  // requested since the previous call to GetActions() (i.e. results are
  // non-repeatable).
  std::string GetActions() {
    std::string actions = actions_;
    actions_.clear();
    return actions;
  }

 protected:
  // Appends |new_action| to |actions_|, using a comma as a separator if
  // other actions are already listed.
  void AppendAction(const std::string& new_action) {
    if (!actions_.empty())
      actions_ += ",";
    actions_ += new_action;
  }

 private:
  // Comma-separated list of actions that have been performed.
  std::string actions_;

  DISALLOW_COPY_AND_ASSIGN(ActionRecorder);
};

// Actions that can be returned by TestDelegate::GetActions().
const char kFreezeRenderers[] = "freeze_renderers";
const char kThawRenderers[] = "thaw_renderers";
const char kNoActions[] = "";

// Test implementation of RendererFreezer::Delegate that records the actions it
// was asked to perform.
class TestDelegate : public RendererFreezer::Delegate, public ActionRecorder {
 public:
  TestDelegate()
      : can_freeze_renderers_(true),
        freeze_renderers_result_(true),
        thaw_renderers_result_(true) {}

  virtual ~TestDelegate() {}

  // RendererFreezer::Delegate overrides.
  virtual bool FreezeRenderers() OVERRIDE {
    AppendAction(kFreezeRenderers);

    return freeze_renderers_result_;
  }
  virtual bool ThawRenderers() OVERRIDE {
    AppendAction(kThawRenderers);

    return thaw_renderers_result_;
  }
  virtual bool CanFreezeRenderers() OVERRIDE { return can_freeze_renderers_; }

  void set_freeze_renderers_result(bool result) {
    freeze_renderers_result_ = result;
  }

  void set_thaw_renderers_result(bool result) {
    thaw_renderers_result_ = result;
  }

  // Sets whether the delegate is capable of freezing renderers.  This also
  // changes |freeze_renderers_result_| and |thaw_renderers_result_|.
  void set_can_freeze_renderers(bool can_freeze) {
    can_freeze_renderers_ = can_freeze;

    // If the delegate cannot freeze renderers, then the result of trying to do
    // so will be false.
    freeze_renderers_result_ = can_freeze;
    thaw_renderers_result_ = can_freeze;
  }

 private:
  bool can_freeze_renderers_;
  bool freeze_renderers_result_;
  bool thaw_renderers_result_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

}  // namespace

class RendererFreezerTest : public testing::Test {
 public:
  RendererFreezerTest()
      : power_manager_client_(new FakePowerManagerClient()),
        test_delegate_(new TestDelegate()) {
    DBusThreadManager::GetSetterForTesting()->SetPowerManagerClient(
        scoped_ptr<PowerManagerClient>(power_manager_client_));
  }

  virtual ~RendererFreezerTest() {
    renderer_freezer_.reset();

    DBusThreadManager::Shutdown();
  }

  void Init() {
    renderer_freezer_.reset(new RendererFreezer(
        scoped_ptr<RendererFreezer::Delegate>(test_delegate_)));
  }

 protected:
  FakePowerManagerClient* power_manager_client_;
  TestDelegate* test_delegate_;

  scoped_ptr<RendererFreezer> renderer_freezer_;

 private:
  base::MessageLoop message_loop_;
  DISALLOW_COPY_AND_ASSIGN(RendererFreezerTest);
};

// Tests that the RendererFreezer freezes renderers on suspend and thaws them on
// resume.
TEST_F(RendererFreezerTest, SuspendResume) {
  Init();

  power_manager_client_->SendSuspendImminent();

  // The RendererFreezer should have grabbed an asynchronous callback and done
  // nothing else.
  EXPECT_EQ(1, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());
  EXPECT_EQ(kNoActions, test_delegate_->GetActions());

  // The RendererFreezer should eventually freeze the renderers and run the
  // callback.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());
  EXPECT_EQ(kFreezeRenderers, test_delegate_->GetActions());

  // The renderers should be thawed when we resume.
  power_manager_client_->SendSuspendDone();
  EXPECT_EQ(kThawRenderers, test_delegate_->GetActions());
}

// Tests that the RendereFreezer doesn't freeze renderers if the suspend attempt
// was canceled before it had a chance to complete.
TEST_F(RendererFreezerTest, SuspendCanceled) {
  Init();

  // We shouldn't do anything yet.
  power_manager_client_->SendSuspendImminent();
  EXPECT_EQ(kNoActions, test_delegate_->GetActions());

  // If a suspend gets canceled for any reason, we should see a SuspendDone().
  power_manager_client_->SendSuspendDone();

  // We shouldn't try to freeze the renderers now.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kNoActions, test_delegate_->GetActions());
}

// Tests that the renderer freezer does nothing if the delegate cannot freeze
// renderers.
TEST_F(RendererFreezerTest, DelegateCannotFreezeRenderers) {
  test_delegate_->set_can_freeze_renderers(false);
  Init();

  power_manager_client_->SendSuspendImminent();

  // The RendererFreezer should not have grabbed a callback or done anything
  // else.
  EXPECT_EQ(0, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());
  EXPECT_EQ(kNoActions, test_delegate_->GetActions());

  // There should be nothing in the message loop.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kNoActions, test_delegate_->GetActions());

  // Nothing happens on resume.
  power_manager_client_->SendSuspendDone();
  EXPECT_EQ(kNoActions, test_delegate_->GetActions());
}

// Tests that the RendererFreezer does nothing on resume if the freezing
// operation was unsuccessful.
TEST_F(RendererFreezerTest, ErrorFreezingRenderers) {
  Init();
  test_delegate_->set_freeze_renderers_result(false);

  power_manager_client_->SendSuspendImminent();
  EXPECT_EQ(1, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());

  // The freezing operation should fail, but we should still report readiness.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kFreezeRenderers, test_delegate_->GetActions());
  EXPECT_EQ(0, power_manager_client_->GetNumPendingSuspendReadinessCallbacks());

  // Since the delegate reported that the freezing was unsuccessful, don't do
  // anything on resume.
  power_manager_client_->SendSuspendDone();
  EXPECT_EQ(kNoActions, test_delegate_->GetActions());
}

#if defined(GTEST_HAS_DEATH_TEST)
// Tests that the RendererFreezer crashes the browser if the freezing operation
// was successful but the thawing operation failed.
TEST_F(RendererFreezerTest, ErrorThawingRenderers) {
  Init();
  test_delegate_->set_thaw_renderers_result(false);

  power_manager_client_->SendSuspendImminent();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kFreezeRenderers, test_delegate_->GetActions());

  EXPECT_DEATH(power_manager_client_->SendSuspendDone(), "Unable to thaw");
}
#endif  // GTEST_HAS_DEATH_TEST

}  // namespace chromeos
