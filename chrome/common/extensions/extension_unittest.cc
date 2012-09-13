// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension.h"

#include "base/format_macros.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/command.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/extensions/features/feature.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "chrome/common/extensions/permissions/socket_permission.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "net/base/mime_sniffer.h"
#include "skia/ext/image_operations.h"
#include "net/base/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

using extensions::APIPermission;
using extensions::APIPermissionSet;
using extensions::Extension;
using extensions::Feature;
using extensions::PermissionSet;
using extensions::SocketPermission;
using extensions::SocketPermissionData;

namespace keys = extension_manifest_keys;
namespace values = extension_manifest_values;
namespace errors = extension_manifest_errors;

namespace {

void CompareLists(const std::vector<std::string>& expected,
                  const std::vector<std::string>& actual) {
  ASSERT_EQ(expected.size(), actual.size());

  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected[i], actual[i]);
  }
}

static scoped_refptr<Extension> LoadManifestUnchecked(
    const std::string& dir,
    const std::string& test_file,
    Extension::Location location,
    int extra_flags,
    std::string* error) {
  FilePath path;
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
                                             Extension::Location location,
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
  return LoadManifest(dir, test_file, Extension::INVALID, extra_flags);
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

static scoped_ptr<ExtensionAction> LoadAction(const std::string& manifest) {
  scoped_refptr<Extension> extension = LoadManifest("page_action",
      manifest);
  return extension->page_action()->CopyForTest();
}

static void LoadActionAndExpectError(const std::string& manifest,
                                     const std::string& expected_error) {
  std::string error;
  scoped_refptr<Extension> extension = LoadManifestUnchecked("page_action",
      manifest, Extension::INTERNAL, Extension::NO_FLAGS, &error);
  EXPECT_FALSE(extension);
  EXPECT_EQ(expected_error, error);
}

}

class ExtensionTest : public testing::Test {
};

// We persist location values in the preferences, so this is a sanity test that
// someone doesn't accidentally change them.
TEST(ExtensionTest, LocationValuesTest) {
  ASSERT_EQ(0, Extension::INVALID);
  ASSERT_EQ(1, Extension::INTERNAL);
  ASSERT_EQ(2, Extension::EXTERNAL_PREF);
  ASSERT_EQ(3, Extension::EXTERNAL_REGISTRY);
  ASSERT_EQ(4, Extension::LOAD);
  ASSERT_EQ(5, Extension::COMPONENT);
  ASSERT_EQ(6, Extension::EXTERNAL_PREF_DOWNLOAD);
  ASSERT_EQ(7, Extension::EXTERNAL_POLICY_DOWNLOAD);
}

TEST(ExtensionTest, LocationPriorityTest) {
  for (int i = 0; i < Extension::NUM_LOCATIONS; i++) {
    Extension::Location loc = static_cast<Extension::Location>(i);

    // INVALID is not a valid location.
    if (loc == Extension::INVALID)
      continue;

    // Comparing a location that has no rank will hit a CHECK. Do a
    // compare with every valid location, to be sure each one is covered.

    // Check that no install source can override a componenet extension.
    ASSERT_EQ(Extension::COMPONENT,
              Extension::GetHigherPriorityLocation(Extension::COMPONENT, loc));
    ASSERT_EQ(Extension::COMPONENT,
              Extension::GetHigherPriorityLocation(loc, Extension::COMPONENT));

    // Check that any source can override a user install. This might change
    // in the future, in which case this test should be updated.
    ASSERT_EQ(loc,
              Extension::GetHigherPriorityLocation(Extension::INTERNAL, loc));
    ASSERT_EQ(loc,
              Extension::GetHigherPriorityLocation(loc, Extension::INTERNAL));
  }

  // Check a few interesting cases that we know can happen:
  ASSERT_EQ(Extension::EXTERNAL_POLICY_DOWNLOAD,
            Extension::GetHigherPriorityLocation(
                Extension::EXTERNAL_POLICY_DOWNLOAD,
                Extension::EXTERNAL_PREF));

  ASSERT_EQ(Extension::EXTERNAL_PREF,
            Extension::GetHigherPriorityLocation(
                Extension::INTERNAL,
                Extension::EXTERNAL_PREF));
}

TEST(ExtensionTest, GetResourceURLAndPath) {
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

TEST(ExtensionTest, GetAbsolutePathNoError) {
  scoped_refptr<Extension> extension = LoadManifestStrict("absolute_path",
      "absolute.json");
  EXPECT_TRUE(extension.get());
  std::string err;
  Extension::InstallWarningVector warnings;
  EXPECT_TRUE(extension_file_util::ValidateExtension(extension.get(),
                                                     &err, &warnings));
  EXPECT_EQ(0U, warnings.size());

  EXPECT_EQ(extension->path().AppendASCII("test.html").value(),
            extension->GetResource("test.html").GetFilePath().value());
  EXPECT_EQ(extension->path().AppendASCII("test.js").value(),
            extension->GetResource("test.js").GetFilePath().value());
}

TEST(ExtensionTest, LoadPageActionHelper) {
  scoped_ptr<ExtensionAction> action;

  // First try with an empty dictionary.
  action = LoadAction("page_action_empty.json");
  ASSERT_TRUE(action != NULL);

  // Now setup some values to use in the action.
  const std::string id("MyExtensionActionId");
  const std::string name("MyExtensionActionName");
  std::string img1("image1.png");

  action = LoadAction("page_action.json");
  ASSERT_TRUE(NULL != action.get());
  ASSERT_EQ(id, action->id());

  // No title, so fall back to name.
  ASSERT_EQ(name, action->GetTitle(1));
  ASSERT_EQ(img1, action->default_icon_path());

  // Same test with explicitly set type.
  action = LoadAction("page_action_type.json");
  ASSERT_TRUE(NULL != action.get());

  // Try an action without id key.
  action = LoadAction("page_action_no_id.json");
  ASSERT_TRUE(NULL != action.get());

  // Then try without the name key. It's optional, so no error.
  action = LoadAction("page_action_no_name.json");
  ASSERT_TRUE(NULL != action.get());
  ASSERT_TRUE(action->GetTitle(1).empty());

  // Then try without the icon paths key.
  action = LoadAction("page_action_no_icon.json");
  ASSERT_TRUE(NULL != action.get());

  // Now test that we can parse the new format for page actions.
  const std::string kTitle("MyExtensionActionTitle");
  const std::string kIcon("image1.png");
  const std::string kPopupHtmlFile("a_popup.html");

  action = LoadAction("page_action_new_format.json");
  ASSERT_TRUE(action.get());
  ASSERT_EQ(kTitle, action->GetTitle(1));
  ASSERT_FALSE(action->default_icon_path().empty());

  // Invalid title should give an error even with a valid name.
  LoadActionAndExpectError("page_action_invalid_title.json",
      errors::kInvalidPageActionDefaultTitle);

  // Invalid name should give an error only with no title.
  action = LoadAction("page_action_invalid_name.json");
  ASSERT_TRUE(NULL != action.get());
  ASSERT_EQ(kTitle, action->GetTitle(1));

  LoadActionAndExpectError("page_action_invalid_name_no_title.json",
      errors::kInvalidPageActionName);

  // Test that keys "popup" and "default_popup" both work, but can not
  // be used at the same time.
  // These tests require an extension_url, so we also load the manifest.

  // Only use "popup", expect success.
  scoped_refptr<Extension> extension = LoadManifest("page_action",
             "page_action_popup.json");
  action = LoadAction("page_action_popup.json");
  ASSERT_TRUE(NULL != action.get());
  ASSERT_STREQ(
      extension->url().Resolve(kPopupHtmlFile).spec().c_str(),
      action->GetPopupUrl(ExtensionAction::kDefaultTabId).spec().c_str());

  // Use both "popup" and "default_popup", expect failure.
  LoadActionAndExpectError("page_action_popup_and_default_popup.json",
      ExtensionErrorUtils::FormatErrorMessage(
          errors::kInvalidPageActionOldAndNewKeys,
          keys::kPageActionDefaultPopup,
          keys::kPageActionPopup));

  // Use only "default_popup", expect success.
  extension = LoadManifest("page_action", "page_action_popup.json");
  action = LoadAction("page_action_default_popup.json");
  ASSERT_TRUE(NULL != action.get());
  ASSERT_STREQ(
      extension->url().Resolve(kPopupHtmlFile).spec().c_str(),
      action->GetPopupUrl(ExtensionAction::kDefaultTabId).spec().c_str());

  // Setting default_popup to "" is the same as having no popup.
  action = LoadAction("page_action_empty_default_popup.json");
  ASSERT_TRUE(NULL != action.get());
  EXPECT_FALSE(action->HasPopup(ExtensionAction::kDefaultTabId));
  ASSERT_STREQ(
      "",
      action->GetPopupUrl(ExtensionAction::kDefaultTabId).spec().c_str());

  // Setting popup to "" is the same as having no popup.
  action = LoadAction("page_action_empty_popup.json");

  ASSERT_TRUE(NULL != action.get());
  EXPECT_FALSE(action->HasPopup(ExtensionAction::kDefaultTabId));
  ASSERT_STREQ(
      "",
      action->GetPopupUrl(ExtensionAction::kDefaultTabId).spec().c_str());
}

TEST(ExtensionTest, IdIsValid) {
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

TEST(ExtensionTest, GenerateID) {
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
TEST(ExtensionTest, MimeTypeSniffing) {
  FilePath path;
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

TEST(ExtensionTest, EffectiveHostPermissions) {
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
    SocketPermissionData::OperationType type,
    const char* host,
    int port) {
  SocketPermission::CheckParam param(type, host, port);
  return extension->CheckAPIPermissionWithParam(
      APIPermission::kSocket, &param);
}

TEST(ExtensionTest, SocketPermissions) {
  // Set feature current channel to appropriate value.
  Feature::ScopedCurrentChannel scoped_channel(
      chrome::VersionInfo::GetChannel());
  scoped_refptr<Extension> extension;
  std::string error;

  extension = LoadManifest("socket_permissions", "empty.json");
  EXPECT_FALSE(CheckSocketPermission(
        extension, SocketPermissionData::TCP_CONNECT, "www.example.com", 80));

  extension = LoadManifestUnchecked("socket_permissions",
                                    "socket1.json",
                                    Extension::INTERNAL, Extension::NO_FLAGS,
                                    &error);
  EXPECT_TRUE(extension == NULL);
  ASSERT_EQ(ExtensionErrorUtils::FormatErrorMessage(
        errors::kInvalidPermission, base::IntToString(0)), error);

  extension = LoadManifest("socket_permissions", "socket2.json");
  EXPECT_TRUE(CheckSocketPermission(
        extension, SocketPermissionData::TCP_CONNECT, "www.example.com", 80));
  EXPECT_FALSE(CheckSocketPermission(
        extension, SocketPermissionData::UDP_BIND, "", 80));
  EXPECT_TRUE(CheckSocketPermission(
        extension, SocketPermissionData::UDP_BIND, "", 8888));

  EXPECT_FALSE(CheckSocketPermission(
        extension, SocketPermissionData::UDP_SEND_TO, "example.com", 1900));
  EXPECT_TRUE(CheckSocketPermission(
        extension, SocketPermissionData::UDP_SEND_TO, "239.255.255.250", 1900));
}

// Returns a copy of |source| resized to |size| x |size|.
static SkBitmap ResizedCopy(const SkBitmap& source, int size) {
  return skia::ImageOperations::Resize(source,
                                       skia::ImageOperations::RESIZE_LANCZOS3,
                                       size,
                                       size);
}

static bool SizeEquals(const SkBitmap& bitmap, const gfx::Size& size) {
  return bitmap.width() == size.width() && bitmap.height() == size.height();
}

TEST(ExtensionTest, ImageCaching) {
  FilePath path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("extensions");

  // Initialize the Extension.
  std::string errors;
  DictionaryValue values;
  values.SetString(keys::kName, "test");
  values.SetString(keys::kVersion, "0.1");
  scoped_refptr<Extension> extension(Extension::Create(
      path, Extension::INVALID, values, Extension::NO_FLAGS, &errors));
  ASSERT_TRUE(extension.get());

  // Create an ExtensionResource pointing at an icon.
  FilePath icon_relative_path(FILE_PATH_LITERAL("icon3.png"));
  ExtensionResource resource(extension->id(),
                             extension->path(),
                             icon_relative_path);

  // Read in the icon file.
  FilePath icon_absolute_path = extension->path().Append(icon_relative_path);
  std::string raw_png;
  ASSERT_TRUE(file_util::ReadFileToString(icon_absolute_path, &raw_png));
  SkBitmap image;
  ASSERT_TRUE(gfx::PNGCodec::Decode(
      reinterpret_cast<const unsigned char*>(raw_png.data()),
      raw_png.length(),
      &image));

  // Make sure the icon file is the size we expect.
  gfx::Size original_size(66, 66);
  ASSERT_EQ(image.width(), original_size.width());
  ASSERT_EQ(image.height(), original_size.height());

  // Create two resized versions at size 16x16 and 24x24.
  SkBitmap image16 = ResizedCopy(image, 16);
  SkBitmap image24 = ResizedCopy(image, 24);

  gfx::Size size16(16, 16);
  gfx::Size size24(24, 24);

  // Cache the 16x16 copy.
  EXPECT_FALSE(extension->HasCachedImage(resource, size16));
  extension->SetCachedImage(resource, image16, original_size);
  EXPECT_TRUE(extension->HasCachedImage(resource, size16));
  EXPECT_TRUE(SizeEquals(extension->GetCachedImage(resource, size16), size16));
  EXPECT_FALSE(extension->HasCachedImage(resource, size24));
  EXPECT_FALSE(extension->HasCachedImage(resource, original_size));

  // Cache the 24x24 copy.
  extension->SetCachedImage(resource, image24, original_size);
  EXPECT_TRUE(extension->HasCachedImage(resource, size24));
  EXPECT_TRUE(SizeEquals(extension->GetCachedImage(resource, size24), size24));
  EXPECT_FALSE(extension->HasCachedImage(resource, original_size));

  // Cache the original, and verify that it gets returned when we ask for a
  // max_size that is larger than the original.
  gfx::Size size128(128, 128);
  EXPECT_TRUE(image.width() < size128.width() &&
              image.height() < size128.height());
  extension->SetCachedImage(resource, image, original_size);
  EXPECT_TRUE(extension->HasCachedImage(resource, original_size));
  EXPECT_TRUE(extension->HasCachedImage(resource, size128));
  EXPECT_TRUE(SizeEquals(extension->GetCachedImage(resource, original_size),
                         original_size));
  EXPECT_TRUE(SizeEquals(extension->GetCachedImage(resource, size128),
                         original_size));
  EXPECT_EQ(extension->GetCachedImage(resource, original_size).getPixels(),
            extension->GetCachedImage(resource, size128).getPixels());
}

// This tests the API permissions with an empty manifest (one that just
// specifies a name and a version and nothing else).
TEST(ExtensionTest, ApiPermissions) {
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

TEST(ExtensionTest, GetPermissionMessages_ManyApiPermissions) {
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

TEST(ExtensionTest, GetPermissionMessages_ManyHosts) {
  scoped_refptr<Extension> extension;
  extension = LoadManifest("permissions", "many-hosts.json");
  std::vector<string16> warnings = extension->GetPermissionMessageStrings();
  ASSERT_EQ(1u, warnings.size());
  EXPECT_EQ("Access your data on encrypted.google.com and www.google.com",
            UTF16ToUTF8(warnings[0]));
}

TEST(ExtensionTest, GetPermissionMessages_Plugins) {
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

TEST(ExtensionTest, WantsFileAccess) {
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

TEST(ExtensionTest, ExtraFlags) {
  scoped_refptr<Extension> extension;
  extension = LoadManifest("app", "manifest.json", Extension::FROM_WEBSTORE);
  EXPECT_TRUE(extension->from_webstore());

  extension = LoadManifest("app", "manifest.json", Extension::FROM_BOOKMARK);
  EXPECT_TRUE(extension->from_bookmark());

  extension = LoadManifest("app", "manifest.json", Extension::NO_FLAGS);
  EXPECT_FALSE(extension->from_bookmark());
  EXPECT_FALSE(extension->from_webstore());
}

TEST(ExtensionTest, BrowserActionSynthesizesCommand) {
  scoped_refptr<Extension> extension;

  extension = LoadManifest("api_test/browser_action/synthesized",
                           "manifest.json");
  // An extension with a browser action but no extension command specified
  // should get a command assigned to it.
  const extensions::Command* command = extension->browser_action_command();
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
            Extension::GenerateIdForPath(FilePath(FILE_PATH_LITERAL("foo")))),
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
                                    Extension::INTERNAL, Extension::NO_FLAGS,
                                    &error);
  EXPECT_TRUE(extension == NULL);
  EXPECT_EQ(ExtensionErrorUtils::FormatErrorMessage(
      errors::kInvalidPermissionScheme, base::IntToString(1)), error);

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
      Extension::COMPONENT, Extension::NO_FLAGS);
  EXPECT_TRUE(Allowed(extension, http_url));
  EXPECT_TRUE(Allowed(extension, https_url));
  EXPECT_TRUE(Allowed(extension, settings_url));
  EXPECT_TRUE(Allowed(extension, about_url));
  EXPECT_TRUE(Allowed(extension, favicon_url));
  EXPECT_TRUE(extension->HasHostPermission(favicon_url));

  // Component extensions should only get access to what they ask for.
  extension = LoadManifest("script_and_capture",
      "extension_component_google.json", Extension::COMPONENT,
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

TEST(ExtensionTest, GenerateId) {
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
    Extension::Location location,
    int num_plugins,
    const FilePath& extension_path,
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

TEST(ExtensionTest, GetSyncTypeNormalExtensionNoUpdateUrl) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Extension::INTERNAL, 0, FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_NE(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST(ExtensionTest, GetSyncTypeUserScriptValidUpdateUrl) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(USER_SCRIPT, GURL(kValidUpdateUrl1), GURL(),
                            Extension::INTERNAL, 0, FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_NE(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST(ExtensionTest, GetSyncTypeUserScriptNoUpdateUrl) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(USER_SCRIPT, GURL(), GURL(),
                            Extension::INTERNAL, 0, FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_EQ(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST(ExtensionTest, GetSyncTypeThemeNoUpdateUrl) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(THEME, GURL(), GURL(),
                            Extension::INTERNAL, 0, FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_EQ(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST(ExtensionTest, GetSyncTypeExtensionWithLaunchUrl) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL("http://www.google.com"),
                            Extension::INTERNAL, 0, FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_NE(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST(ExtensionTest, GetSyncTypeExtensionExternal) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Extension::EXTERNAL_PREF, 0, FilePath(),
                            Extension::NO_FLAGS));

  EXPECT_EQ(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST(ExtensionTest, GetSyncTypeUserScriptThirdPartyUpdateUrl) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(
          USER_SCRIPT, GURL("http://third-party.update_url.com"), GURL(),
          Extension::INTERNAL, 0, FilePath(), Extension::NO_FLAGS));
  EXPECT_EQ(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST(ExtensionTest, OnlyDisplayAppsInLauncher) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Extension::INTERNAL, 0, FilePath(),
                            Extension::NO_FLAGS));

  EXPECT_FALSE(extension->ShouldDisplayInLauncher());

  scoped_refptr<Extension> app(
      MakeSyncTestExtension(APP, GURL(), GURL("http://www.google.com"),
                            Extension::INTERNAL, 0, FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_TRUE(app->ShouldDisplayInLauncher());
}

TEST(ExtensionTest, OnlySyncInternal) {
  scoped_refptr<Extension> extension_internal(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Extension::INTERNAL, 0, FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_TRUE(extension_internal->IsSyncable());

  scoped_refptr<Extension> extension_noninternal(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Extension::COMPONENT, 0, FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_FALSE(extension_noninternal->IsSyncable());
}

TEST(ExtensionTest, DontSyncDefault) {
  scoped_refptr<Extension> extension_default(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Extension::INTERNAL, 0, FilePath(),
                            Extension::WAS_INSTALLED_BY_DEFAULT));
  EXPECT_FALSE(extension_default->IsSyncable());
}

// These last 2 tests don't make sense on Chrome OS, where extension plugins
// are not allowed.
#if !defined(OS_CHROMEOS)
TEST(ExtensionTest, GetSyncTypeExtensionWithPlugin) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Extension::INTERNAL, 1, FilePath(),
                            Extension::NO_FLAGS));
  if (extension)
    EXPECT_EQ(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST(ExtensionTest, GetSyncTypeExtensionWithTwoPlugins) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Extension::INTERNAL, 2, FilePath(),
                            Extension::NO_FLAGS));
  if (extension)
    EXPECT_EQ(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}
#endif // !defined(OS_CHROMEOS)
