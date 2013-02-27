// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/commands/commands_handler.h"
#include "chrome/common/extensions/command.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/extensions/features/feature.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "chrome/common/extensions/permissions/socket_permission.h"
#include "chrome/common/extensions/permissions/usb_device_permission.h"
#include "chrome/common/url_constants.h"
#include "extensions/common/error_utils.h"
#include "googleurl/src/gurl.h"
#include "net/base/mime_sniffer.h"
#include "net/base/mock_host_resolver.h"
#include "skia/ext/image_operations.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

using content::SocketPermissionRequest;

namespace keys = extension_manifest_keys;
namespace values = extension_manifest_values;
namespace errors = extension_manifest_errors;

namespace extensions {
namespace {

scoped_refptr<Extension> LoadManifestUnchecked(
    const std::string& dir,
    const std::string& test_file,
    Manifest::Location location,
    int extra_flags,
    std::string* error) {
  base::FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("extensions")
             .AppendASCII(dir)
             .AppendASCII(test_file);

  JSONFileValueSerializer serializer(path);
  scoped_ptr<Value> result(serializer.Deserialize(NULL, error));
  if (!result.get())
    return NULL;

  scoped_refptr<Extension> extension = Extension::Create(
      path.DirName(), location, *static_cast<DictionaryValue*>(result.get()),
      extra_flags, error);
  return extension;
}

static scoped_refptr<Extension> LoadManifest(const std::string& dir,
                                             const std::string& test_file,
                                             Manifest::Location location,
                                             int extra_flags) {
  std::string error;
  scoped_refptr<Extension> extension = LoadManifestUnchecked(dir, test_file,
    location, extra_flags, &error);

  EXPECT_TRUE(extension) << test_file << ":" << error;
  return extension;
}

static scoped_refptr<Extension> LoadManifest(const std::string& dir,
                                             const std::string& test_file,
                                             int extra_flags) {
  return LoadManifest(dir, test_file, Manifest::INVALID_LOCATION, extra_flags);
}

static scoped_refptr<Extension> LoadManifest(const std::string& dir,
                                             const std::string& test_file) {
  return LoadManifest(dir, test_file, Extension::NO_FLAGS);
}

static scoped_refptr<Extension> LoadManifestStrict(
    const std::string& dir,
    const std::string& test_file) {
  return LoadManifest(dir, test_file, Extension::NO_FLAGS);
}

}  // namespace

class ExtensionTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ManifestHandler::Register(extension_manifest_keys::kCommands,
                              make_linked_ptr(new CommandsHandler));
  }
};

// We persist location values in the preferences, so this is a sanity test that
// someone doesn't accidentally change them.
TEST_F(ExtensionTest, LocationValuesTest) {
  ASSERT_EQ(0, Manifest::INVALID_LOCATION);
  ASSERT_EQ(1, Manifest::INTERNAL);
  ASSERT_EQ(2, Manifest::EXTERNAL_PREF);
  ASSERT_EQ(3, Manifest::EXTERNAL_REGISTRY);
  ASSERT_EQ(4, Manifest::UNPACKED);
  ASSERT_EQ(5, Manifest::COMPONENT);
  ASSERT_EQ(6, Manifest::EXTERNAL_PREF_DOWNLOAD);
  ASSERT_EQ(7, Manifest::EXTERNAL_POLICY_DOWNLOAD);
  ASSERT_EQ(8, Manifest::COMMAND_LINE);
}

TEST_F(ExtensionTest, LocationPriorityTest) {
  for (int i = 0; i < Manifest::NUM_LOCATIONS; i++) {
    Manifest::Location loc = static_cast<Manifest::Location>(i);

    // INVALID is not a valid location.
    if (loc == Manifest::INVALID_LOCATION)
      continue;

    // Comparing a location that has no rank will hit a CHECK. Do a
    // compare with every valid location, to be sure each one is covered.

    // Check that no install source can override a componenet extension.
    ASSERT_EQ(Manifest::COMPONENT,
              Manifest::GetHigherPriorityLocation(Manifest::COMPONENT, loc));
    ASSERT_EQ(Manifest::COMPONENT,
              Manifest::GetHigherPriorityLocation(loc, Manifest::COMPONENT));

    // Check that any source can override a user install. This might change
    // in the future, in which case this test should be updated.
    ASSERT_EQ(loc,
              Manifest::GetHigherPriorityLocation(Manifest::INTERNAL, loc));
    ASSERT_EQ(loc,
              Manifest::GetHigherPriorityLocation(loc, Manifest::INTERNAL));
  }

  // Check a few interesting cases that we know can happen:
  ASSERT_EQ(Manifest::EXTERNAL_POLICY_DOWNLOAD,
            Manifest::GetHigherPriorityLocation(
                Manifest::EXTERNAL_POLICY_DOWNLOAD,
                Manifest::EXTERNAL_PREF));

  ASSERT_EQ(Manifest::EXTERNAL_PREF,
            Manifest::GetHigherPriorityLocation(
                Manifest::INTERNAL,
                Manifest::EXTERNAL_PREF));
}

TEST_F(ExtensionTest, GetResourceURLAndPath) {
  scoped_refptr<Extension> extension = LoadManifestStrict("empty_manifest",
      "empty.json");
  EXPECT_TRUE(extension.get());

  EXPECT_EQ(extension->url().spec() + "bar/baz.js",
            Extension::GetResourceURL(extension->url(), "bar/baz.js").spec());
  EXPECT_EQ(extension->url().spec() + "baz.js",
            Extension::GetResourceURL(extension->url(),
                                      "bar/../baz.js").spec());
  EXPECT_EQ(extension->url().spec() + "baz.js",
            Extension::GetResourceURL(extension->url(), "../baz.js").spec());

  // Test that absolute-looking paths ("/"-prefixed) are pasted correctly.
  EXPECT_EQ(extension->url().spec() + "test.html",
            extension->GetResourceURL("/test.html").spec());
}

TEST_F(ExtensionTest, GetAbsolutePathNoError) {
  scoped_refptr<Extension> extension = LoadManifestStrict("absolute_path",
      "absolute.json");
  EXPECT_TRUE(extension.get());
  std::string err;
  std::vector<InstallWarning> warnings;
  EXPECT_TRUE(extension_file_util::ValidateExtension(extension.get(),
                                                     &err, &warnings));
  EXPECT_EQ(0U, warnings.size());

  EXPECT_EQ(extension->path().AppendASCII("test.html").value(),
            extension->GetResource("test.html").GetFilePath().value());
  EXPECT_EQ(extension->path().AppendASCII("test.js").value(),
            extension->GetResource("test.js").GetFilePath().value());
}


TEST_F(ExtensionTest, IdIsValid) {
  EXPECT_TRUE(Extension::IdIsValid("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Extension::IdIsValid("pppppppppppppppppppppppppppppppp"));
  EXPECT_TRUE(Extension::IdIsValid("abcdefghijklmnopabcdefghijklmnop"));
  EXPECT_TRUE(Extension::IdIsValid("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"));
  EXPECT_FALSE(Extension::IdIsValid("abcdefghijklmnopabcdefghijklmno"));
  EXPECT_FALSE(Extension::IdIsValid("abcdefghijklmnopabcdefghijklmnopa"));
  EXPECT_FALSE(Extension::IdIsValid("0123456789abcdef0123456789abcdef"));
  EXPECT_FALSE(Extension::IdIsValid("abcdefghijklmnopabcdefghijklmnoq"));
  EXPECT_FALSE(Extension::IdIsValid("abcdefghijklmnopabcdefghijklmno0"));
}

TEST_F(ExtensionTest, GenerateID) {
  const uint8 public_key_info[] = {
    0x30, 0x81, 0x9f, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7,
    0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x81, 0x8d, 0x00, 0x30, 0x81,
    0x89, 0x02, 0x81, 0x81, 0x00, 0xb8, 0x7f, 0x2b, 0x20, 0xdc, 0x7c, 0x9b,
    0x0c, 0xdc, 0x51, 0x61, 0x99, 0x0d, 0x36, 0x0f, 0xd4, 0x66, 0x88, 0x08,
    0x55, 0x84, 0xd5, 0x3a, 0xbf, 0x2b, 0xa4, 0x64, 0x85, 0x7b, 0x0c, 0x04,
    0x13, 0x3f, 0x8d, 0xf4, 0xbc, 0x38, 0x0d, 0x49, 0xfe, 0x6b, 0xc4, 0x5a,
    0xb0, 0x40, 0x53, 0x3a, 0xd7, 0x66, 0x09, 0x0f, 0x9e, 0x36, 0x74, 0x30,
    0xda, 0x8a, 0x31, 0x4f, 0x1f, 0x14, 0x50, 0xd7, 0xc7, 0x20, 0x94, 0x17,
    0xde, 0x4e, 0xb9, 0x57, 0x5e, 0x7e, 0x0a, 0xe5, 0xb2, 0x65, 0x7a, 0x89,
    0x4e, 0xb6, 0x47, 0xff, 0x1c, 0xbd, 0xb7, 0x38, 0x13, 0xaf, 0x47, 0x85,
    0x84, 0x32, 0x33, 0xf3, 0x17, 0x49, 0xbf, 0xe9, 0x96, 0xd0, 0xd6, 0x14,
    0x6f, 0x13, 0x8d, 0xc5, 0xfc, 0x2c, 0x72, 0xba, 0xac, 0xea, 0x7e, 0x18,
    0x53, 0x56, 0xa6, 0x83, 0xa2, 0xce, 0x93, 0x93, 0xe7, 0x1f, 0x0f, 0xe6,
    0x0f, 0x02, 0x03, 0x01, 0x00, 0x01
  };

  std::string extension_id;
  EXPECT_TRUE(
      Extension::GenerateId(
          std::string(reinterpret_cast<const char*>(&public_key_info[0]),
                      arraysize(public_key_info)),
          &extension_id));
  EXPECT_EQ("melddjfinppjdikinhbgehiennejpfhp", extension_id);
}

// This test ensures that the mimetype sniffing code stays in sync with the
// actual crx files that we test other parts of the system with.
TEST_F(ExtensionTest, MimeTypeSniffing) {
  base::FilePath path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("extensions").AppendASCII("good.crx");

  std::string data;
  ASSERT_TRUE(file_util::ReadFileToString(path, &data));

  std::string result;
  EXPECT_TRUE(net::SniffMimeType(data.c_str(), data.size(),
              GURL("http://www.example.com/foo.crx"), "", &result));
  EXPECT_EQ(std::string(Extension::kMimeType), result);

  data.clear();
  result.clear();
  path = path.DirName().AppendASCII("bad_magic.crx");
  ASSERT_TRUE(file_util::ReadFileToString(path, &data));
  EXPECT_TRUE(net::SniffMimeType(data.c_str(), data.size(),
              GURL("http://www.example.com/foo.crx"), "", &result));
  EXPECT_EQ("application/octet-stream", result);
}

TEST_F(ExtensionTest, EffectiveHostPermissions) {
  scoped_refptr<Extension> extension;
  URLPatternSet hosts;

  extension = LoadManifest("effective_host_permissions", "empty.json");
  EXPECT_EQ(0u, extension->GetEffectiveHostPermissions().patterns().size());
  EXPECT_FALSE(hosts.MatchesURL(GURL("http://www.google.com")));
  EXPECT_FALSE(extension->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "one_host.json");
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://www.google.com")));
  EXPECT_FALSE(hosts.MatchesURL(GURL("https://www.google.com")));
  EXPECT_FALSE(extension->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions",
                           "one_host_wildcard.json");
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://google.com")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://foo.google.com")));
  EXPECT_FALSE(extension->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "two_hosts.json");
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://www.google.com")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://www.reddit.com")));
  EXPECT_FALSE(extension->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions",
                           "https_not_considered.json");
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://google.com")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("https://google.com")));
  EXPECT_FALSE(extension->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions",
                           "two_content_scripts.json");
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://google.com")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://www.reddit.com")));
  EXPECT_TRUE(extension->GetActivePermissions()->HasEffectiveAccessToURL(
      GURL("http://www.reddit.com")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://news.ycombinator.com")));
  EXPECT_TRUE(extension->GetActivePermissions()->HasEffectiveAccessToURL(
      GURL("http://news.ycombinator.com")));
  EXPECT_FALSE(extension->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "all_hosts.json");
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://test/")));
  EXPECT_FALSE(hosts.MatchesURL(GURL("https://test/")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://www.google.com")));
  EXPECT_TRUE(extension->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "all_hosts2.json");
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://test/")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://www.google.com")));
  EXPECT_TRUE(extension->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "all_hosts3.json");
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_FALSE(hosts.MatchesURL(GURL("http://test/")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("https://test/")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://www.google.com")));
  EXPECT_TRUE(extension->HasEffectiveAccessToAllHosts());
}

static bool CheckSocketPermission(scoped_refptr<Extension> extension,
    SocketPermissionRequest::OperationType type,
    const char* host,
    int port) {
  SocketPermission::CheckParam param(type, host, port);
  return extension->CheckAPIPermissionWithParam(
      APIPermission::kSocket, &param);
}

TEST_F(ExtensionTest, SocketPermissions) {
  // Set feature current channel to appropriate value.
  Feature::ScopedCurrentChannel scoped_channel(
      chrome::VersionInfo::CHANNEL_DEV);
  scoped_refptr<Extension> extension;
  std::string error;

  extension = LoadManifest("socket_permissions", "empty.json");
  EXPECT_FALSE(CheckSocketPermission(extension,
      SocketPermissionRequest::TCP_CONNECT, "www.example.com", 80));

  extension = LoadManifestUnchecked("socket_permissions",
                                    "socket1.json",
                                    Manifest::INTERNAL, Extension::NO_FLAGS,
                                    &error);
  EXPECT_TRUE(extension == NULL);
  ASSERT_EQ(ErrorUtils::FormatErrorMessage(
        errors::kInvalidPermission, "socket"), error);

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

// This tests the API permissions with an empty manifest (one that just
// specifies a name and a version and nothing else).
TEST_F(ExtensionTest, ApiPermissions) {
  const struct {
    const char* permission_name;
    bool expect_success;
  } kTests[] = {
    // Negative test.
    { "non_existing_permission", false },
    // Test default module/package permission.
    { "browserAction",  true },
    { "devtools",       true },
    { "extension",      true },
    { "i18n",           true },
    { "pageAction",     true },
    { "pageActions",    true },
    { "test",           true },
    // Some negative tests.
    { "bookmarks",      false },
    { "cookies",        false },
    { "history",        false },
    // Make sure we find the module name after stripping '.' and '/'.
    { "browserAction/abcd/onClick",  true },
    { "browserAction.abcd.onClick",  true },
    // Test Tabs functions.
    { "tabs.create",      true},
    { "tabs.duplicate",   true},
    { "tabs.onRemoved",   true},
    { "tabs.remove",      true},
    { "tabs.update",      true},
    { "tabs.getSelected", true},
    { "tabs.onUpdated",   true },
    // Test getPermissionWarnings functions. Only one requires permissions.
    { "management.getPermissionWarningsById", false },
    { "management.getPermissionWarningsByManifest", true },
  };

  scoped_refptr<Extension> extension;
  extension = LoadManifest("empty_manifest", "empty.json");

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTests); ++i) {
    EXPECT_EQ(kTests[i].expect_success,
              extension->HasAPIPermission(kTests[i].permission_name))
                  << "Permission being tested: " << kTests[i].permission_name;
  }
}

TEST_F(ExtensionTest, GetPermissionMessages_ManyApiPermissions) {
  scoped_refptr<Extension> extension;
  extension = LoadManifest("permissions", "many-apis.json");
  std::vector<string16> warnings = extension->GetPermissionMessageStrings();
  ASSERT_EQ(6u, warnings.size());
  EXPECT_EQ("Access your data on api.flickr.com",
            UTF16ToUTF8(warnings[0]));
  EXPECT_EQ("Read and modify your bookmarks", UTF16ToUTF8(warnings[1]));
  EXPECT_EQ("Detect your physical location", UTF16ToUTF8(warnings[2]));
  EXPECT_EQ("Read and modify your browsing history", UTF16ToUTF8(warnings[3]));
  EXPECT_EQ("Access your tabs and browsing activity", UTF16ToUTF8(warnings[4]));
  EXPECT_EQ("Manage your apps, extensions, and themes",
            UTF16ToUTF8(warnings[5]));
}

TEST_F(ExtensionTest, GetPermissionMessages_ManyHosts) {
  scoped_refptr<Extension> extension;
  extension = LoadManifest("permissions", "many-hosts.json");
  std::vector<string16> warnings = extension->GetPermissionMessageStrings();
  ASSERT_EQ(1u, warnings.size());
  EXPECT_EQ("Access your data on encrypted.google.com and www.google.com",
            UTF16ToUTF8(warnings[0]));
}

TEST_F(ExtensionTest, GetPermissionMessages_Plugins) {
  scoped_refptr<Extension> extension;
  extension = LoadManifest("permissions", "plugins.json");
  std::vector<string16> warnings = extension->GetPermissionMessageStrings();
  // We don't parse the plugins key on Chrome OS, so it should not ask for any
  // permissions.
#if defined(OS_CHROMEOS)
  ASSERT_EQ(0u, warnings.size());
#else
  ASSERT_EQ(1u, warnings.size());
  EXPECT_EQ("Access all data on your computer and the websites you visit",
            UTF16ToUTF8(warnings[0]));
#endif
}

TEST_F(ExtensionTest, WantsFileAccess) {
  scoped_refptr<Extension> extension;
  GURL file_url("file:///etc/passwd");

  // <all_urls> permission
  extension = LoadManifest("permissions", "permissions_all_urls.json");
  EXPECT_TRUE(extension->wants_file_access());
  EXPECT_FALSE(extension->CanExecuteScriptOnPage(
      file_url, file_url, -1, NULL, NULL));
  extension = LoadManifest(
      "permissions", "permissions_all_urls.json", Extension::ALLOW_FILE_ACCESS);
  EXPECT_TRUE(extension->wants_file_access());
  EXPECT_TRUE(extension->CanExecuteScriptOnPage(
      file_url, file_url, -1, NULL, NULL));

  // file:///* permission
  extension = LoadManifest("permissions", "permissions_file_scheme.json");
  EXPECT_TRUE(extension->wants_file_access());
  EXPECT_FALSE(extension->CanExecuteScriptOnPage(
      file_url, file_url, -1, NULL, NULL));
  extension = LoadManifest("permissions", "permissions_file_scheme.json",
      Extension::ALLOW_FILE_ACCESS);
  EXPECT_TRUE(extension->wants_file_access());
  EXPECT_TRUE(extension->CanExecuteScriptOnPage(
      file_url, file_url, -1, NULL, NULL));

  // http://* permission
  extension = LoadManifest("permissions", "permissions_http_scheme.json");
  EXPECT_FALSE(extension->wants_file_access());
  EXPECT_FALSE(extension->CanExecuteScriptOnPage(
      file_url, file_url, -1, NULL, NULL));
  extension = LoadManifest("permissions", "permissions_http_scheme.json",
      Extension::ALLOW_FILE_ACCESS);
  EXPECT_FALSE(extension->wants_file_access());
  EXPECT_FALSE(extension->CanExecuteScriptOnPage(
      file_url, file_url, -1, NULL, NULL));

  // <all_urls> content script match
  extension = LoadManifest("permissions", "content_script_all_urls.json");
  EXPECT_TRUE(extension->wants_file_access());
  EXPECT_FALSE(extension->CanExecuteScriptOnPage(
      file_url, file_url, -1, &extension->content_scripts()[0], NULL));
  extension = LoadManifest("permissions", "content_script_all_urls.json",
      Extension::ALLOW_FILE_ACCESS);
  EXPECT_TRUE(extension->wants_file_access());
  EXPECT_TRUE(extension->CanExecuteScriptOnPage(
      file_url, file_url, -1, &extension->content_scripts()[0], NULL));

  // file:///* content script match
  extension = LoadManifest("permissions", "content_script_file_scheme.json");
  EXPECT_TRUE(extension->wants_file_access());
  EXPECT_FALSE(extension->CanExecuteScriptOnPage(
      file_url, file_url, -1, &extension->content_scripts()[0], NULL));
  extension = LoadManifest("permissions", "content_script_file_scheme.json",
      Extension::ALLOW_FILE_ACCESS);
  EXPECT_TRUE(extension->wants_file_access());
  EXPECT_TRUE(extension->CanExecuteScriptOnPage(
      file_url, file_url, -1, &extension->content_scripts()[0], NULL));

  // http://* content script match
  extension = LoadManifest("permissions", "content_script_http_scheme.json");
  EXPECT_FALSE(extension->wants_file_access());
  EXPECT_FALSE(extension->CanExecuteScriptOnPage(
      file_url, file_url, -1, &extension->content_scripts()[0], NULL));
  extension = LoadManifest("permissions", "content_script_http_scheme.json",
      Extension::ALLOW_FILE_ACCESS);
  EXPECT_FALSE(extension->wants_file_access());
  EXPECT_FALSE(extension->CanExecuteScriptOnPage(
      file_url, file_url, -1, &extension->content_scripts()[0], NULL));
}

TEST_F(ExtensionTest, ExtraFlags) {
  scoped_refptr<Extension> extension;
  extension = LoadManifest("app", "manifest.json", Extension::FROM_WEBSTORE);
  EXPECT_TRUE(extension->from_webstore());

  extension = LoadManifest("app", "manifest.json", Extension::FROM_BOOKMARK);
  EXPECT_TRUE(extension->from_bookmark());

  extension = LoadManifest("app", "manifest.json", Extension::NO_FLAGS);
  EXPECT_FALSE(extension->from_bookmark());
  EXPECT_FALSE(extension->from_webstore());
}

TEST_F(ExtensionTest, BrowserActionSynthesizesCommand) {
  scoped_refptr<Extension> extension;

  extension = LoadManifest("api_test/browser_action/synthesized",
                           "manifest.json");
  // An extension with a browser action but no extension command specified
  // should get a command assigned to it.
  const extensions::Command* command =
      CommandsInfo::GetBrowserActionCommand(extension);
  ASSERT_TRUE(command != NULL);
  ASSERT_EQ(ui::VKEY_UNKNOWN, command->accelerator().key_code());
}

// Base class for testing the CanExecuteScriptOnPage and CanCaptureVisiblePage
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
            Extension::GenerateIdForPath(
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
  }

  bool AllowedScript(const Extension* extension, const GURL& url,
                     const GURL& top_url) {
    return extension->CanExecuteScriptOnPage(url, top_url, -1, NULL, NULL);
  }

  bool BlockedScript(const Extension* extension, const GURL& url,
                     const GURL& top_url) {
    return !extension->CanExecuteScriptOnPage(url, top_url, -1, NULL, NULL);
  }

  bool Allowed(const Extension* extension, const GURL& url) {
    return Allowed(extension, url, -1);
  }

  bool Allowed(const Extension* extension, const GURL& url, int tab_id) {
    return (extension->CanExecuteScriptOnPage(url, url, tab_id, NULL, NULL) &&
            extension->CanCaptureVisiblePage(url, tab_id, NULL));
  }

  bool CaptureOnly(const Extension* extension, const GURL& url) {
    return CaptureOnly(extension, url, -1);
  }

  bool CaptureOnly(const Extension* extension, const GURL& url, int tab_id) {
    return !extension->CanExecuteScriptOnPage(url, url, tab_id, NULL, NULL) &&
            extension->CanCaptureVisiblePage(url, tab_id, NULL);
  }

  bool Blocked(const Extension* extension, const GURL& url) {
    return Blocked(extension, url, -1);
  }

  bool Blocked(const Extension* extension, const GURL& url, int tab_id) {
    return !(extension->CanExecuteScriptOnPage(url, url, tab_id, NULL, NULL) ||
             extension->CanCaptureVisiblePage(url, tab_id, NULL));
  }

  bool AllowedExclusivelyOnTab(
      const Extension* extension,
      const std::set<GURL>& allowed_urls,
      int tab_id) {
    bool result = true;
    for (std::set<GURL>::iterator it = urls_.begin(); it != urls_.end(); ++it) {
      const GURL& url = *it;
      if (allowed_urls.count(url))
        result &= Allowed(extension, url, tab_id);
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
  scoped_refptr<Extension> extension;

  // Test <all_urls> for regular extensions.
  extension = LoadManifestStrict("script_and_capture",
      "extension_regular_all.json");
  EXPECT_TRUE(Allowed(extension, http_url));
  EXPECT_TRUE(Allowed(extension, https_url));
  EXPECT_TRUE(Blocked(extension, file_url));
  EXPECT_TRUE(Blocked(extension, settings_url));
  EXPECT_TRUE(CaptureOnly(extension, favicon_url));
  EXPECT_TRUE(Blocked(extension, about_url));
  EXPECT_TRUE(Blocked(extension, extension_url));

  // Test access to iframed content.
  GURL within_extension_url = extension->GetResourceURL("page.html");
  EXPECT_TRUE(AllowedScript(extension, http_url, http_url_with_path));
  EXPECT_TRUE(AllowedScript(extension, https_url, http_url_with_path));
  EXPECT_TRUE(AllowedScript(extension, http_url, within_extension_url));
  EXPECT_TRUE(AllowedScript(extension, https_url, within_extension_url));
  EXPECT_TRUE(BlockedScript(extension, http_url, extension_url));
  EXPECT_TRUE(BlockedScript(extension, https_url, extension_url));

  EXPECT_FALSE(extension->HasHostPermission(settings_url));
  EXPECT_FALSE(extension->HasHostPermission(about_url));
  EXPECT_TRUE(extension->HasHostPermission(favicon_url));

  // Test * for scheme, which implies just the http/https schemes.
  extension = LoadManifestStrict("script_and_capture",
      "extension_wildcard.json");
  EXPECT_TRUE(Allowed(extension, http_url));
  EXPECT_TRUE(Allowed(extension, https_url));
  EXPECT_TRUE(Blocked(extension, settings_url));
  EXPECT_TRUE(Blocked(extension, about_url));
  EXPECT_TRUE(Blocked(extension, file_url));
  EXPECT_TRUE(Blocked(extension, favicon_url));
  extension = LoadManifest("script_and_capture",
      "extension_wildcard_settings.json");
  EXPECT_TRUE(Blocked(extension, settings_url));

  // Having chrome://*/ should not work for regular extensions. Note that
  // for favicon access, we require the explicit pattern chrome://favicon/*.
  std::string error;
  extension = LoadManifestUnchecked("script_and_capture",
                                    "extension_wildcard_chrome.json",
                                    Manifest::INTERNAL, Extension::NO_FLAGS,
                                    &error);
  EXPECT_TRUE(extension == NULL);
  EXPECT_EQ(ErrorUtils::FormatErrorMessage(
      errors::kInvalidPermissionScheme, "chrome://*/"), error);

  // Having chrome://favicon/* should not give you chrome://*
  extension = LoadManifestStrict("script_and_capture",
      "extension_chrome_favicon_wildcard.json");
  EXPECT_TRUE(Blocked(extension, settings_url));
  EXPECT_TRUE(CaptureOnly(extension, favicon_url));
  EXPECT_TRUE(Blocked(extension, about_url));
  EXPECT_TRUE(extension->HasHostPermission(favicon_url));

  // Having http://favicon should not give you chrome://favicon
  extension = LoadManifestStrict("script_and_capture",
      "extension_http_favicon.json");
  EXPECT_TRUE(Blocked(extension, settings_url));
  EXPECT_TRUE(Blocked(extension, favicon_url));

  // Component extensions with <all_urls> should get everything.
  extension = LoadManifest("script_and_capture", "extension_component_all.json",
      Manifest::COMPONENT, Extension::NO_FLAGS);
  EXPECT_TRUE(Allowed(extension, http_url));
  EXPECT_TRUE(Allowed(extension, https_url));
  EXPECT_TRUE(Allowed(extension, settings_url));
  EXPECT_TRUE(Allowed(extension, about_url));
  EXPECT_TRUE(Allowed(extension, favicon_url));
  EXPECT_TRUE(extension->HasHostPermission(favicon_url));

  // Component extensions should only get access to what they ask for.
  extension = LoadManifest("script_and_capture",
      "extension_component_google.json", Manifest::COMPONENT,
      Extension::NO_FLAGS);
  EXPECT_TRUE(Allowed(extension, http_url));
  EXPECT_TRUE(Blocked(extension, https_url));
  EXPECT_TRUE(Blocked(extension, file_url));
  EXPECT_TRUE(Blocked(extension, settings_url));
  EXPECT_TRUE(Blocked(extension, favicon_url));
  EXPECT_TRUE(Blocked(extension, about_url));
  EXPECT_TRUE(Blocked(extension, extension_url));
  EXPECT_FALSE(extension->HasHostPermission(settings_url));
}

TEST_F(ExtensionScriptAndCaptureVisibleTest, TabSpecific) {
  scoped_refptr<Extension> extension =
      LoadManifestStrict("script_and_capture", "tab_specific.json");

  EXPECT_FALSE(extension->GetTabSpecificPermissions(0).get());
  EXPECT_FALSE(extension->GetTabSpecificPermissions(1).get());
  EXPECT_FALSE(extension->GetTabSpecificPermissions(2).get());

  std::set<GURL> no_urls;

  EXPECT_TRUE(AllowedExclusivelyOnTab(extension, no_urls, 0));
  EXPECT_TRUE(AllowedExclusivelyOnTab(extension, no_urls, 1));
  EXPECT_TRUE(AllowedExclusivelyOnTab(extension, no_urls, 2));

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
        new PermissionSet(APIPermissionSet(), allowed_hosts, URLPatternSet()));
    extension->UpdateTabSpecificPermissions(0, permissions);
    EXPECT_EQ(permissions->explicit_hosts(),
              extension->GetTabSpecificPermissions(0)->explicit_hosts());
  }

  EXPECT_TRUE(AllowedExclusivelyOnTab(extension, allowed_urls, 0));
  EXPECT_TRUE(AllowedExclusivelyOnTab(extension, no_urls, 1));
  EXPECT_TRUE(AllowedExclusivelyOnTab(extension, no_urls, 2));

  extension->ClearTabSpecificPermissions(0);
  EXPECT_FALSE(extension->GetTabSpecificPermissions(0).get());

  EXPECT_TRUE(AllowedExclusivelyOnTab(extension, no_urls, 0));
  EXPECT_TRUE(AllowedExclusivelyOnTab(extension, no_urls, 1));
  EXPECT_TRUE(AllowedExclusivelyOnTab(extension, no_urls, 2));

  std::set<GURL> more_allowed_urls = allowed_urls;
  more_allowed_urls.insert(https_url);
  URLPatternSet more_allowed_hosts = allowed_hosts;
  more_allowed_hosts.AddPattern(URLPattern(URLPattern::SCHEME_ALL,
                                           https_url.spec()));

  {
    scoped_refptr<PermissionSet> permissions(
        new PermissionSet(APIPermissionSet(), allowed_hosts, URLPatternSet()));
    extension->UpdateTabSpecificPermissions(0, permissions);
    EXPECT_EQ(permissions->explicit_hosts(),
              extension->GetTabSpecificPermissions(0)->explicit_hosts());

    permissions = new PermissionSet(APIPermissionSet(),
                                    more_allowed_hosts,
                                    URLPatternSet());
    extension->UpdateTabSpecificPermissions(1, permissions);
    EXPECT_EQ(permissions->explicit_hosts(),
              extension->GetTabSpecificPermissions(1)->explicit_hosts());
  }

  EXPECT_TRUE(AllowedExclusivelyOnTab(extension, allowed_urls, 0));
  EXPECT_TRUE(AllowedExclusivelyOnTab(extension, more_allowed_urls, 1));
  EXPECT_TRUE(AllowedExclusivelyOnTab(extension, no_urls, 2));

  extension->ClearTabSpecificPermissions(0);
  EXPECT_FALSE(extension->GetTabSpecificPermissions(0).get());

  EXPECT_TRUE(AllowedExclusivelyOnTab(extension, no_urls, 0));
  EXPECT_TRUE(AllowedExclusivelyOnTab(extension, more_allowed_urls, 1));
  EXPECT_TRUE(AllowedExclusivelyOnTab(extension, no_urls, 2));

  extension->ClearTabSpecificPermissions(1);
  EXPECT_FALSE(extension->GetTabSpecificPermissions(1).get());

  EXPECT_TRUE(AllowedExclusivelyOnTab(extension, no_urls, 0));
  EXPECT_TRUE(AllowedExclusivelyOnTab(extension, no_urls, 1));
  EXPECT_TRUE(AllowedExclusivelyOnTab(extension, no_urls, 2));
}

TEST_F(ExtensionTest, GenerateId) {
  std::string result;
  EXPECT_TRUE(Extension::GenerateId("", &result));

  EXPECT_TRUE(Extension::GenerateId("test", &result));
  EXPECT_EQ(result, "jpignaibiiemhngfjkcpokkamffknabf");

  EXPECT_TRUE(Extension::GenerateId("_", &result));
  EXPECT_EQ(result, "ncocknphbhhlhkikpnnlmbcnbgdempcd");

  EXPECT_TRUE(Extension::GenerateId(
      "this_string_is_longer_than_a_single_sha256_hash_digest", &result));
  EXPECT_EQ(result, "jimneklojkjdibfkgiiophfhjhbdgcfi");
}

namespace {
enum SyncTestExtensionType {
  EXTENSION,
  APP,
  USER_SCRIPT,
  THEME
};

static scoped_refptr<Extension> MakeSyncTestExtension(
    SyncTestExtensionType type,
    const GURL& update_url,
    const GURL& launch_url,
    Manifest::Location location,
    int num_plugins,
    const base::FilePath& extension_path,
    int creation_flags) {
  DictionaryValue source;
  source.SetString(extension_manifest_keys::kName,
                   "PossiblySyncableExtension");
  source.SetString(extension_manifest_keys::kVersion, "0.0.0.0");
  if (type == APP)
    source.SetString(extension_manifest_keys::kApp, "true");
  if (type == THEME)
    source.Set(extension_manifest_keys::kTheme, new DictionaryValue());
  if (!update_url.is_empty()) {
    source.SetString(extension_manifest_keys::kUpdateURL,
                     update_url.spec());
  }
  if (!launch_url.is_empty()) {
    source.SetString(extension_manifest_keys::kLaunchWebURL,
                     launch_url.spec());
  }
  if (type != THEME) {
    source.SetBoolean(extension_manifest_keys::kConvertedFromUserScript,
                      type == USER_SCRIPT);
    ListValue* plugins = new ListValue();
    for (int i = 0; i < num_plugins; ++i) {
      DictionaryValue* plugin = new DictionaryValue();
      plugin->SetString(extension_manifest_keys::kPluginsPath, "");
      plugins->Set(i, plugin);
    }
    source.Set(extension_manifest_keys::kPlugins, plugins);
  }

  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(
      extension_path, location, source, creation_flags, &error);
  EXPECT_TRUE(extension);
  EXPECT_EQ("", error);
  return extension;
}

static const char kValidUpdateUrl1[] =
    "http://clients2.google.com/service/update2/crx";
static const char kValidUpdateUrl2[] =
    "https://clients2.google.com/service/update2/crx";
}

TEST_F(ExtensionTest, GetSyncTypeNormalExtensionNoUpdateUrl) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Manifest::INTERNAL, 0, base::FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_NE(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

// http://crbug.com/172712
TEST_F(ExtensionTest, DISABLED_GetSyncTypeUserScriptValidUpdateUrl) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(USER_SCRIPT, GURL(kValidUpdateUrl1), GURL(),
                            Manifest::INTERNAL, 0, base::FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_NE(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST_F(ExtensionTest, GetSyncTypeUserScriptNoUpdateUrl) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(USER_SCRIPT, GURL(), GURL(),
                            Manifest::INTERNAL, 0, base::FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_EQ(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST_F(ExtensionTest, GetSyncTypeThemeNoUpdateUrl) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(THEME, GURL(), GURL(),
                            Manifest::INTERNAL, 0, base::FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_EQ(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST_F(ExtensionTest, GetSyncTypeExtensionWithLaunchUrl) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL("http://www.google.com"),
                            Manifest::INTERNAL, 0, base::FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_NE(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST_F(ExtensionTest, GetSyncTypeExtensionExternal) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Manifest::EXTERNAL_PREF, 0, base::FilePath(),
                            Extension::NO_FLAGS));

  EXPECT_EQ(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST_F(ExtensionTest, GetSyncTypeUserScriptThirdPartyUpdateUrl) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(
          USER_SCRIPT, GURL("http://third-party.update_url.com"), GURL(),
          Manifest::INTERNAL, 0, base::FilePath(), Extension::NO_FLAGS));
  EXPECT_EQ(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST_F(ExtensionTest, OnlyDisplayAppsInLauncher) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Manifest::INTERNAL, 0, base::FilePath(),
                            Extension::NO_FLAGS));

  EXPECT_FALSE(extension->ShouldDisplayInAppLauncher());
  EXPECT_FALSE(extension->ShouldDisplayInNewTabPage());

  scoped_refptr<Extension> app(
      MakeSyncTestExtension(APP, GURL(), GURL("http://www.google.com"),
                            Manifest::INTERNAL, 0, base::FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_TRUE(app->ShouldDisplayInAppLauncher());
  EXPECT_TRUE(app->ShouldDisplayInNewTabPage());
}

TEST_F(ExtensionTest, DisplayInXManifestProperties) {
  DictionaryValue manifest;
  manifest.SetString(keys::kName, "TestComponentApp");
  manifest.SetString(keys::kVersion, "0.0.0.0");
  manifest.SetString(keys::kApp, "true");
  manifest.SetString(keys::kPlatformAppBackgroundPage, "");

  std::string error;
  scoped_refptr<Extension> app;

  // Default to true.
  app = Extension::Create(
      base::FilePath(), Manifest::COMPONENT, manifest, 0, &error);
  EXPECT_EQ(error, std::string());
  EXPECT_TRUE(app->ShouldDisplayInAppLauncher());
  EXPECT_TRUE(app->ShouldDisplayInNewTabPage());

  // Value display_in_NTP defaults to display_in_launcher.
  manifest.SetBoolean(keys::kDisplayInLauncher, false);
  app = Extension::Create(
      base::FilePath(), Manifest::COMPONENT, manifest, 0, &error);
  EXPECT_EQ(error, std::string());
  EXPECT_FALSE(app->ShouldDisplayInAppLauncher());
  EXPECT_FALSE(app->ShouldDisplayInNewTabPage());

  // Value display_in_NTP = true overriding display_in_launcher = false.
  manifest.SetBoolean(keys::kDisplayInNewTabPage, true);
  app = Extension::Create(
      base::FilePath(), Manifest::COMPONENT, manifest, 0, &error);
  EXPECT_EQ(error, std::string());
  EXPECT_FALSE(app->ShouldDisplayInAppLauncher());
  EXPECT_TRUE(app->ShouldDisplayInNewTabPage());

  // Value display_in_NTP = false only, overrides default = true.
  manifest.Remove(keys::kDisplayInLauncher, NULL);
  manifest.SetBoolean(keys::kDisplayInNewTabPage, false);
  app = Extension::Create(
      base::FilePath(), Manifest::COMPONENT, manifest, 0, &error);
  EXPECT_EQ(error, std::string());
  EXPECT_TRUE(app->ShouldDisplayInAppLauncher());
  EXPECT_FALSE(app->ShouldDisplayInNewTabPage());

  // Error checking.
  manifest.SetString(keys::kDisplayInNewTabPage, "invalid");
  app = Extension::Create(
      base::FilePath(), Manifest::COMPONENT, manifest, 0, &error);
  EXPECT_EQ(error, std::string(errors::kInvalidDisplayInNewTabPage));
}

TEST_F(ExtensionTest, OnlySyncInternal) {
  scoped_refptr<Extension> extension_internal(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Manifest::INTERNAL, 0, base::FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_TRUE(extension_internal->IsSyncable());

  scoped_refptr<Extension> extension_noninternal(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Manifest::COMPONENT, 0, base::FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_FALSE(extension_noninternal->IsSyncable());
}

TEST_F(ExtensionTest, DontSyncDefault) {
  scoped_refptr<Extension> extension_default(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Manifest::INTERNAL, 0, base::FilePath(),
                            Extension::WAS_INSTALLED_BY_DEFAULT));
  EXPECT_FALSE(extension_default->IsSyncable());
}

TEST_F(ExtensionTest, OptionalOnlyPermission) {
  // Set feature current channel to dev because the only permission that must
  // be optional (usbDevices) is only available on dev channel.
  Feature::ScopedCurrentChannel scoped_channel(
      chrome::VersionInfo::CHANNEL_DEV);

  scoped_refptr<Extension> extension;
  std::string error;
  extension = LoadManifestUnchecked("optional_only_permission",
                                    "manifest1.json",
                                    Manifest::INTERNAL, Extension::NO_FLAGS,
                                    &error);
  EXPECT_TRUE(extension == NULL);
  ASSERT_EQ(ErrorUtils::FormatErrorMessage(
        errors::kPermissionMustBeOptional, "usbDevices"), error);

  error.clear();
  extension = LoadManifestUnchecked("optional_only_permission",
                                    "manifest2.json",
                                    Manifest::INTERNAL, Extension::NO_FLAGS,
                                    &error);
  EXPECT_TRUE(extension != NULL);
  EXPECT_TRUE(error.empty());
}

// These last 2 tests don't make sense on Chrome OS, where extension plugins
// are not allowed.
#if !defined(OS_CHROMEOS)
TEST_F(ExtensionTest, GetSyncTypeExtensionWithPlugin) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Manifest::INTERNAL, 1, base::FilePath(),
                            Extension::NO_FLAGS));
  if (extension)
    EXPECT_EQ(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST_F(ExtensionTest, GetSyncTypeExtensionWithTwoPlugins) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Manifest::INTERNAL, 2, base::FilePath(),
                            Extension::NO_FLAGS));
  if (extension)
    EXPECT_EQ(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}
#endif // !defined(OS_CHROMEOS)

}  // namespace extensions
