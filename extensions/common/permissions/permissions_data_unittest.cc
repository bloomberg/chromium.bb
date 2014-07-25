// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "content/public/common/socket_permission_request.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/id_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/socket_permission.h"
#include "extensions/common/switches.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::UTF16ToUTF8;
using content::SocketPermissionRequest;
using extension_test_util::LoadManifest;
using extension_test_util::LoadManifestUnchecked;
using extension_test_util::LoadManifestStrict;

namespace extensions {

namespace {

const char kAllHostsPermission[] = "*://*/*";

bool CheckSocketPermission(
    scoped_refptr<Extension> extension,
    SocketPermissionRequest::OperationType type,
    const char* host,
    int port) {
  SocketPermission::CheckParam param(type, host, port);
  return extension->permissions_data()->CheckAPIPermissionWithParam(
      APIPermission::kSocket, &param);
}

// Creates and returns an extension with the given |id|, |host_permissions|, and
// manifest |location|.
scoped_refptr<const Extension> GetExtensionWithHostPermission(
    const std::string& id,
    const std::string& host_permissions,
    Manifest::Location location) {
  ListBuilder permissions;
  if (!host_permissions.empty())
    permissions.Append(host_permissions);

  return ExtensionBuilder()
      .SetManifest(
          DictionaryBuilder()
              .Set("name", id)
              .Set("description", "an extension")
              .Set("manifest_version", 2)
              .Set("version", "1.0.0")
              .Set("permissions", permissions.Pass())
              .Build())
      .SetLocation(location)
      .SetID(id)
      .Build();
}

// Checks that urls are properly restricted for the given extension.
void CheckRestrictedUrls(const Extension* extension,
                         bool block_chrome_urls) {
  // We log the name so we know _which_ extension failed here.
  const std::string& name = extension->name();
  const GURL chrome_settings_url("chrome://settings/");
  const GURL chrome_extension_url("chrome-extension://foo/bar.html");
  const GURL google_url("https://www.google.com/");
  const GURL self_url("chrome-extension://" + extension->id() + "/foo.html");
  const GURL invalid_url("chrome-debugger://foo/bar.html");

  std::string error;
  EXPECT_EQ(block_chrome_urls,
            PermissionsData::IsRestrictedUrl(
                chrome_settings_url,
                chrome_settings_url,
                extension,
                &error)) << name;
  if (block_chrome_urls)
    EXPECT_EQ(manifest_errors::kCannotAccessChromeUrl, error) << name;
  else
    EXPECT_TRUE(error.empty()) << name;

  error.clear();
  EXPECT_EQ(block_chrome_urls,
            PermissionsData::IsRestrictedUrl(
                chrome_extension_url,
                chrome_extension_url,
                extension,
                &error)) << name;
  if (block_chrome_urls)
    EXPECT_EQ(manifest_errors::kCannotAccessExtensionUrl, error) << name;
  else
    EXPECT_TRUE(error.empty()) << name;

  // Google should never be a restricted url.
  error.clear();
  EXPECT_FALSE(PermissionsData::IsRestrictedUrl(
      google_url, google_url, extension, &error)) << name;
  EXPECT_TRUE(error.empty()) << name;

  // We should always be able to access our own extension pages.
  error.clear();
  EXPECT_FALSE(PermissionsData::IsRestrictedUrl(
      self_url, self_url, extension, &error)) << name;
  EXPECT_TRUE(error.empty()) << name;

  // We should only allow other schemes for extensions when it's a whitelisted
  // extension.
  error.clear();
  bool allow_on_other_schemes =
      PermissionsData::CanExecuteScriptEverywhere(extension);
  EXPECT_EQ(!allow_on_other_schemes,
            PermissionsData::IsRestrictedUrl(
                invalid_url, invalid_url, extension, &error)) << name;
  if (!allow_on_other_schemes) {
    EXPECT_EQ(ErrorUtils::FormatErrorMessage(
                  manifest_errors::kCannotAccessPage,
                  invalid_url.spec()),
              error) << name;
  } else {
    EXPECT_TRUE(error.empty());
  }
}

}  // namespace

TEST(ExtensionPermissionsTest, EffectiveHostPermissions) {
  scoped_refptr<Extension> extension;
  URLPatternSet hosts;

  extension = LoadManifest("effective_host_permissions", "empty.json");
  EXPECT_EQ(0u,
            extension->permissions_data()
                ->GetEffectiveHostPermissions()
                .patterns()
                .size());
  EXPECT_FALSE(hosts.MatchesURL(GURL("http://www.google.com")));
  EXPECT_FALSE(extension->permissions_data()->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "one_host.json");
  hosts = extension->permissions_data()->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://www.google.com")));
  EXPECT_FALSE(hosts.MatchesURL(GURL("https://www.google.com")));
  EXPECT_FALSE(extension->permissions_data()->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions",
                           "one_host_wildcard.json");
  hosts = extension->permissions_data()->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://google.com")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://foo.google.com")));
  EXPECT_FALSE(extension->permissions_data()->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "two_hosts.json");
  hosts = extension->permissions_data()->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://www.google.com")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://www.reddit.com")));
  EXPECT_FALSE(extension->permissions_data()->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions",
                           "https_not_considered.json");
  hosts = extension->permissions_data()->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://google.com")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("https://google.com")));
  EXPECT_FALSE(extension->permissions_data()->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions",
                           "two_content_scripts.json");
  hosts = extension->permissions_data()->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://google.com")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://www.reddit.com")));
  EXPECT_TRUE(extension->permissions_data()
                  ->active_permissions()
                  ->HasEffectiveAccessToURL(GURL("http://www.reddit.com")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://news.ycombinator.com")));
  EXPECT_TRUE(
      extension->permissions_data()
          ->active_permissions()
          ->HasEffectiveAccessToURL(GURL("http://news.ycombinator.com")));
  EXPECT_FALSE(extension->permissions_data()->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "all_hosts.json");
  hosts = extension->permissions_data()->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://test/")));
  EXPECT_FALSE(hosts.MatchesURL(GURL("https://test/")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://www.google.com")));
  EXPECT_TRUE(extension->permissions_data()->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "all_hosts2.json");
  hosts = extension->permissions_data()->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://test/")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://www.google.com")));
  EXPECT_TRUE(extension->permissions_data()->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "all_hosts3.json");
  hosts = extension->permissions_data()->GetEffectiveHostPermissions();
  EXPECT_FALSE(hosts.MatchesURL(GURL("http://test/")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("https://test/")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://www.google.com")));
  EXPECT_TRUE(extension->permissions_data()->HasEffectiveAccessToAllHosts());
}

TEST(ExtensionPermissionsTest, SocketPermissions) {
  // Set feature current channel to appropriate value.
  ScopedCurrentChannel scoped_channel(chrome::VersionInfo::CHANNEL_DEV);
  scoped_refptr<Extension> extension;
  std::string error;

  extension = LoadManifest("socket_permissions", "empty.json");
  EXPECT_FALSE(CheckSocketPermission(extension,
      SocketPermissionRequest::TCP_CONNECT, "www.example.com", 80));

  extension = LoadManifestUnchecked("socket_permissions",
                                    "socket1.json",
                                    Manifest::INTERNAL, Extension::NO_FLAGS,
                                    &error);
  EXPECT_TRUE(extension.get() == NULL);
  std::string expected_error_msg_header = ErrorUtils::FormatErrorMessage(
      manifest_errors::kInvalidPermissionWithDetail,
      "socket",
      "NULL or empty permission list");
  EXPECT_EQ(expected_error_msg_header, error);

  extension = LoadManifest("socket_permissions", "socket2.json");
  EXPECT_TRUE(CheckSocketPermission(extension,
      SocketPermissionRequest::TCP_CONNECT, "www.example.com", 80));
  EXPECT_FALSE(CheckSocketPermission(
        extension, SocketPermissionRequest::UDP_BIND, "", 80));
  EXPECT_TRUE(CheckSocketPermission(
        extension, SocketPermissionRequest::UDP_BIND, "", 8888));

  EXPECT_FALSE(CheckSocketPermission(
        extension, SocketPermissionRequest::UDP_SEND_TO, "example.com", 1900));
  EXPECT_TRUE(CheckSocketPermission(
        extension,
        SocketPermissionRequest::UDP_SEND_TO,
        "239.255.255.250", 1900));
}

TEST(ExtensionPermissionsTest, IsRestrictedUrl) {
  scoped_refptr<const Extension> extension =
      GetExtensionWithHostPermission("normal_extension",
                                     kAllHostsPermission,
                                     Manifest::INTERNAL);
  // Chrome urls should be blocked for normal extensions.
  CheckRestrictedUrls(extension, true);

  scoped_refptr<const Extension> component =
      GetExtensionWithHostPermission("component",
                                     kAllHostsPermission,
                                     Manifest::COMPONENT);
  // Chrome urls should be accessible by component extensions.
  CheckRestrictedUrls(component, false);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kExtensionsOnChromeURLs);
  // Enabling the switch should allow all extensions to access chrome urls.
  CheckRestrictedUrls(extension, false);

}

TEST(ExtensionPermissionsTest, GetPermissionMessages_ManyAPIPermissions) {
  scoped_refptr<Extension> extension;
  extension = LoadManifest("permissions", "many-apis.json");
  std::vector<base::string16> warnings =
      extension->permissions_data()->GetPermissionMessageStrings();
  // Warning for "tabs" is suppressed by "history" permission.
  ASSERT_EQ(5u, warnings.size());
  EXPECT_EQ("Read and change your data on api.flickr.com",
            UTF16ToUTF8(warnings[0]));
  EXPECT_EQ("Read and change your bookmarks", UTF16ToUTF8(warnings[1]));
  EXPECT_EQ("Detect your physical location", UTF16ToUTF8(warnings[2]));
  EXPECT_EQ("Read and change your browsing history", UTF16ToUTF8(warnings[3]));
  EXPECT_EQ("Manage your apps, extensions, and themes",
            UTF16ToUTF8(warnings[4]));
}

TEST(ExtensionPermissionsTest, GetPermissionMessages_ManyHostsPermissions) {
  scoped_refptr<Extension> extension;
  extension = LoadManifest("permissions", "more-than-3-hosts.json");
  std::vector<base::string16> warnings =
      extension->permissions_data()->GetPermissionMessageStrings();
  std::vector<base::string16> warnings_details =
      extension->permissions_data()->GetPermissionMessageDetailsStrings();
  ASSERT_EQ(1u, warnings.size());
  ASSERT_EQ(1u, warnings_details.size());
  EXPECT_EQ("Read and change your data on a number of websites",
            UTF16ToUTF8(warnings[0]));
  EXPECT_EQ("- www.a.com\n- www.b.com\n- www.c.com\n- www.d.com\n- www.e.com",
            UTF16ToUTF8(warnings_details[0]));
}

TEST(ExtensionPermissionsTest, GetPermissionMessages_LocationApiPermission) {
  scoped_refptr<Extension> extension;
  extension = LoadManifest("permissions",
                           "location-api.json",
                           Manifest::COMPONENT,
                           Extension::NO_FLAGS);
  std::vector<base::string16> warnings =
      extension->permissions_data()->GetPermissionMessageStrings();
  ASSERT_EQ(1u, warnings.size());
  EXPECT_EQ("Detect your physical location", UTF16ToUTF8(warnings[0]));
}

TEST(ExtensionPermissionsTest, GetPermissionMessages_ManyHosts) {
  scoped_refptr<Extension> extension;
  extension = LoadManifest("permissions", "many-hosts.json");
  std::vector<base::string16> warnings =
      extension->permissions_data()->GetPermissionMessageStrings();
  ASSERT_EQ(1u, warnings.size());
  EXPECT_EQ(
      "Read and change your data on encrypted.google.com and www.google.com",
      UTF16ToUTF8(warnings[0]));
}

TEST(ExtensionPermissionsTest, GetPermissionMessages_Plugins) {
  scoped_refptr<Extension> extension;
  extension = LoadManifest("permissions", "plugins.json");
  std::vector<base::string16> warnings =
      extension->permissions_data()->GetPermissionMessageStrings();
// We don't parse the plugins key on Chrome OS, so it should not ask for any
// permissions.
#if defined(OS_CHROMEOS)
  ASSERT_EQ(0u, warnings.size());
#else
  ASSERT_EQ(1u, warnings.size());
  EXPECT_EQ(
      "Read and change all your data on your computer and the websites you "
      "visit",
      UTF16ToUTF8(warnings[0]));
#endif
}

// Base class for testing the CanAccessPage and CanCaptureVisiblePage
// methods of Extension for extensions with various permissions.
class ExtensionScriptAndCaptureVisibleTest : public testing::Test {
 protected:
  ExtensionScriptAndCaptureVisibleTest()
      : http_url("http://www.google.com"),
        http_url_with_path("http://www.google.com/index.html"),
        https_url("https://www.google.com"),
        file_url("file:///foo/bar"),
        favicon_url("chrome://favicon/http://www.google.com"),
        extension_url("chrome-extension://" +
            id_util::GenerateIdForPath(
                base::FilePath(FILE_PATH_LITERAL("foo")))),
        settings_url("chrome://settings"),
        about_url("about:flags") {
    urls_.insert(http_url);
    urls_.insert(http_url_with_path);
    urls_.insert(https_url);
    urls_.insert(file_url);
    urls_.insert(favicon_url);
    urls_.insert(extension_url);
    urls_.insert(settings_url);
    urls_.insert(about_url);
    // Ignore the policy delegate for this test.
    PermissionsData::SetPolicyDelegate(NULL);
  }

  bool AllowedScript(const Extension* extension, const GURL& url,
                     const GURL& top_url) {
    return AllowedScript(extension, url, top_url, -1);
  }

  bool AllowedScript(const Extension* extension, const GURL& url,
                     const GURL& top_url, int tab_id) {
    return extension->permissions_data()->CanAccessPage(
        extension, url, top_url, tab_id, -1, NULL);
  }

  bool BlockedScript(const Extension* extension, const GURL& url,
                     const GURL& top_url) {
    return !extension->permissions_data()->CanAccessPage(
        extension, url, top_url, -1, -1, NULL);
  }

  bool Allowed(const Extension* extension, const GURL& url) {
    return Allowed(extension, url, -1);
  }

  bool Allowed(const Extension* extension, const GURL& url, int tab_id) {
    return (extension->permissions_data()->CanAccessPage(
                extension, url, url, tab_id, -1, NULL) &&
            extension->permissions_data()->CanCaptureVisiblePage(tab_id, NULL));
  }

  bool CaptureOnly(const Extension* extension, const GURL& url) {
    return CaptureOnly(extension, url, -1);
  }

  bool CaptureOnly(const Extension* extension, const GURL& url, int tab_id) {
    return !extension->permissions_data()->CanAccessPage(
               extension, url, url, tab_id, -1, NULL) &&
           extension->permissions_data()->CanCaptureVisiblePage(tab_id, NULL);
  }

  bool ScriptOnly(const Extension* extension, const GURL& url,
                  const GURL& top_url) {
    return ScriptOnly(extension, url, top_url, -1);
  }

  bool ScriptOnly(const Extension* extension, const GURL& url,
                  const GURL& top_url, int tab_id) {
    return AllowedScript(extension, url, top_url, tab_id) &&
           !extension->permissions_data()->CanCaptureVisiblePage(tab_id, NULL);
  }

  bool Blocked(const Extension* extension, const GURL& url) {
    return Blocked(extension, url, -1);
  }

  bool Blocked(const Extension* extension, const GURL& url, int tab_id) {
    return !(extension->permissions_data()->CanAccessPage(
                 extension, url, url, tab_id, -1, NULL) ||
             extension->permissions_data()->CanCaptureVisiblePage(tab_id,
                                                                  NULL));
  }

  bool ScriptAllowedExclusivelyOnTab(
      const Extension* extension,
      const std::set<GURL>& allowed_urls,
      int tab_id) {
    bool result = true;
    for (std::set<GURL>::iterator it = urls_.begin(); it != urls_.end(); ++it) {
      const GURL& url = *it;
      if (allowed_urls.count(url))
        result &= AllowedScript(extension, url, url, tab_id);
      else
        result &= Blocked(extension, url, tab_id);
    }
    return result;
  }

  // URLs that are "safe" to provide scripting and capture visible tab access
  // to if the permissions allow it.
  const GURL http_url;
  const GURL http_url_with_path;
  const GURL https_url;
  const GURL file_url;

  // We should allow host permission but not scripting permission for favicon
  // urls.
  const GURL favicon_url;

  // URLs that regular extensions should never get access to.
  const GURL extension_url;
  const GURL settings_url;
  const GURL about_url;

 private:
  // The set of all URLs above.
  std::set<GURL> urls_;
};

TEST_F(ExtensionScriptAndCaptureVisibleTest, Permissions) {
  // Test <all_urls> for regular extensions.
  scoped_refptr<Extension> extension = LoadManifestStrict("script_and_capture",
      "extension_regular_all.json");

  EXPECT_TRUE(Allowed(extension.get(), http_url));
  EXPECT_TRUE(Allowed(extension.get(), https_url));
  EXPECT_TRUE(CaptureOnly(extension.get(), file_url));
  EXPECT_TRUE(CaptureOnly(extension.get(), settings_url));
  EXPECT_TRUE(CaptureOnly(extension.get(), favicon_url));
  EXPECT_TRUE(CaptureOnly(extension.get(), about_url));
  EXPECT_TRUE(CaptureOnly(extension.get(), extension_url));

  // Test access to iframed content.
  GURL within_extension_url = extension->GetResourceURL("page.html");
  EXPECT_TRUE(AllowedScript(extension.get(), http_url, http_url_with_path));
  EXPECT_TRUE(AllowedScript(extension.get(), https_url, http_url_with_path));
  EXPECT_TRUE(AllowedScript(extension.get(), http_url, within_extension_url));
  EXPECT_TRUE(AllowedScript(extension.get(), https_url, within_extension_url));
  EXPECT_TRUE(BlockedScript(extension.get(), http_url, extension_url));
  EXPECT_TRUE(BlockedScript(extension.get(), https_url, extension_url));

  EXPECT_FALSE(extension->permissions_data()->HasHostPermission(settings_url));
  EXPECT_FALSE(extension->permissions_data()->HasHostPermission(about_url));
  EXPECT_TRUE(extension->permissions_data()->HasHostPermission(favicon_url));

  // Test * for scheme, which implies just the http/https schemes.
  extension = LoadManifestStrict("script_and_capture",
      "extension_wildcard.json");
  EXPECT_TRUE(ScriptOnly(extension.get(), http_url, http_url));
  EXPECT_TRUE(ScriptOnly(extension.get(), https_url, https_url));
  EXPECT_TRUE(Blocked(extension.get(), settings_url));
  EXPECT_TRUE(Blocked(extension.get(), about_url));
  EXPECT_TRUE(Blocked(extension.get(), file_url));
  EXPECT_TRUE(Blocked(extension.get(), favicon_url));
  extension =
      LoadManifest("script_and_capture", "extension_wildcard_settings.json");
  EXPECT_TRUE(Blocked(extension.get(), settings_url));

  // Having chrome://*/ should not work for regular extensions. Note that
  // for favicon access, we require the explicit pattern chrome://favicon/*.
  std::string error;
  extension = LoadManifestUnchecked("script_and_capture",
                                    "extension_wildcard_chrome.json",
                                    Manifest::INTERNAL, Extension::NO_FLAGS,
                                    &error);
  std::vector<InstallWarning> warnings = extension->install_warnings();
  EXPECT_FALSE(warnings.empty());
  EXPECT_EQ(ErrorUtils::FormatErrorMessage(
                manifest_errors::kInvalidPermissionScheme,
                "chrome://*/"),
            warnings[0].message);
  EXPECT_TRUE(Blocked(extension.get(), settings_url));
  EXPECT_TRUE(Blocked(extension.get(), favicon_url));
  EXPECT_TRUE(Blocked(extension.get(), about_url));

  // Having chrome://favicon/* should not give you chrome://*
  extension = LoadManifestStrict("script_and_capture",
      "extension_chrome_favicon_wildcard.json");
  EXPECT_TRUE(Blocked(extension.get(), settings_url));
  EXPECT_TRUE(Blocked(extension.get(), favicon_url));
  EXPECT_TRUE(Blocked(extension.get(), about_url));
  EXPECT_TRUE(extension->permissions_data()->HasHostPermission(favicon_url));

  // Having http://favicon should not give you chrome://favicon
  extension = LoadManifestStrict("script_and_capture",
      "extension_http_favicon.json");
  EXPECT_TRUE(Blocked(extension.get(), settings_url));
  EXPECT_TRUE(Blocked(extension.get(), favicon_url));

  // Component extensions with <all_urls> should get everything.
  extension = LoadManifest("script_and_capture", "extension_component_all.json",
      Manifest::COMPONENT, Extension::NO_FLAGS);
  EXPECT_TRUE(Allowed(extension.get(), http_url));
  EXPECT_TRUE(Allowed(extension.get(), https_url));
  EXPECT_TRUE(Allowed(extension.get(), settings_url));
  EXPECT_TRUE(Allowed(extension.get(), about_url));
  EXPECT_TRUE(Allowed(extension.get(), favicon_url));
  EXPECT_TRUE(extension->permissions_data()->HasHostPermission(favicon_url));

  // Component extensions should only get access to what they ask for.
  extension = LoadManifest("script_and_capture",
      "extension_component_google.json", Manifest::COMPONENT,
      Extension::NO_FLAGS);
  EXPECT_TRUE(ScriptOnly(extension.get(), http_url, http_url));
  EXPECT_TRUE(Blocked(extension.get(), https_url));
  EXPECT_TRUE(Blocked(extension.get(), file_url));
  EXPECT_TRUE(Blocked(extension.get(), settings_url));
  EXPECT_TRUE(Blocked(extension.get(), favicon_url));
  EXPECT_TRUE(Blocked(extension.get(), about_url));
  EXPECT_TRUE(Blocked(extension.get(), extension_url));
  EXPECT_FALSE(extension->permissions_data()->HasHostPermission(settings_url));
}

TEST_F(ExtensionScriptAndCaptureVisibleTest, PermissionsWithChromeURLsEnabled) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kExtensionsOnChromeURLs);

  scoped_refptr<Extension> extension;

  // Test <all_urls> for regular extensions.
  extension = LoadManifestStrict("script_and_capture",
      "extension_regular_all.json");
  EXPECT_TRUE(Allowed(extension.get(), http_url));
  EXPECT_TRUE(Allowed(extension.get(), https_url));
  EXPECT_TRUE(CaptureOnly(extension.get(), file_url));
  EXPECT_TRUE(CaptureOnly(extension.get(), settings_url));
  EXPECT_TRUE(Allowed(extension.get(), favicon_url));  // chrome:// requested
  EXPECT_TRUE(CaptureOnly(extension.get(), about_url));
  EXPECT_TRUE(CaptureOnly(extension.get(), extension_url));

  // Test access to iframed content.
  GURL within_extension_url = extension->GetResourceURL("page.html");
  EXPECT_TRUE(AllowedScript(extension.get(), http_url, http_url_with_path));
  EXPECT_TRUE(AllowedScript(extension.get(), https_url, http_url_with_path));
  EXPECT_TRUE(AllowedScript(extension.get(), http_url, within_extension_url));
  EXPECT_TRUE(AllowedScript(extension.get(), https_url, within_extension_url));
  EXPECT_TRUE(AllowedScript(extension.get(), http_url, extension_url));
  EXPECT_TRUE(AllowedScript(extension.get(), https_url, extension_url));

  const PermissionsData* permissions_data = extension->permissions_data();
  EXPECT_FALSE(permissions_data->HasHostPermission(settings_url));
  EXPECT_FALSE(permissions_data->HasHostPermission(about_url));
  EXPECT_TRUE(permissions_data->HasHostPermission(favicon_url));

  // Test * for scheme, which implies just the http/https schemes.
  extension = LoadManifestStrict("script_and_capture",
      "extension_wildcard.json");
  EXPECT_TRUE(ScriptOnly(extension.get(), http_url, http_url));
  EXPECT_TRUE(ScriptOnly(extension.get(), https_url, https_url));
  EXPECT_TRUE(Blocked(extension.get(), settings_url));
  EXPECT_TRUE(Blocked(extension.get(), about_url));
  EXPECT_TRUE(Blocked(extension.get(), file_url));
  EXPECT_TRUE(Blocked(extension.get(), favicon_url));
  extension =
      LoadManifest("script_and_capture", "extension_wildcard_settings.json");
  EXPECT_TRUE(Blocked(extension.get(), settings_url));

  // Having chrome://*/ should work for regular extensions with the flag
  // enabled.
  std::string error;
  extension = LoadManifestUnchecked("script_and_capture",
                                    "extension_wildcard_chrome.json",
                                    Manifest::INTERNAL, Extension::NO_FLAGS,
                                    &error);
  EXPECT_FALSE(extension.get() == NULL);
  EXPECT_TRUE(Blocked(extension.get(), http_url));
  EXPECT_TRUE(Blocked(extension.get(), https_url));
  EXPECT_TRUE(ScriptOnly(extension.get(), settings_url, settings_url));
  EXPECT_TRUE(Blocked(extension.get(), about_url));
  EXPECT_TRUE(Blocked(extension.get(), file_url));
  EXPECT_TRUE(ScriptOnly(extension.get(), favicon_url, favicon_url));

  // Having chrome://favicon/* should not give you chrome://*
  extension = LoadManifestStrict("script_and_capture",
      "extension_chrome_favicon_wildcard.json");
  EXPECT_TRUE(Blocked(extension.get(), settings_url));
  EXPECT_TRUE(ScriptOnly(extension.get(), favicon_url, favicon_url));
  EXPECT_TRUE(Blocked(extension.get(), about_url));
  EXPECT_TRUE(extension->permissions_data()->HasHostPermission(favicon_url));

  // Having http://favicon should not give you chrome://favicon
  extension = LoadManifestStrict("script_and_capture",
      "extension_http_favicon.json");
  EXPECT_TRUE(Blocked(extension.get(), settings_url));
  EXPECT_TRUE(Blocked(extension.get(), favicon_url));

  // Component extensions with <all_urls> should get everything.
  extension = LoadManifest("script_and_capture", "extension_component_all.json",
      Manifest::COMPONENT, Extension::NO_FLAGS);
  EXPECT_TRUE(Allowed(extension.get(), http_url));
  EXPECT_TRUE(Allowed(extension.get(), https_url));
  EXPECT_TRUE(Allowed(extension.get(), settings_url));
  EXPECT_TRUE(Allowed(extension.get(), about_url));
  EXPECT_TRUE(Allowed(extension.get(), favicon_url));
  EXPECT_TRUE(extension->permissions_data()->HasHostPermission(favicon_url));

  // Component extensions should only get access to what they ask for.
  extension = LoadManifest("script_and_capture",
      "extension_component_google.json", Manifest::COMPONENT,
      Extension::NO_FLAGS);
  EXPECT_TRUE(ScriptOnly(extension.get(), http_url, http_url));
  EXPECT_TRUE(Blocked(extension.get(), https_url));
  EXPECT_TRUE(Blocked(extension.get(), file_url));
  EXPECT_TRUE(Blocked(extension.get(), settings_url));
  EXPECT_TRUE(Blocked(extension.get(), favicon_url));
  EXPECT_TRUE(Blocked(extension.get(), about_url));
  EXPECT_TRUE(Blocked(extension.get(), extension_url));
  EXPECT_FALSE(extension->permissions_data()->HasHostPermission(settings_url));
}

TEST_F(ExtensionScriptAndCaptureVisibleTest, TabSpecific) {
  scoped_refptr<Extension> extension =
      LoadManifestStrict("script_and_capture", "tab_specific.json");

  const PermissionsData* permissions_data = extension->permissions_data();
  EXPECT_FALSE(permissions_data->GetTabSpecificPermissionsForTesting(0));
  EXPECT_FALSE(permissions_data->GetTabSpecificPermissionsForTesting(1));
  EXPECT_FALSE(permissions_data->GetTabSpecificPermissionsForTesting(2));

  std::set<GURL> no_urls;

  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 0));
  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 1));
  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 2));

  URLPatternSet allowed_hosts;
  allowed_hosts.AddPattern(URLPattern(URLPattern::SCHEME_ALL,
                                      http_url.spec()));
  std::set<GURL> allowed_urls;
  allowed_urls.insert(http_url);
  // http_url_with_path() will also be allowed, because Extension should be
  // considering the security origin of the URL not the URL itself, and
  // http_url is in allowed_hosts.
  allowed_urls.insert(http_url_with_path);

  {
    scoped_refptr<PermissionSet> permissions(
        new PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                          allowed_hosts, URLPatternSet()));
    permissions_data->UpdateTabSpecificPermissions(0, permissions);
    EXPECT_EQ(permissions->explicit_hosts(),
              permissions_data->GetTabSpecificPermissionsForTesting(0)
                  ->explicit_hosts());
  }

  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), allowed_urls, 0));
  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 1));
  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 2));

  permissions_data->ClearTabSpecificPermissions(0);
  EXPECT_FALSE(permissions_data->GetTabSpecificPermissionsForTesting(0));

  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 0));
  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 1));
  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 2));

  std::set<GURL> more_allowed_urls = allowed_urls;
  more_allowed_urls.insert(https_url);
  URLPatternSet more_allowed_hosts = allowed_hosts;
  more_allowed_hosts.AddPattern(URLPattern(URLPattern::SCHEME_ALL,
                                           https_url.spec()));

  {
    scoped_refptr<PermissionSet> permissions(
        new PermissionSet(APIPermissionSet(),  ManifestPermissionSet(),
                          allowed_hosts, URLPatternSet()));
    permissions_data->UpdateTabSpecificPermissions(0, permissions);
    EXPECT_EQ(permissions->explicit_hosts(),
              permissions_data->GetTabSpecificPermissionsForTesting(0)
                  ->explicit_hosts());

    permissions = new PermissionSet(APIPermissionSet(),
                                    ManifestPermissionSet(),
                                    more_allowed_hosts,
                                    URLPatternSet());
    permissions_data->UpdateTabSpecificPermissions(1, permissions);
    EXPECT_EQ(permissions->explicit_hosts(),
              permissions_data->GetTabSpecificPermissionsForTesting(1)
                  ->explicit_hosts());
  }

  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), allowed_urls, 0));
  EXPECT_TRUE(
      ScriptAllowedExclusivelyOnTab(extension.get(), more_allowed_urls, 1));
  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 2));

  permissions_data->ClearTabSpecificPermissions(0);
  EXPECT_FALSE(permissions_data->GetTabSpecificPermissionsForTesting(0));

  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 0));
  EXPECT_TRUE(
      ScriptAllowedExclusivelyOnTab(extension.get(), more_allowed_urls, 1));
  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 2));

  permissions_data->ClearTabSpecificPermissions(1);
  EXPECT_FALSE(permissions_data->GetTabSpecificPermissionsForTesting(1));

  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 0));
  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 1));
  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 2));
}

}  // namespace extensions
