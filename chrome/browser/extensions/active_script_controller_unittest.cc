// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>
#include <utility>

#include "base/values.h"
#include "chrome/browser/extensions/active_script_controller.h"
#include "chrome/browser/extensions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_sync_service_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest.h"
#include "extensions/common/user_script.h"
#include "extensions/common/value_builder.h"

namespace extensions {

namespace {

const char kAllHostsPermission[] = "*://*/*";

}  // namespace

// Unittests for the ActiveScriptController mostly test the internal logic
// of the controller itself (when to allow/deny extension script injection).
// Testing real injection is allowed/denied as expected (i.e., that the
// ActiveScriptController correctly interfaces in the system) is done in the
// ActiveScriptControllerBrowserTests.
class ActiveScriptControllerUnitTest : public ChromeRenderViewHostTestHarness {
 protected:
  ActiveScriptControllerUnitTest();
  ~ActiveScriptControllerUnitTest() override;

  // Creates an extension with all hosts permission and adds it to the registry.
  const Extension* AddExtension();

  // Reloads |extension_| by removing it from the registry and recreating it.
  const Extension* ReloadExtension();

  // Returns true if the |extension| requires user consent before injecting
  // a script.
  bool RequiresUserConsent(const Extension* extension) const;

  // Request an injection for the given |extension|.
  void RequestInjection(const Extension* extension);
  void RequestInjection(const Extension* extension,
                        UserScript::RunLocation run_location);

  // Returns the number of times a given extension has had a script execute.
  size_t GetExecutionCountForExtension(const std::string& extension_id) const;

  ActiveScriptController* controller() const {
    return active_script_controller_;
  }

 private:
  // Returns a closure to use as a script execution for a given extension.
  base::Closure GetExecutionCallbackForExtension(
      const std::string& extension_id);

  // Increment the number of executions for the given |extension_id|.
  void IncrementExecutionCount(const std::string& extension_id);

  void SetUp() override;

  // Since ActiveScriptController's behavior is behind a flag, override the
  // feature switch.
  FeatureSwitch::ScopedOverride feature_override_;

  // The associated ActiveScriptController.
  ActiveScriptController* active_script_controller_;

  // The map of observed executions, keyed by extension id.
  std::map<std::string, int> extension_executions_;

  scoped_refptr<const Extension> extension_;

  DISALLOW_COPY_AND_ASSIGN(ActiveScriptControllerUnitTest);
};

ActiveScriptControllerUnitTest::ActiveScriptControllerUnitTest()
    : feature_override_(FeatureSwitch::scripts_require_action(),
                        FeatureSwitch::OVERRIDE_ENABLED),
      active_script_controller_(nullptr) {}

ActiveScriptControllerUnitTest::~ActiveScriptControllerUnitTest() {
}

const Extension* ActiveScriptControllerUnitTest::AddExtension() {
  const std::string kId = crx_file::id_util::GenerateId("all_hosts_extension");
  extension_ =
      ExtensionBuilder()
          .SetManifest(std::move(
              DictionaryBuilder()
                  .Set("name", "all_hosts_extension")
                  .Set("description", "an extension")
                  .Set("manifest_version", 2)
                  .Set("version", "1.0.0")
                  .Set("permissions",
                       std::move(ListBuilder().Append(kAllHostsPermission)))))
          .SetLocation(Manifest::INTERNAL)
          .SetID(kId)
          .Build();

  ExtensionRegistry::Get(profile())->AddEnabled(extension_);
  PermissionsUpdater(profile()).InitializePermissions(extension_.get());
  return extension_.get();
}

const Extension* ActiveScriptControllerUnitTest::ReloadExtension() {
  ExtensionRegistry::Get(profile())->RemoveEnabled(extension_->id());
  return AddExtension();
}

bool ActiveScriptControllerUnitTest::RequiresUserConsent(
    const Extension* extension) const {
  PermissionsData::AccessType access_type =
      controller()->RequiresUserConsentForScriptInjectionForTesting(
          extension, UserScript::PROGRAMMATIC_SCRIPT);
  // We should never downright refuse access in these tests.
  DCHECK_NE(PermissionsData::ACCESS_DENIED, access_type);
  return access_type == PermissionsData::ACCESS_WITHHELD;
}

void ActiveScriptControllerUnitTest::RequestInjection(
    const Extension* extension) {
  RequestInjection(extension, UserScript::DOCUMENT_IDLE);
}

void ActiveScriptControllerUnitTest::RequestInjection(
    const Extension* extension,
    UserScript::RunLocation run_location) {
  controller()->RequestScriptInjectionForTesting(
      extension, run_location,
      GetExecutionCallbackForExtension(extension->id()));
}

size_t ActiveScriptControllerUnitTest::GetExecutionCountForExtension(
    const std::string& extension_id) const {
  std::map<std::string, int>::const_iterator iter =
      extension_executions_.find(extension_id);
  if (iter != extension_executions_.end())
    return iter->second;
  return 0u;
}

base::Closure ActiveScriptControllerUnitTest::GetExecutionCallbackForExtension(
    const std::string& extension_id) {
  // We use base unretained here, but if this ever gets executed outside of
  // this test's lifetime, we have a major problem anyway.
  return base::Bind(&ActiveScriptControllerUnitTest::IncrementExecutionCount,
                    base::Unretained(this),
                    extension_id);
}

void ActiveScriptControllerUnitTest::IncrementExecutionCount(
    const std::string& extension_id) {
  ++extension_executions_[extension_id];
}

void ActiveScriptControllerUnitTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  // Skip syncing for testing purposes.
  ExtensionSyncServiceFactory::GetInstance()->SetTestingFactory(profile(),
                                                                nullptr);

  TabHelper::CreateForWebContents(web_contents());
  TabHelper* tab_helper = TabHelper::FromWebContents(web_contents());
  // These should never be null.
  DCHECK(tab_helper);
  active_script_controller_ = tab_helper->active_script_controller();
  DCHECK(active_script_controller_);
}

// Test that extensions with all_hosts require permission to execute, and, once
// that permission is granted, do execute.
TEST_F(ActiveScriptControllerUnitTest, RequestPermissionAndExecute) {
  const Extension* extension = AddExtension();
  ASSERT_TRUE(extension);

  NavigateAndCommit(GURL("https://www.google.com"));

  // Ensure that there aren't any executions pending.
  ASSERT_EQ(0u, GetExecutionCountForExtension(extension->id()));
  ASSERT_FALSE(controller()->WantsToRun(extension));

  ExtensionActionAPI* extension_action_api =
      ExtensionActionAPI::Get(profile());
  ASSERT_FALSE(extension_action_api->HasBeenBlocked(extension, web_contents()));

  // Since the extension requests all_hosts, we should require user consent.
  EXPECT_TRUE(RequiresUserConsent(extension));

  // Request an injection. The extension should want to run, but should not have
  // executed.
  RequestInjection(extension);
  EXPECT_TRUE(controller()->WantsToRun(extension));
  EXPECT_TRUE(extension_action_api->HasBeenBlocked(extension, web_contents()));
  EXPECT_EQ(0u, GetExecutionCountForExtension(extension->id()));

  // Click to accept the extension executing.
  controller()->OnClicked(extension);

  // The extension should execute, and the extension shouldn't want to run.
  EXPECT_EQ(1u, GetExecutionCountForExtension(extension->id()));
  EXPECT_FALSE(controller()->WantsToRun(extension));
  EXPECT_FALSE(extension_action_api->HasBeenBlocked(extension, web_contents()));

  // Since we already executed on the given page, we shouldn't need permission
  // for a second time.
  EXPECT_FALSE(RequiresUserConsent(extension));

  // Reloading and same-origin navigations shouldn't clear those permissions,
  // and we shouldn't require user constent again.
  Reload();
  EXPECT_FALSE(RequiresUserConsent(extension));
  NavigateAndCommit(GURL("https://www.google.com/foo"));
  EXPECT_FALSE(RequiresUserConsent(extension));
  NavigateAndCommit(GURL("https://www.google.com/bar"));
  EXPECT_FALSE(RequiresUserConsent(extension));

  // Cross-origin navigations should clear permissions.
  NavigateAndCommit(GURL("https://otherdomain.google.com"));
  EXPECT_TRUE(RequiresUserConsent(extension));

  // Grant access.
  RequestInjection(extension);
  controller()->OnClicked(extension);
  EXPECT_EQ(2u, GetExecutionCountForExtension(extension->id()));
  EXPECT_FALSE(controller()->WantsToRun(extension));

  // Navigating to another site should also clear the permissions.
  NavigateAndCommit(GURL("https://www.foo.com"));
  EXPECT_TRUE(RequiresUserConsent(extension));
}

// Test that injections that are not executed by the time the user navigates are
// ignored and never execute.
TEST_F(ActiveScriptControllerUnitTest, PendingInjectionsRemovedAtNavigation) {
  const Extension* extension = AddExtension();
  ASSERT_TRUE(extension);

  NavigateAndCommit(GURL("https://www.google.com"));

  ASSERT_EQ(0u, GetExecutionCountForExtension(extension->id()));

  // Request an injection. The extension should want to run, but not execute.
  RequestInjection(extension);
  EXPECT_TRUE(controller()->WantsToRun(extension));
  EXPECT_EQ(0u, GetExecutionCountForExtension(extension->id()));

  // Reload. This should remove the pending injection, and we should not
  // execute anything.
  Reload();
  EXPECT_FALSE(controller()->WantsToRun(extension));
  EXPECT_EQ(0u, GetExecutionCountForExtension(extension->id()));

  // Request and accept a new injection.
  RequestInjection(extension);
  controller()->OnClicked(extension);

  // The extension should only have executed once, even though a grand total
  // of two executions were requested.
  EXPECT_EQ(1u, GetExecutionCountForExtension(extension->id()));
  EXPECT_FALSE(controller()->WantsToRun(extension));
}

// Test that queueing multiple pending injections, and then accepting, triggers
// them all.
TEST_F(ActiveScriptControllerUnitTest, MultiplePendingInjection) {
  const Extension* extension = AddExtension();
  ASSERT_TRUE(extension);
  NavigateAndCommit(GURL("https://www.google.com"));

  ASSERT_EQ(0u, GetExecutionCountForExtension(extension->id()));

  const size_t kNumInjections = 3u;
  // Queue multiple pending injections.
  for (size_t i = 0u; i < kNumInjections; ++i)
    RequestInjection(extension);

  EXPECT_EQ(0u, GetExecutionCountForExtension(extension->id()));

  controller()->OnClicked(extension);

  // All pending injections should have executed.
  EXPECT_EQ(kNumInjections, GetExecutionCountForExtension(extension->id()));
  EXPECT_FALSE(controller()->WantsToRun(extension));
}

TEST_F(ActiveScriptControllerUnitTest, ActiveScriptsUseActiveTabPermissions) {
  const Extension* extension = AddExtension();
  NavigateAndCommit(GURL("https://www.google.com"));

  ActiveTabPermissionGranter* active_tab_permission_granter =
      TabHelper::FromWebContents(web_contents())
          ->active_tab_permission_granter();
  ASSERT_TRUE(active_tab_permission_granter);
  // Grant the extension active tab permissions. This normally happens, e.g.,
  // if the user clicks on a browser action.
  active_tab_permission_granter->GrantIfRequested(extension);

  // Since we have active tab permissions, we shouldn't need user consent
  // anymore.
  EXPECT_FALSE(RequiresUserConsent(extension));

  // Reloading and other same-origin navigations maintain the permission to
  // execute.
  Reload();
  EXPECT_FALSE(RequiresUserConsent(extension));
  NavigateAndCommit(GURL("https://www.google.com/foo"));
  EXPECT_FALSE(RequiresUserConsent(extension));
  NavigateAndCommit(GURL("https://www.google.com/bar"));
  EXPECT_FALSE(RequiresUserConsent(extension));

  // Navigating to a different origin will require user consent again.
  NavigateAndCommit(GURL("https://yahoo.com"));
  EXPECT_TRUE(RequiresUserConsent(extension));

  // Back to the original origin should also re-require constent.
  NavigateAndCommit(GURL("https://www.google.com"));
  EXPECT_TRUE(RequiresUserConsent(extension));

  RequestInjection(extension);
  EXPECT_TRUE(controller()->WantsToRun(extension));
  EXPECT_EQ(0u, GetExecutionCountForExtension(extension->id()));

  // Grant active tab.
  active_tab_permission_granter->GrantIfRequested(extension);

  // The pending injections should have run since active tab permission was
  // granted.
  EXPECT_EQ(1u, GetExecutionCountForExtension(extension->id()));
  EXPECT_FALSE(controller()->WantsToRun(extension));
}

TEST_F(ActiveScriptControllerUnitTest, ActiveScriptsCanHaveAllUrlsPref) {
  const Extension* extension = AddExtension();
  ASSERT_TRUE(extension);

  NavigateAndCommit(GURL("https://www.google.com"));
  EXPECT_TRUE(RequiresUserConsent(extension));

  // Enable the extension on all urls.
  util::SetAllowedScriptingOnAllUrls(extension->id(), profile(), true);

  EXPECT_FALSE(RequiresUserConsent(extension));
  // This should carry across navigations, and websites.
  NavigateAndCommit(GURL("http://www.foo.com"));
  EXPECT_FALSE(RequiresUserConsent(extension));

  // Turning off the preference should have instant effect.
  util::SetAllowedScriptingOnAllUrls(extension->id(), profile(), false);
  EXPECT_TRUE(RequiresUserConsent(extension));

  // And should also persist across navigations and websites.
  NavigateAndCommit(GURL("http://www.bar.com"));
  EXPECT_TRUE(RequiresUserConsent(extension));
}

TEST_F(ActiveScriptControllerUnitTest, TestAlwaysRun) {
  const Extension* extension = AddExtension();
  ASSERT_TRUE(extension);

  NavigateAndCommit(GURL("https://www.google.com/?gws_rd=ssl"));

  // Ensure that there aren't any executions pending.
  ASSERT_EQ(0u, GetExecutionCountForExtension(extension->id()));
  ASSERT_FALSE(controller()->WantsToRun(extension));

  // Since the extension requests all_hosts, we should require user consent.
  EXPECT_TRUE(RequiresUserConsent(extension));

  // Request an injection. The extension should want to run, but not execute.
  RequestInjection(extension);
  EXPECT_TRUE(controller()->WantsToRun(extension));
  EXPECT_EQ(0u, GetExecutionCountForExtension(extension->id()));

  // Allow the extension to always run on this origin.
  ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.GrantHostPermission(web_contents()->GetLastCommittedURL());
  controller()->OnClicked(extension);

  // The extension should execute, and the extension shouldn't want to run.
  EXPECT_EQ(1u, GetExecutionCountForExtension(extension->id()));
  EXPECT_FALSE(controller()->WantsToRun(extension));

  // Since we already executed on the given page, we shouldn't need permission
  // for a second time.
  EXPECT_FALSE(RequiresUserConsent(extension));

  // Navigating to another site that hasn't been granted a persisted permission
  // should necessitate user consent.
  NavigateAndCommit(GURL("https://www.foo.com/bar"));
  EXPECT_TRUE(RequiresUserConsent(extension));

  // We shouldn't need user permission upon returning to the original origin.
  NavigateAndCommit(GURL("https://www.google.com/foo/bar"));
  EXPECT_FALSE(RequiresUserConsent(extension));

  // Reloading the extension should not clear any granted host permissions.
  extension = ReloadExtension();
  Reload();
  EXPECT_FALSE(RequiresUserConsent(extension));

  // Different host...
  NavigateAndCommit(GURL("https://www.foo.com/bar"));
  EXPECT_TRUE(RequiresUserConsent(extension));
  // Different scheme...
  NavigateAndCommit(GURL("http://www.google.com/foo/bar"));
  EXPECT_TRUE(RequiresUserConsent(extension));
  // Different subdomain...
  NavigateAndCommit(GURL("https://en.google.com/foo/bar"));
  EXPECT_TRUE(RequiresUserConsent(extension));
  // Only the "always run" origin should be allowed to run without user consent.
  NavigateAndCommit(GURL("https://www.google.com/foo/bar"));
  EXPECT_FALSE(RequiresUserConsent(extension));
}

TEST_F(ActiveScriptControllerUnitTest, TestDifferentScriptRunLocations) {
  const Extension* extension = AddExtension();
  ASSERT_TRUE(extension);

  NavigateAndCommit(GURL("https://www.foo.com"));

  EXPECT_EQ(BLOCKED_ACTION_NONE, controller()->GetBlockedActions(extension));

  RequestInjection(extension, UserScript::DOCUMENT_END);
  EXPECT_EQ(BLOCKED_ACTION_SCRIPT_OTHER,
            controller()->GetBlockedActions(extension));
  RequestInjection(extension, UserScript::DOCUMENT_IDLE);
  EXPECT_EQ(BLOCKED_ACTION_SCRIPT_OTHER,
            controller()->GetBlockedActions(extension));
  RequestInjection(extension, UserScript::DOCUMENT_START);
  EXPECT_EQ(BLOCKED_ACTION_SCRIPT_AT_START | BLOCKED_ACTION_SCRIPT_OTHER,
            controller()->GetBlockedActions(extension));

  controller()->OnClicked(extension);
  EXPECT_EQ(BLOCKED_ACTION_NONE, controller()->GetBlockedActions(extension));
}

TEST_F(ActiveScriptControllerUnitTest, TestWebRequestBlocked) {
  const Extension* extension = AddExtension();
  ASSERT_TRUE(extension);

  NavigateAndCommit(GURL("https://www.foo.com"));

  EXPECT_EQ(BLOCKED_ACTION_NONE, controller()->GetBlockedActions(extension));
  EXPECT_FALSE(controller()->WantsToRun(extension));

  controller()->OnWebRequestBlocked(extension);
  EXPECT_EQ(BLOCKED_ACTION_WEB_REQUEST,
            controller()->GetBlockedActions(extension));
  EXPECT_TRUE(controller()->WantsToRun(extension));

  RequestInjection(extension);
  EXPECT_EQ(BLOCKED_ACTION_WEB_REQUEST | BLOCKED_ACTION_SCRIPT_OTHER,
            controller()->GetBlockedActions(extension));
  EXPECT_TRUE(controller()->WantsToRun(extension));

  NavigateAndCommit(GURL("https://www.bar.com"));
  EXPECT_EQ(BLOCKED_ACTION_NONE, controller()->GetBlockedActions(extension));
  EXPECT_FALSE(controller()->WantsToRun(extension));
}

}  // namespace extensions
