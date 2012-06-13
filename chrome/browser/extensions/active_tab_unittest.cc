// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/active_tab_permission_manager.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/test/test_browser_thread.h"

using base::DictionaryValue;
using base::ListValue;
using content::BrowserThread;
using content::NavigationController;

namespace extensions {
namespace {

scoped_refptr<const Extension> CreateTestExtension(
    const std::string& name,
    bool has_active_tab_permission) {
  DictionaryValue manifest;
  manifest.SetString("name", name);
  manifest.SetString("version", "1.0.0");
  manifest.SetInteger("manifest_version", 2);

  if (has_active_tab_permission) {
    scoped_ptr<ListValue> permissions(new ListValue());
    permissions->Append(Value::CreateStringValue("activeTab"));
    manifest.Set("permissions", permissions.release());
  }

  std::string error;
  scoped_refptr<const Extension> extension = Extension::Create(
      FilePath(),
      Extension::INTERNAL,
      manifest,
      0,  // no flags.
      name,
      &error);
  CHECK_EQ("", error);
  return extension;
}

class ActiveTabTest : public TabContentsTestHarness {
 public:
  ActiveTabTest()
      : extension(CreateTestExtension("extension", true)),
        another_extension(CreateTestExtension("another", true)),
        extension_without_active_tab(
            CreateTestExtension("without activeTab", false)),
        ui_thread_(BrowserThread::UI, MessageLoop::current()) {}

 protected:
  int tab_id() {
    return tab_contents()->extension_tab_helper()->tab_id();
  }

  ActiveTabPermissionManager* active_tab_permission_manager() {
    return tab_contents()->extension_tab_helper()->
                           active_tab_permission_manager();
  }

  bool IsAllowed(const scoped_refptr<const Extension>& extension,
                 const GURL& url) {
    return IsAllowed(extension, url, tab_id());
  }

  bool IsAllowed(const scoped_refptr<const Extension>& extension,
                 const GURL& url,
                 int tab_id) {
    return (extension->CanExecuteScriptOnPage(url, tab_id, NULL, NULL) &&
            extension->CanCaptureVisiblePage(url, tab_id, NULL));
  }

  bool IsBlocked(const scoped_refptr<const Extension>& extension,
                 const GURL& url) {
    return IsBlocked(extension, url, tab_id());
  }

  bool IsBlocked(const scoped_refptr<const Extension>& extension,
                 const GURL& url,
                 int tab_id) {
    return (!extension->CanExecuteScriptOnPage(url, tab_id, NULL, NULL) &&
            !extension->CanCaptureVisiblePage(url, tab_id, NULL));
  }

  // Fakes loading a new frame on the page using the WebContentsObserver
  // interface.
  // TODO(kalman): if somebody can tell me a way to do this from the
  // TabContentsTestHarness (or any other test harness) then pray tell.
  void AddFrame(const GURL& url) {
    active_tab_permission_manager()->DidCommitProvisionalLoadForFrame(
        0,      // frame_id
        false,  // is_main_frame
        url,
        content::PAGE_TRANSITION_AUTO_SUBFRAME,
        NULL);  // render_view_host
  }

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

  active_tab_permission_manager()->GrantIfRequested(extension);
  active_tab_permission_manager()->GrantIfRequested(
      extension_without_active_tab);

  // Granted to extension and extension_without_active_tab, but the latter
  // doesn't have the activeTab permission so not granted.
  EXPECT_TRUE(IsAllowed(extension, google));
  EXPECT_TRUE(IsBlocked(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  // Other subdomains shouldn't be given access.
  GURL mail_google("http://mail.google.com");
  EXPECT_TRUE(IsBlocked(extension, mail_google));
  EXPECT_TRUE(IsBlocked(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  // Reloading the page should clear the active permissions.
  Reload();

  EXPECT_TRUE(IsBlocked(extension, google));
  EXPECT_TRUE(IsBlocked(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  // But they should still be able to be granted again.
  active_tab_permission_manager()->GrantIfRequested(extension);

  EXPECT_TRUE(IsAllowed(extension, google));
  EXPECT_TRUE(IsBlocked(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  // And grant a few more times redundantly for good measure.
  active_tab_permission_manager()->GrantIfRequested(extension);
  active_tab_permission_manager()->GrantIfRequested(extension);
  active_tab_permission_manager()->GrantIfRequested(another_extension);
  active_tab_permission_manager()->GrantIfRequested(another_extension);
  active_tab_permission_manager()->GrantIfRequested(another_extension);
  active_tab_permission_manager()->GrantIfRequested(extension);
  active_tab_permission_manager()->GrantIfRequested(extension);
  active_tab_permission_manager()->GrantIfRequested(another_extension);
  active_tab_permission_manager()->GrantIfRequested(another_extension);

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

  // Should be able to grant to multiple extensions at the same time (if they
  // have the activeTab permission, of course).
  active_tab_permission_manager()->GrantIfRequested(extension);
  active_tab_permission_manager()->GrantIfRequested(another_extension);
  active_tab_permission_manager()->GrantIfRequested(
      extension_without_active_tab);

  EXPECT_TRUE(IsBlocked(extension, google));
  EXPECT_TRUE(IsBlocked(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  EXPECT_TRUE(IsAllowed(extension, chromium));
  EXPECT_TRUE(IsAllowed(another_extension, chromium));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, chromium));

  // Should be able to go back to URLs that were previously cleared.
  NavigateAndCommit(google);

  active_tab_permission_manager()->GrantIfRequested(extension);
  active_tab_permission_manager()->GrantIfRequested(another_extension);
  active_tab_permission_manager()->GrantIfRequested(
      extension_without_active_tab);

  EXPECT_TRUE(IsAllowed(extension, google));
  EXPECT_TRUE(IsAllowed(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  EXPECT_TRUE(IsBlocked(extension, chromium));
  EXPECT_TRUE(IsBlocked(another_extension, chromium));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, chromium));
};

TEST_F(ActiveTabTest, GrantToMultiplePages) {
  GURL google("http://www.google.com");
  NavigateAndCommit(google);

  active_tab_permission_manager()->GrantIfRequested(extension);

  // Adding a frame after access was granted shouldn't give it access.
  GURL chromium("http://www.chromium.org");
  AddFrame(chromium);

  EXPECT_TRUE(IsAllowed(extension, google));
  EXPECT_TRUE(IsBlocked(extension, chromium));

  // Granting access to another extension should give it access to both the
  // main and sub-frames, but still not to the first extension.
  active_tab_permission_manager()->GrantIfRequested(another_extension);

  EXPECT_TRUE(IsAllowed(extension, google));
  EXPECT_TRUE(IsBlocked(extension, chromium));
  EXPECT_TRUE(IsAllowed(another_extension, google));
  EXPECT_TRUE(IsAllowed(another_extension, chromium));

  // Granting access to the first extension should now give it access to the
  // frame.
  active_tab_permission_manager()->GrantIfRequested(extension);

  EXPECT_TRUE(IsAllowed(extension, google));
  EXPECT_TRUE(IsAllowed(extension, chromium));
  EXPECT_TRUE(IsAllowed(another_extension, google));
  EXPECT_TRUE(IsAllowed(another_extension, chromium));

  // Reloading should clear all access.
  Reload();

  EXPECT_TRUE(IsBlocked(extension, google));
  EXPECT_TRUE(IsBlocked(extension, chromium));
  EXPECT_TRUE(IsBlocked(another_extension, google));
  EXPECT_TRUE(IsBlocked(another_extension, chromium));

  // And after granting, no access to the frames that were there.
  active_tab_permission_manager()->GrantIfRequested(extension);

  EXPECT_TRUE(IsAllowed(extension, google));
  EXPECT_TRUE(IsBlocked(extension, chromium));
  EXPECT_TRUE(IsBlocked(another_extension, google));
  EXPECT_TRUE(IsBlocked(another_extension, chromium));

  // Having lots of frames on the same page should behave as expected.
  GURL chromium_index("http://www.chromium.org/index.html");
  GURL chromium_about("http://www.chromium.org/about.html");
  GURL chromium_blank("http://www.chromium.org/blank.html");
  GURL gmail("http://www.gmail.com");
  GURL mail_google("http://mail.google.com");
  GURL plus_google("http://plus.google.com");
  GURL codereview_appspot("http://codereview.appspot.com");
  GURL omahaproxy_appspot("http://omahaproxy.appspot.com");

  AddFrame(chromium_index);
  AddFrame(chromium_about);
  AddFrame(gmail);
  AddFrame(mail_google);

  EXPECT_TRUE(IsBlocked(extension, chromium_index));
  EXPECT_TRUE(IsBlocked(extension, chromium_about));
  EXPECT_TRUE(IsBlocked(extension, chromium_blank));
  EXPECT_TRUE(IsBlocked(extension, gmail));
  EXPECT_TRUE(IsBlocked(extension, mail_google));
  EXPECT_TRUE(IsBlocked(extension, plus_google));
  EXPECT_TRUE(IsBlocked(extension, codereview_appspot));
  EXPECT_TRUE(IsBlocked(extension, omahaproxy_appspot));

  EXPECT_TRUE(IsBlocked(another_extension, chromium_index));
  EXPECT_TRUE(IsBlocked(another_extension, chromium_about));
  EXPECT_TRUE(IsBlocked(another_extension, chromium_blank));
  EXPECT_TRUE(IsBlocked(another_extension, gmail));
  EXPECT_TRUE(IsBlocked(another_extension, mail_google));
  EXPECT_TRUE(IsBlocked(another_extension, plus_google));
  EXPECT_TRUE(IsBlocked(another_extension, codereview_appspot));
  EXPECT_TRUE(IsBlocked(another_extension, omahaproxy_appspot));

  active_tab_permission_manager()->GrantIfRequested(extension);

  AddFrame(chromium_blank);
  AddFrame(plus_google);
  AddFrame(codereview_appspot);

  EXPECT_TRUE(IsAllowed(extension, chromium_index));
  EXPECT_TRUE(IsAllowed(extension, chromium_about));
  // Even though chromium_blank hasn't been given granted, this will work
  // because it's on the same origin as the other codereview URLs.
  // because k$b
  EXPECT_TRUE(IsAllowed(extension, chromium_blank));
  EXPECT_TRUE(IsAllowed(extension, gmail));
  EXPECT_TRUE(IsAllowed(extension, mail_google));
  EXPECT_TRUE(IsBlocked(extension, plus_google));
  EXPECT_TRUE(IsBlocked(extension, codereview_appspot));
  EXPECT_TRUE(IsBlocked(extension, omahaproxy_appspot));

  EXPECT_TRUE(IsBlocked(another_extension, chromium_index));
  EXPECT_TRUE(IsBlocked(another_extension, chromium_about));
  EXPECT_TRUE(IsBlocked(another_extension, chromium_blank));
  EXPECT_TRUE(IsBlocked(another_extension, gmail));
  EXPECT_TRUE(IsBlocked(another_extension, mail_google));
  EXPECT_TRUE(IsBlocked(another_extension, plus_google));
  EXPECT_TRUE(IsBlocked(another_extension, codereview_appspot));
  EXPECT_TRUE(IsBlocked(another_extension, omahaproxy_appspot));

  active_tab_permission_manager()->GrantIfRequested(another_extension);

  AddFrame(omahaproxy_appspot);

  EXPECT_TRUE(IsAllowed(extension, chromium_index));
  EXPECT_TRUE(IsAllowed(extension, chromium_about));
  EXPECT_TRUE(IsAllowed(extension, chromium_blank));
  EXPECT_TRUE(IsAllowed(extension, gmail));
  EXPECT_TRUE(IsAllowed(extension, mail_google));
  EXPECT_TRUE(IsBlocked(extension, plus_google));
  EXPECT_TRUE(IsBlocked(extension, codereview_appspot));
  EXPECT_TRUE(IsBlocked(extension, omahaproxy_appspot));

  EXPECT_TRUE(IsAllowed(another_extension, chromium_index));
  EXPECT_TRUE(IsAllowed(another_extension, chromium_about));
  EXPECT_TRUE(IsAllowed(another_extension, chromium_blank));
  EXPECT_TRUE(IsAllowed(another_extension, gmail));
  EXPECT_TRUE(IsAllowed(another_extension, mail_google));
  EXPECT_TRUE(IsAllowed(another_extension, plus_google));
  EXPECT_TRUE(IsAllowed(another_extension, codereview_appspot));
  EXPECT_TRUE(IsBlocked(another_extension, omahaproxy_appspot));

  active_tab_permission_manager()->GrantIfRequested(extension);

  EXPECT_TRUE(IsAllowed(extension, chromium_index));
  EXPECT_TRUE(IsAllowed(extension, chromium_about));
  EXPECT_TRUE(IsAllowed(extension, chromium_blank));
  EXPECT_TRUE(IsAllowed(extension, gmail));
  EXPECT_TRUE(IsAllowed(extension, mail_google));
  EXPECT_TRUE(IsAllowed(extension, plus_google));
  EXPECT_TRUE(IsAllowed(extension, codereview_appspot));
  EXPECT_TRUE(IsAllowed(extension, omahaproxy_appspot));

  EXPECT_TRUE(IsAllowed(another_extension, chromium_index));
  EXPECT_TRUE(IsAllowed(another_extension, chromium_about));
  EXPECT_TRUE(IsAllowed(another_extension, chromium_blank));
  EXPECT_TRUE(IsAllowed(another_extension, gmail));
  EXPECT_TRUE(IsAllowed(another_extension, mail_google));
  EXPECT_TRUE(IsAllowed(another_extension, plus_google));
  EXPECT_TRUE(IsAllowed(another_extension, codereview_appspot));
  EXPECT_TRUE(IsBlocked(another_extension, omahaproxy_appspot));
}

TEST_F(ActiveTabTest, Uninstalling) {
  // Some semi-arbitrary setup.
  GURL google("http://www.google.com");
  NavigateAndCommit(google);

  GURL chromium("http://www.chromium.org");
  AddFrame(chromium);

  active_tab_permission_manager()->GrantIfRequested(extension);

  GURL gmail("http://www.gmail.com");
  AddFrame(gmail);

  EXPECT_TRUE(IsAllowed(extension, google));
  EXPECT_TRUE(IsAllowed(extension, chromium));
  EXPECT_TRUE(IsBlocked(extension, gmail));

  // Uninstalling the extension should clear its tab permissions.
  UnloadedExtensionInfo details(
      extension,
      extension_misc::UNLOAD_REASON_DISABLE);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_UNLOADED,
      content::Source<Profile>(tab_contents()->profile()),
      content::Details<UnloadedExtensionInfo>(&details));

  EXPECT_TRUE(IsBlocked(extension, google));
  EXPECT_TRUE(IsBlocked(extension, chromium));
  EXPECT_TRUE(IsBlocked(extension, gmail));

  // Granting the extension again should give them back.
  active_tab_permission_manager()->GrantIfRequested(extension);

  EXPECT_TRUE(IsAllowed(extension, google));
  EXPECT_TRUE(IsAllowed(extension, chromium));
  EXPECT_TRUE(IsAllowed(extension, gmail));
}

TEST_F(ActiveTabTest, OnlyActiveTab) {
  GURL google("http://www.google.com");
  NavigateAndCommit(google);

  active_tab_permission_manager()->GrantIfRequested(extension);

  EXPECT_TRUE(IsAllowed(extension, google, tab_id()));
  EXPECT_TRUE(IsBlocked(extension, google, tab_id() + 1));
}

}  // namespace
}  // namespace extensions
