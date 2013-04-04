// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/common/extensions/features/feature.h"
#include "chrome/common/extensions/permissions/chrome_api_permissions.h"
#include "chrome/common/extensions/permissions/scoped_testing_permissions_info.h"
#include "chrome/common/extensions/value_builder.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/test/test_browser_thread.h"

using base::DictionaryValue;
using base::ListValue;
using content::BrowserThread;
using content::NavigationController;

namespace extensions {
namespace {

scoped_refptr<const Extension> CreateTestExtension(
    const std::string& id,
    bool has_active_tab_permission) {
  ListBuilder permissions;
  if (has_active_tab_permission)
    permissions.Append("activeTab");
  return ExtensionBuilder()
      .SetManifest(DictionaryBuilder()
          .Set("name", "Extension with ID " + id)
          .Set("version", "1.0")
          .Set("manifest_version", 2)
          .Set("permissions", permissions))
      .SetID(id)
      .Build();
}

class ActiveTabTest : public ChromeRenderViewHostTestHarness {
 public:
  ActiveTabTest()
      : permissions_info_(ChromeAPIPermissions()),
        extension(CreateTestExtension("deadbeef", true)),
        another_extension(CreateTestExtension("feedbeef", true)),
        extension_without_active_tab(CreateTestExtension("badbeef", false)),
        ui_thread_(BrowserThread::UI, MessageLoop::current()) {}

 protected:
  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
    TabHelper::CreateForWebContents(web_contents());
  }

  int tab_id() {
    return SessionID::IdForTab(web_contents());
  }

  ActiveTabPermissionGranter* active_tab_permission_granter() {
    return extensions::TabHelper::FromWebContents(web_contents())->
        active_tab_permission_granter();
  }

  bool IsAllowed(const scoped_refptr<const Extension>& extension,
                 const GURL& url) {
    return IsAllowed(extension, url, tab_id());
  }

  bool IsAllowed(const scoped_refptr<const Extension>& extension,
                 const GURL& url,
                 int tab_id) {
    return extension->CanExecuteScriptOnPage(url, url, tab_id, NULL, NULL) &&
           extension->CanCaptureVisiblePage(url, tab_id, NULL) &&
           HasTabsPermission(extension, tab_id);
  }

  bool IsBlocked(const scoped_refptr<const Extension>& extension,
                 const GURL& url) {
    return IsBlocked(extension, url, tab_id());
  }

  bool IsBlocked(const scoped_refptr<const Extension>& extension,
                 const GURL& url,
                 int tab_id) {
    // Note: can't check HasTabsPermission because it isn't URL specific.
    return !extension->CanExecuteScriptOnPage(url, url, tab_id, NULL, NULL) &&
           !extension->CanCaptureVisiblePage(url, tab_id, NULL);
  }

  bool HasTabsPermission(const scoped_refptr<const Extension>& extension) {
    return HasTabsPermission(extension, tab_id());
  }

  bool HasTabsPermission(const scoped_refptr<const Extension>& extension,
                         int tab_id) {
    return extension->HasAPIPermissionForTab(tab_id, APIPermission::kTab);
  }

  ScopedTestingPermissionsInfo permissions_info_;

  // An extension with the activeTab permission.
  scoped_refptr<const Extension> extension;

  // Another extension with activeTab (for good measure).
  scoped_refptr<const Extension> another_extension;

  // An extension without the activeTab permission.
  scoped_refptr<const Extension> extension_without_active_tab;

 private:
  content::TestBrowserThread ui_thread_;
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

  active_tab_permission_granter()->GrantIfRequested(extension);
  active_tab_permission_granter()->GrantIfRequested(
      extension_without_active_tab);

  // Granted to extension and extension_without_active_tab, but the latter
  // doesn't have the activeTab permission so not granted.
  EXPECT_TRUE(IsAllowed(extension, google));
  EXPECT_TRUE(IsBlocked(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  // Other subdomains shouldn't be given access.
  GURL mail_google("http://mail.google.com");
  EXPECT_TRUE(IsBlocked(extension, mail_google));
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
  active_tab_permission_granter()->GrantIfRequested(extension);

  EXPECT_TRUE(IsAllowed(extension, google));
  EXPECT_TRUE(IsBlocked(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  // And grant a few more times redundantly for good measure.
  active_tab_permission_granter()->GrantIfRequested(extension);
  active_tab_permission_granter()->GrantIfRequested(extension);
  active_tab_permission_granter()->GrantIfRequested(another_extension);
  active_tab_permission_granter()->GrantIfRequested(another_extension);
  active_tab_permission_granter()->GrantIfRequested(another_extension);
  active_tab_permission_granter()->GrantIfRequested(extension);
  active_tab_permission_granter()->GrantIfRequested(extension);
  active_tab_permission_granter()->GrantIfRequested(another_extension);
  active_tab_permission_granter()->GrantIfRequested(another_extension);

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
  active_tab_permission_granter()->GrantIfRequested(extension);
  active_tab_permission_granter()->GrantIfRequested(another_extension);
  active_tab_permission_granter()->GrantIfRequested(
      extension_without_active_tab);

  EXPECT_TRUE(IsBlocked(extension, google));
  EXPECT_TRUE(IsBlocked(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  EXPECT_TRUE(IsAllowed(extension, chromium));
  EXPECT_TRUE(IsAllowed(another_extension, chromium));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, chromium));

  // Should be able to go back to URLs that were previously cleared.
  NavigateAndCommit(google);

  active_tab_permission_granter()->GrantIfRequested(extension);
  active_tab_permission_granter()->GrantIfRequested(another_extension);
  active_tab_permission_granter()->GrantIfRequested(
      extension_without_active_tab);

  EXPECT_TRUE(IsAllowed(extension, google));
  EXPECT_TRUE(IsAllowed(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  EXPECT_TRUE(IsBlocked(extension, chromium));
  EXPECT_TRUE(IsBlocked(another_extension, chromium));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, chromium));
};

TEST_F(ActiveTabTest, Uninstalling) {
  // Some semi-arbitrary setup.
  GURL google("http://www.google.com");
  NavigateAndCommit(google);

  active_tab_permission_granter()->GrantIfRequested(extension);

  EXPECT_TRUE(active_tab_permission_granter()->IsGranted(extension));
  EXPECT_TRUE(IsAllowed(extension, google));

  // Uninstalling the extension should clear its tab permissions.
  UnloadedExtensionInfo details(
      extension,
      extension_misc::UNLOAD_REASON_DISABLE);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_UNLOADED,
      content::Source<Profile>(Profile::FromBrowserContext(
          web_contents()->GetBrowserContext())),
      content::Details<UnloadedExtensionInfo>(&details));

  EXPECT_FALSE(active_tab_permission_granter()->IsGranted(extension));
  // Note: can't EXPECT_FALSE(IsAllowed) here because uninstalled extensions
  // are just that... considered to be uninstalled, and the manager might
  // just ignore them from here on.

  // Granting the extension again should give them back.
  active_tab_permission_granter()->GrantIfRequested(extension);

  EXPECT_TRUE(active_tab_permission_granter()->IsGranted(extension));
  EXPECT_TRUE(IsAllowed(extension, google));
}

TEST_F(ActiveTabTest, OnlyActiveTab) {
  GURL google("http://www.google.com");
  NavigateAndCommit(google);

  active_tab_permission_granter()->GrantIfRequested(extension);

  EXPECT_TRUE(IsAllowed(extension, google, tab_id()));
  EXPECT_TRUE(IsBlocked(extension, google, tab_id() + 1));
  EXPECT_FALSE(HasTabsPermission(extension, tab_id() + 1));
}

TEST_F(ActiveTabTest, NavigateInPage) {
  GURL google("http://www.google.com");
  NavigateAndCommit(google);

  active_tab_permission_granter()->GrantIfRequested(extension);

  // Perform an in-page navigation. The extension should not lose the temporary
  // permission.
  GURL google_h1("http://www.google.com#h1");
  NavigateAndCommit(google_h1);

  EXPECT_TRUE(IsAllowed(extension, google, tab_id()));
  EXPECT_TRUE(IsAllowed(extension, google_h1, tab_id()));

  GURL chromium("http://www.chromium.org");
  NavigateAndCommit(chromium);

  EXPECT_FALSE(IsAllowed(extension, google, tab_id()));
  EXPECT_FALSE(IsAllowed(extension, google_h1, tab_id()));
  EXPECT_FALSE(IsAllowed(extension, chromium, tab_id()));

  active_tab_permission_granter()->GrantIfRequested(extension);

  EXPECT_FALSE(IsAllowed(extension, google, tab_id()));
  EXPECT_FALSE(IsAllowed(extension, google_h1, tab_id()));
  EXPECT_TRUE(IsAllowed(extension, chromium, tab_id()));

  GURL chromium_h1("http://www.chromium.org#h1");
  NavigateAndCommit(chromium_h1);

  EXPECT_FALSE(IsAllowed(extension, google, tab_id()));
  EXPECT_FALSE(IsAllowed(extension, google_h1, tab_id()));
  EXPECT_TRUE(IsAllowed(extension, chromium, tab_id()));
  EXPECT_TRUE(IsAllowed(extension, chromium_h1, tab_id()));

  Reload();

  EXPECT_FALSE(IsAllowed(extension, google, tab_id()));
  EXPECT_FALSE(IsAllowed(extension, google_h1, tab_id()));
  EXPECT_FALSE(IsAllowed(extension, chromium, tab_id()));
  EXPECT_FALSE(IsAllowed(extension, chromium_h1, tab_id()));
}

}  // namespace
}  // namespace extensions
