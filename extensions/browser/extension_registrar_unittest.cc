// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_registrar.h"

#include <memory>

#include "base/location.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_notification_tracker.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

using testing::Return;
using testing::_;

class TestExtensionSystem : public MockExtensionSystem {
 public:
  explicit TestExtensionSystem(content::BrowserContext* context)
      : MockExtensionSystem(context),
        runtime_data_(ExtensionRegistry::Get(context)) {}

  ~TestExtensionSystem() override {}

  // MockExtensionSystem:
  void RegisterExtensionWithRequestContexts(
      const Extension* extension,
      const base::Closure& callback) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
  }
  RuntimeData* runtime_data() override { return &runtime_data_; }

 private:
  RuntimeData runtime_data_;
  DISALLOW_COPY_AND_ASSIGN(TestExtensionSystem);
};

class TestExtensionRegistrarDelegate : public ExtensionRegistrar::Delegate {
 public:
  TestExtensionRegistrarDelegate() = default;
  ~TestExtensionRegistrarDelegate() override = default;

  // ExtensionRegistrar::Delegate:
  MOCK_METHOD2(PostActivateExtension,
               void(scoped_refptr<const Extension> extension,
                    bool is_newly_added));
  MOCK_METHOD1(PostDeactivateExtension,
               void(scoped_refptr<const Extension> extension));
  MOCK_METHOD1(CanEnableExtension, bool(const Extension* extension));
  MOCK_METHOD1(CanDisableExtension, bool(const Extension* extension));
  MOCK_METHOD1(ShouldBlockExtension, bool(const Extension* extension));

 private:
  DISALLOW_COPY_AND_ASSIGN(TestExtensionRegistrarDelegate);
};

}  // namespace

class ExtensionRegistrarTest : public ExtensionsTest {
 public:
  ExtensionRegistrarTest()
      : ExtensionsTest(std::make_unique<content::TestBrowserThreadBundle>()) {}
  ~ExtensionRegistrarTest() override = default;

  void SetUp() override {
    ExtensionsTest::SetUp();
    extensions_browser_client()->set_extension_system_factory(&factory_);
    extension_ = ExtensionBuilder("extension").Build();
    registrar_.emplace(browser_context(), delegate());

    notification_tracker_.ListenFor(
        extensions::NOTIFICATION_EXTENSION_UPDATE_DISABLED,
        content::Source<content::BrowserContext>(browser_context()));
    notification_tracker_.ListenFor(
        extensions::NOTIFICATION_EXTENSION_REMOVED,
        content::Source<content::BrowserContext>(browser_context()));

    // Mock defaults.
    ON_CALL(delegate_, CanEnableExtension(extension_.get()))
        .WillByDefault(Return(true));
    ON_CALL(delegate_, CanDisableExtension(extension_.get()))
        .WillByDefault(Return(true));
    ON_CALL(delegate_, ShouldBlockExtension(extension_.get()))
        .WillByDefault(Return(false));
    EXPECT_CALL(delegate_, PostActivateExtension(_, _)).Times(0);
    EXPECT_CALL(delegate_, PostDeactivateExtension(_)).Times(0);
  }

 protected:
  // Boilerplate to verify the mock's expected calls. With a SCOPED_TRACE at the
  // call site, this includes the caller's function in the Gtest trace on
  // failure. Otherwise, the failures are unhelpfully listed at the end of the
  // test.
  void VerifyMock() {
    EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(&delegate_));

    // Re-add the expectations for functions that must not be called.
    EXPECT_CALL(delegate_, PostActivateExtension(_, _)).Times(0);
    EXPECT_CALL(delegate_, PostDeactivateExtension(_)).Times(0);
  }

  // Adds the extension as enabled and verifies the result.
  void AddEnabledExtension() {
    SCOPED_TRACE("AddEnabledExtension");
    ExtensionRegistry* extension_registry =
        ExtensionRegistry::Get(browser_context());

    EXPECT_CALL(delegate_, PostActivateExtension(extension_, true));
    registrar_->AddExtension(extension_);
    ExpectInSet(ExtensionRegistry::ENABLED);
    EXPECT_FALSE(IsExtensionReady());

    TestExtensionRegistryObserver(extension_registry).WaitForExtensionReady();
    EXPECT_TRUE(IsExtensionReady());

    VerifyMock();
  }

  // Adds the extension as disabled and verifies the result.
  void AddDisabledExtension() {
    SCOPED_TRACE("AddDisabledExtension");
    ExtensionPrefs::Get(browser_context())
        ->SetExtensionDisabled(extension_->id(),
                               disable_reason::DISABLE_USER_ACTION);
    registrar_->AddExtension(extension_);
    ExpectInSet(ExtensionRegistry::DISABLED);
    EXPECT_FALSE(IsExtensionReady());
    EXPECT_TRUE(notification_tracker_.Check1AndReset(
        extensions::NOTIFICATION_EXTENSION_UPDATE_DISABLED));
  }

  // Adds the extension as blacklisted and verifies the result.
  void AddBlacklistedExtension() {
    SCOPED_TRACE("AddBlacklistedExtension");
    ExtensionPrefs::Get(browser_context())
        ->SetExtensionBlacklistState(extension_->id(), BLACKLISTED_MALWARE);
    registrar_->AddExtension(extension_);
    ExpectInSet(ExtensionRegistry::BLACKLISTED);
    EXPECT_FALSE(IsExtensionReady());
    EXPECT_EQ(0u, notification_tracker_.size());
  }

  // Adds the extension as blocked and verifies the result.
  void AddBlockedExtension() {
    SCOPED_TRACE("AddBlockedExtension");
    registrar_->AddExtension(extension_);
    ExpectInSet(ExtensionRegistry::BLOCKED);
    EXPECT_FALSE(IsExtensionReady());
    EXPECT_EQ(0u, notification_tracker_.size());
  }

  // Removes an enabled extension and verifies the result.
  void RemoveEnabledExtension() {
    SCOPED_TRACE("RemoveEnabledExtension");
    // Calling RemoveExtension removes its entry from the enabled list and
    // removes the extension.
    EXPECT_CALL(delegate_, PostDeactivateExtension(extension_));
    registrar_->RemoveExtension(extension_->id(),
                                UnloadedExtensionReason::UNINSTALL);
    ExpectInSet(ExtensionRegistry::NONE);

    // Removing an enabled extension should trigger a notification.
    EXPECT_TRUE(notification_tracker_.Check1AndReset(
        extensions::NOTIFICATION_EXTENSION_REMOVED));

    VerifyMock();
  }

  // Removes a disabled extension and verifies the result.
  void RemoveDisabledExtension() {
    SCOPED_TRACE("RemoveDisabledExtension");
    // Calling RemoveExtension removes its entry from the disabled list and
    // removes the extension.
    registrar_->RemoveExtension(extension_->id(),
                                UnloadedExtensionReason::UNINSTALL);
    ExpectInSet(ExtensionRegistry::NONE);

    // Removing a disabled extension should trigger a notification.
    EXPECT_TRUE(notification_tracker_.Check1AndReset(
        extensions::NOTIFICATION_EXTENSION_REMOVED));
  }

  // Removes a blacklisted extension and verifies the result.
  void RemoveBlacklistedExtension() {
    SCOPED_TRACE("RemoveBlacklistedExtension");
    // Calling RemoveExtension removes the extension.
    // TODO(michaelpg): Blacklisted extensions shouldn't need to be
    // "deactivated". See crbug.com/708230.
    EXPECT_CALL(delegate_, PostDeactivateExtension(extension_));
    registrar_->RemoveExtension(extension_->id(),
                                UnloadedExtensionReason::UNINSTALL);

    // RemoveExtension does not un-blacklist the extension.
    ExpectInSet(ExtensionRegistry::BLACKLISTED);

    // Removing a blacklisted extension should trigger a notification.
    EXPECT_TRUE(notification_tracker_.Check1AndReset(
        extensions::NOTIFICATION_EXTENSION_REMOVED));

    VerifyMock();
  }

  // Removes a blocked extension and verifies the result.
  void RemoveBlockedExtension() {
    SCOPED_TRACE("RemoveBlockedExtension");
    // Calling RemoveExtension removes the extension.
    // TODO(michaelpg): Blocked extensions shouldn't need to be
    // "deactivated". See crbug.com/708230.
    EXPECT_CALL(delegate_, PostDeactivateExtension(extension_));
    registrar_->RemoveExtension(extension_->id(),
                                UnloadedExtensionReason::UNINSTALL);

    // RemoveExtension does not un-block the extension.
    ExpectInSet(ExtensionRegistry::BLOCKED);

    // Removing a blocked extension should trigger a notification.
    EXPECT_TRUE(notification_tracker_.Check1AndReset(
        extensions::NOTIFICATION_EXTENSION_REMOVED));

    VerifyMock();
  }

  void EnableExtension() {
    SCOPED_TRACE("EnableExtension");
    ExtensionRegistry* extension_registry =
        ExtensionRegistry::Get(browser_context());

    EXPECT_CALL(delegate_, PostActivateExtension(extension_, false));
    registrar_->EnableExtension(extension_->id());
    ExpectInSet(ExtensionRegistry::ENABLED);
    EXPECT_FALSE(IsExtensionReady());

    TestExtensionRegistryObserver(extension_registry).WaitForExtensionReady();
    ExpectInSet(ExtensionRegistry::ENABLED);
    EXPECT_TRUE(IsExtensionReady());

    VerifyMock();
  }

  void DisableExtension() {
    SCOPED_TRACE("DisableExtension");
    EXPECT_CALL(delegate_, PostDeactivateExtension(extension_));
    registrar_->DisableExtension(extension_->id(),
                                 disable_reason::DISABLE_USER_ACTION);
    ExpectInSet(ExtensionRegistry::DISABLED);
    EXPECT_FALSE(IsExtensionReady());

    VerifyMock();
  }

  // Verifies that the extension is in the given set in the ExtensionRegistry
  // and not in other sets.
  void ExpectInSet(ExtensionRegistry::IncludeFlag set_id) {
    ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());

    EXPECT_EQ(set_id == ExtensionRegistry::ENABLED,
              registry->enabled_extensions().Contains(extension_->id()));

    EXPECT_EQ(set_id == ExtensionRegistry::DISABLED,
              registry->disabled_extensions().Contains(extension_->id()));

    EXPECT_EQ(set_id == ExtensionRegistry::TERMINATED,
              registry->terminated_extensions().Contains(extension_->id()));

    EXPECT_EQ(set_id == ExtensionRegistry::BLACKLISTED,
              registry->blacklisted_extensions().Contains(extension_->id()));

    EXPECT_EQ(set_id == ExtensionRegistry::BLOCKED,
              registry->blocked_extensions().Contains(extension_->id()));
  }

  bool IsExtensionReady() {
    return ExtensionRegistry::Get(browser_context())
        ->ready_extensions()
        .Contains(extension_->id());
  }

  ExtensionRegistrar* registrar() { return &registrar_.value(); }
  TestExtensionRegistrarDelegate* delegate() { return &delegate_; }

  scoped_refptr<const Extension> extension() const { return extension_; }

 private:
  MockExtensionSystemFactory<TestExtensionSystem> factory_;
  // Use NiceMock to allow uninteresting calls, so the delegate can be queried
  // any number of times. We will explicitly disallow unexpected calls to
  // PostActivateExtension/PostDeactivateExtension with EXPECT_CALL statements.
  testing::NiceMock<TestExtensionRegistrarDelegate> delegate_;
  scoped_refptr<const Extension> extension_;

  content::TestNotificationTracker notification_tracker_;

  // Initialized in SetUp().
  base::Optional<ExtensionRegistrar> registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionRegistrarTest);
};

TEST_F(ExtensionRegistrarTest, Basic) {
  AddEnabledExtension();
  RemoveEnabledExtension();
}

TEST_F(ExtensionRegistrarTest, AlreadyEnabled) {
  AddEnabledExtension();

  // As the extension is already enabled, this is a no-op.
  registrar()->EnableExtension(extension()->id());
  ExpectInSet(ExtensionRegistry::ENABLED);
  EXPECT_TRUE(IsExtensionReady());

  RemoveEnabledExtension();
}

TEST_F(ExtensionRegistrarTest, Disable) {
  AddEnabledExtension();

  // Disable the extension before removing it.
  DisableExtension();
  RemoveDisabledExtension();
}

TEST_F(ExtensionRegistrarTest, DisableAndEnable) {
  AddEnabledExtension();

  // Disable then enable the extension.
  DisableExtension();
  EnableExtension();

  RemoveEnabledExtension();
}

TEST_F(ExtensionRegistrarTest, AddDisabled) {
  // An extension can be added as disabled, then removed.
  AddDisabledExtension();
  RemoveDisabledExtension();

  // An extension can be added as disabled, then enabled.
  AddDisabledExtension();
  EnableExtension();
  RemoveEnabledExtension();
}

TEST_F(ExtensionRegistrarTest, AddForceEnabled) {
  // Prevent the extension from being disabled.
  ON_CALL(*delegate(), CanDisableExtension(extension().get()))
      .WillByDefault(Return(false));
  AddEnabledExtension();

  // Extension cannot be disabled.
  registrar()->DisableExtension(extension()->id(),
                                disable_reason::DISABLE_USER_ACTION);
  ExpectInSet(ExtensionRegistry::ENABLED);
}

TEST_F(ExtensionRegistrarTest, AddForceDisabled) {
  // Prevent the extension from being enabled.
  ON_CALL(*delegate(), CanEnableExtension(extension().get()))
      .WillByDefault(Return(false));
  AddDisabledExtension();

  // Extension cannot be enabled.
  registrar()->EnableExtension(extension()->id());
  ExpectInSet(ExtensionRegistry::DISABLED);
}

TEST_F(ExtensionRegistrarTest, AddBlacklisted) {
  AddBlacklistedExtension();

  // A blacklisted extension cannot be enabled/disabled.
  registrar()->EnableExtension(extension()->id());
  ExpectInSet(ExtensionRegistry::BLACKLISTED);
  registrar()->DisableExtension(extension()->id(),
                                disable_reason::DISABLE_USER_ACTION);
  ExpectInSet(ExtensionRegistry::BLACKLISTED);

  RemoveBlacklistedExtension();
}

TEST_F(ExtensionRegistrarTest, AddBlocked) {
  // Block extensions.
  ON_CALL(*delegate(), ShouldBlockExtension(extension().get()))
      .WillByDefault(Return(true));

  // A blocked extension can be added.
  AddBlockedExtension();

  // Extension cannot be enabled/disabled.
  registrar()->EnableExtension(extension()->id());
  ExpectInSet(ExtensionRegistry::BLOCKED);
  registrar()->DisableExtension(extension()->id(),
                                disable_reason::DISABLE_USER_ACTION);
  ExpectInSet(ExtensionRegistry::BLOCKED);

  RemoveBlockedExtension();
}

}  // namespace extensions
