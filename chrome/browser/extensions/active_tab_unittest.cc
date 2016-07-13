// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/value_builder.h"

using base::DictionaryValue;
using base::ListValue;
using content::BrowserThread;
using content::NavigationController;

namespace extensions {
namespace {

scoped_refptr<const Extension> CreateTestExtension(
    const std::string& id,
    bool has_active_tab_permission,
    bool has_tab_capture_permission) {
  ListBuilder permissions;
  if (has_active_tab_permission)
    permissions.Append("activeTab");
  if (has_tab_capture_permission)
    permissions.Append("tabCapture");
  return ExtensionBuilder()
      .SetManifest(DictionaryBuilder()
                       .Set("name", "Extension with ID " + id)
                       .Set("version", "1.0")
                       .Set("manifest_version", 2)
                       .Set("permissions", permissions.Build())
                       .Build())
      .SetID(id)
      .Build();
}

enum PermittedFeature {
  PERMITTED_NONE,
  PERMITTED_SCRIPT_ONLY,
  PERMITTED_CAPTURE_ONLY,
  PERMITTED_BOTH
};

class ActiveTabTest : public ChromeRenderViewHostTestHarness {
 protected:
  ActiveTabTest()
      : current_channel(version_info::Channel::DEV),
        extension(CreateTestExtension("deadbeef", true, false)),
        another_extension(CreateTestExtension("feedbeef", true, false)),
        extension_without_active_tab(CreateTestExtension("badbeef",
                                                         false,
                                                         false)),
        extension_with_tab_capture(CreateTestExtension("cafebeef",
                                                       true,
                                                       true)) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    TabHelper::CreateForWebContents(web_contents());
  }

  int tab_id() {
    return SessionTabHelper::IdForTab(web_contents());
  }

  ActiveTabPermissionGranter* active_tab_permission_granter() {
    return extensions::TabHelper::FromWebContents(web_contents())->
        active_tab_permission_granter();
  }

  bool IsAllowed(const scoped_refptr<const Extension>& extension,
                 const GURL& url) {
    return IsAllowed(extension, url, PERMITTED_BOTH, tab_id());
  }

  bool IsAllowed(const scoped_refptr<const Extension>& extension,
                 const GURL& url,
                 PermittedFeature feature) {
    return IsAllowed(extension, url, feature, tab_id());
  }

  bool IsAllowed(const scoped_refptr<const Extension>& extension,
                 const GURL& url,
                 PermittedFeature feature,
                 int tab_id) {
    const PermissionsData* permissions_data = extension->permissions_data();
    bool script =
        permissions_data->CanAccessPage(extension.get(), url, tab_id, nullptr);
    bool capture = HasTabsPermission(extension, tab_id) &&
                   permissions_data->CanCaptureVisiblePage(tab_id, NULL);
    switch (feature) {
      case PERMITTED_SCRIPT_ONLY:
        return script && !capture;
      case PERMITTED_CAPTURE_ONLY:
        return capture && !script;
      case PERMITTED_BOTH:
        return script && capture;
      case PERMITTED_NONE:
        return !script && !capture;
    }
    NOTREACHED();
    return false;
  }

  bool IsBlocked(const scoped_refptr<const Extension>& extension,
                 const GURL& url) {
    return IsBlocked(extension, url, tab_id());
  }

  bool IsBlocked(const scoped_refptr<const Extension>& extension,
                 const GURL& url,
                 int tab_id) {
    return IsAllowed(extension, url, PERMITTED_NONE, tab_id);
  }

  bool HasTabsPermission(const scoped_refptr<const Extension>& extension) {
    return HasTabsPermission(extension, tab_id());
  }

  bool HasTabsPermission(const scoped_refptr<const Extension>& extension,
                         int tab_id) {
    return extension->permissions_data()->HasAPIPermissionForTab(
        tab_id, APIPermission::kTab);
  }

  bool IsGrantedForTab(const Extension* extension,
                       const content::WebContents* web_contents) {
    return extension->permissions_data()->HasAPIPermissionForTab(
        SessionTabHelper::IdForTab(web_contents), APIPermission::kTab);
  }

  // TODO(justinlin): Remove when tabCapture is moved to stable.
  ScopedCurrentChannel current_channel;

  // An extension with the activeTab permission.
  scoped_refptr<const Extension> extension;

  // Another extension with activeTab (for good measure).
  scoped_refptr<const Extension> another_extension;

  // An extension without the activeTab permission.
  scoped_refptr<const Extension> extension_without_active_tab;

  // An extension with both the activeTab and tabCapture permission.
  scoped_refptr<const Extension> extension_with_tab_capture;
};

TEST_F(ActiveTabTest, GrantToSinglePage) {
  GURL google("http://www.google.com");
  NavigateAndCommit(google);

  // No access unless it's been granted.
  EXPECT_TRUE(IsBlocked(extension, google));
  EXPECT_TRUE(IsBlocked(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  EXPECT_FALSE(HasTabsPermission(extension));
  EXPECT_FALSE(HasTabsPermission(another_extension));
  EXPECT_FALSE(HasTabsPermission(extension_without_active_tab));

  active_tab_permission_granter()->GrantIfRequested(extension.get());
  active_tab_permission_granter()->GrantIfRequested(
      extension_without_active_tab.get());

  // Granted to extension and extension_without_active_tab, but the latter
  // doesn't have the activeTab permission so not granted.
  EXPECT_TRUE(IsAllowed(extension, google));
  EXPECT_TRUE(IsBlocked(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  // Other subdomains shouldn't be given access.
  GURL mail_google("http://mail.google.com");
  EXPECT_TRUE(IsAllowed(extension, mail_google, PERMITTED_CAPTURE_ONLY));
  EXPECT_TRUE(IsBlocked(another_extension, mail_google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, mail_google));

  // Reloading the page should clear the active permissions.
  Reload();

  EXPECT_TRUE(IsBlocked(extension, google));
  EXPECT_TRUE(IsBlocked(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  EXPECT_FALSE(HasTabsPermission(extension));
  EXPECT_FALSE(HasTabsPermission(another_extension));
  EXPECT_FALSE(HasTabsPermission(extension_without_active_tab));

  // But they should still be able to be granted again.
  active_tab_permission_granter()->GrantIfRequested(extension.get());

  EXPECT_TRUE(IsAllowed(extension, google));
  EXPECT_TRUE(IsBlocked(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  // And grant a few more times redundantly for good measure.
  active_tab_permission_granter()->GrantIfRequested(extension.get());
  active_tab_permission_granter()->GrantIfRequested(extension.get());
  active_tab_permission_granter()->GrantIfRequested(another_extension.get());
  active_tab_permission_granter()->GrantIfRequested(another_extension.get());
  active_tab_permission_granter()->GrantIfRequested(another_extension.get());
  active_tab_permission_granter()->GrantIfRequested(extension.get());
  active_tab_permission_granter()->GrantIfRequested(extension.get());
  active_tab_permission_granter()->GrantIfRequested(another_extension.get());
  active_tab_permission_granter()->GrantIfRequested(another_extension.get());

  EXPECT_TRUE(IsAllowed(extension, google));
  EXPECT_TRUE(IsAllowed(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  // Navigating to a new URL should clear the active permissions.
  GURL chromium("http://www.chromium.org");
  NavigateAndCommit(chromium);

  EXPECT_TRUE(IsBlocked(extension, google));
  EXPECT_TRUE(IsBlocked(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  EXPECT_TRUE(IsBlocked(extension, chromium));
  EXPECT_TRUE(IsBlocked(another_extension, chromium));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, chromium));

  EXPECT_FALSE(HasTabsPermission(extension));
  EXPECT_FALSE(HasTabsPermission(another_extension));
  EXPECT_FALSE(HasTabsPermission(extension_without_active_tab));

  // Should be able to grant to multiple extensions at the same time (if they
  // have the activeTab permission, of course).
  active_tab_permission_granter()->GrantIfRequested(extension.get());
  active_tab_permission_granter()->GrantIfRequested(another_extension.get());
  active_tab_permission_granter()->GrantIfRequested(
      extension_without_active_tab.get());

  EXPECT_TRUE(IsAllowed(extension, google, PERMITTED_CAPTURE_ONLY));
  EXPECT_TRUE(IsAllowed(another_extension, google, PERMITTED_CAPTURE_ONLY));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  EXPECT_TRUE(IsAllowed(extension, chromium));
  EXPECT_TRUE(IsAllowed(another_extension, chromium));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, chromium));

  // Should be able to go back to URLs that were previously cleared.
  NavigateAndCommit(google);

  active_tab_permission_granter()->GrantIfRequested(extension.get());
  active_tab_permission_granter()->GrantIfRequested(another_extension.get());
  active_tab_permission_granter()->GrantIfRequested(
      extension_without_active_tab.get());

  EXPECT_TRUE(IsAllowed(extension, google));
  EXPECT_TRUE(IsAllowed(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  EXPECT_TRUE(IsAllowed(extension, chromium, PERMITTED_CAPTURE_ONLY));
  EXPECT_TRUE(IsAllowed(another_extension, chromium, PERMITTED_CAPTURE_ONLY));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, chromium));
}

TEST_F(ActiveTabTest, Uninstalling) {
  // Some semi-arbitrary setup.
  GURL google("http://www.google.com");
  NavigateAndCommit(google);

  active_tab_permission_granter()->GrantIfRequested(extension.get());

  EXPECT_TRUE(IsGrantedForTab(extension.get(), web_contents()));
  EXPECT_TRUE(IsAllowed(extension, google));

  // Uninstalling the extension should clear its tab permissions.
  ExtensionRegistry* registry =
      ExtensionRegistry::Get(web_contents()->GetBrowserContext());
  registry->TriggerOnUnloaded(extension.get(),
                              UnloadedExtensionInfo::REASON_DISABLE);

  // Note: can't EXPECT_FALSE(IsAllowed) here because uninstalled extensions
  // are just that... considered to be uninstalled, and the manager might
  // just ignore them from here on.

  // Granting the extension again should give them back.
  active_tab_permission_granter()->GrantIfRequested(extension.get());

  EXPECT_TRUE(IsGrantedForTab(extension.get(), web_contents()));
  EXPECT_TRUE(IsAllowed(extension, google));
}

TEST_F(ActiveTabTest, OnlyActiveTab) {
  GURL google("http://www.google.com");
  NavigateAndCommit(google);

  active_tab_permission_granter()->GrantIfRequested(extension.get());

  EXPECT_TRUE(IsAllowed(extension, google, PERMITTED_BOTH, tab_id()));
  EXPECT_TRUE(IsBlocked(extension, google, tab_id() + 1));
  EXPECT_FALSE(HasTabsPermission(extension, tab_id() + 1));
}

TEST_F(ActiveTabTest, NavigateInPage) {
  GURL google("http://www.google.com");
  NavigateAndCommit(google);

  active_tab_permission_granter()->GrantIfRequested(extension.get());

  // Perform an in-page navigation. The extension should not lose the temporary
  // permission.
  GURL google_h1("http://www.google.com#h1");
  NavigateAndCommit(google_h1);

  EXPECT_TRUE(IsAllowed(extension, google));
  EXPECT_TRUE(IsAllowed(extension, google_h1));

  GURL chromium("http://www.chromium.org");
  NavigateAndCommit(chromium);

  EXPECT_FALSE(IsAllowed(extension, google));
  EXPECT_FALSE(IsAllowed(extension, google_h1));
  EXPECT_FALSE(IsAllowed(extension, chromium));

  active_tab_permission_granter()->GrantIfRequested(extension.get());

  EXPECT_FALSE(IsAllowed(extension, google));
  EXPECT_FALSE(IsAllowed(extension, google_h1));
  EXPECT_TRUE(IsAllowed(extension, chromium));

  GURL chromium_h1("http://www.chromium.org#h1");
  NavigateAndCommit(chromium_h1);

  EXPECT_FALSE(IsAllowed(extension, google));
  EXPECT_FALSE(IsAllowed(extension, google_h1));
  EXPECT_TRUE(IsAllowed(extension, chromium));
  EXPECT_TRUE(IsAllowed(extension, chromium_h1));

  Reload();

  EXPECT_FALSE(IsAllowed(extension, google));
  EXPECT_FALSE(IsAllowed(extension, google_h1));
  EXPECT_FALSE(IsAllowed(extension, chromium));
  EXPECT_FALSE(IsAllowed(extension, chromium_h1));
}

TEST_F(ActiveTabTest, ChromeUrlGrants) {
  GURL internal("chrome://version");
  NavigateAndCommit(internal);
  active_tab_permission_granter()->GrantIfRequested(
      extension_with_tab_capture.get());
  // Do not grant tabs/hosts permissions for tab.
  EXPECT_TRUE(IsAllowed(extension_with_tab_capture, internal,
                        PERMITTED_CAPTURE_ONLY));
  const PermissionsData* permissions_data =
      extension_with_tab_capture->permissions_data();
  EXPECT_TRUE(permissions_data->HasAPIPermissionForTab(
      tab_id(), APIPermission::kTabCaptureForTab));

  EXPECT_TRUE(IsBlocked(extension_with_tab_capture, internal, tab_id() + 1));
  EXPECT_FALSE(permissions_data->HasAPIPermissionForTab(
      tab_id() + 1, APIPermission::kTabCaptureForTab));
}

}  // namespace
}  // namespace extensions
