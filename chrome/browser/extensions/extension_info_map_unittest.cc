// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/json_value_serializer.h"
#include "content/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace keys = extension_manifest_keys;

namespace {

class ExtensionInfoMapTest : public testing::Test {
 public:
  ExtensionInfoMapTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {
  }

 private:
  MessageLoop message_loop_;
  BrowserThread ui_thread_;
  BrowserThread io_thread_;
};

// Returns a barebones test Extension object with the given name.
static scoped_refptr<Extension> CreateExtension(const std::string& name) {
#if defined(OS_WIN)
  FilePath path(FILE_PATH_LITERAL("c:\\foo"));
#elif defined(OS_POSIX)
  FilePath path(FILE_PATH_LITERAL("/foo"));
#endif

  DictionaryValue manifest;
  manifest.SetString(keys::kVersion, "1.0.0.0");
  manifest.SetString(keys::kName, name);

  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(
      path.AppendASCII(name), Extension::INVALID, manifest, false, true,
      &error);
  EXPECT_TRUE(extension) << error;

  return extension;
}

static scoped_refptr<Extension> LoadManifest(const std::string& dir,
                                             const std::string& test_file) {
  FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("extensions")
             .AppendASCII(dir)
             .AppendASCII(test_file);

  JSONFileValueSerializer serializer(path);
  scoped_ptr<Value> result(serializer.Deserialize(NULL, NULL));
  if (!result.get())
    return NULL;

  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(
      path, Extension::INVALID, *static_cast<DictionaryValue*>(result.get()),
      false, true, &error);
  EXPECT_TRUE(extension) << error;

  return extension;
}

// Test that the ExtensionInfoMap handles refcounting properly.
TEST_F(ExtensionInfoMapTest, RefCounting) {
  scoped_refptr<ExtensionInfoMap> info_map(new ExtensionInfoMap());

  // New extensions should have a single reference holding onto them.
  scoped_refptr<Extension> extension1(CreateExtension("extension1"));
  scoped_refptr<Extension> extension2(CreateExtension("extension2"));
  scoped_refptr<Extension> extension3(CreateExtension("extension3"));
  EXPECT_TRUE(extension1->HasOneRef());
  EXPECT_TRUE(extension2->HasOneRef());
  EXPECT_TRUE(extension3->HasOneRef());

  // Add a ref to each extension and give it to the info map. The info map
  // expects the caller to add a ref for it, but then assumes ownership of that
  // reference.
  extension1->AddRef();
  info_map->AddExtension(extension1);
  extension2->AddRef();
  info_map->AddExtension(extension2);
  extension3->AddRef();
  info_map->AddExtension(extension3);

  // Release extension1, and the info map should have the only ref.
  const Extension* weak_extension1 = extension1;
  extension1 = NULL;
  EXPECT_TRUE(weak_extension1->HasOneRef());

  // Remove extension2, and the extension2 object should have the only ref.
  info_map->RemoveExtension(extension2->id());
  EXPECT_TRUE(extension2->HasOneRef());

  // Delete the info map, and the extension3 object should have the only ref.
  info_map = NULL;
  EXPECT_TRUE(extension3->HasOneRef());
}

// Tests that we can query a few extension properties from the ExtensionInfoMap.
TEST_F(ExtensionInfoMapTest, Properties) {
  scoped_refptr<ExtensionInfoMap> info_map(new ExtensionInfoMap());

  scoped_refptr<Extension> extension1(CreateExtension("extension1"));
  scoped_refptr<Extension> extension2(CreateExtension("extension2"));

  extension1->AddRef();
  info_map->AddExtension(extension1);
  extension2->AddRef();
  info_map->AddExtension(extension2);

  EXPECT_EQ(extension1->name(),
            info_map->GetNameForExtension(extension1->id()));
  EXPECT_EQ(extension2->name(),
            info_map->GetNameForExtension(extension2->id()));

  EXPECT_EQ(extension1->path().value(),
            info_map->GetPathForExtension(extension1->id()).value());
  EXPECT_EQ(extension2->path().value(),
            info_map->GetPathForExtension(extension2->id()).value());
}

// Tests CheckURLAccessToExtensionPermission given both extension and app URLs.
TEST_F(ExtensionInfoMapTest, CheckPermissions) {
  scoped_refptr<ExtensionInfoMap> info_map(new ExtensionInfoMap());

  scoped_refptr<Extension> app(LoadManifest("manifest_tests",
                                         "valid_app.json"));
  scoped_refptr<Extension> extension(LoadManifest("manifest_tests",
                                               "tabs_extension.json"));

  GURL app_url("http://www.google.com/mail/foo.html");
  ASSERT_TRUE(app->is_app());
  ASSERT_TRUE(app->web_extent().ContainsURL(app_url));

  app->AddRef();
  info_map->AddExtension(app);
  extension->AddRef();
  info_map->AddExtension(extension);

  // The app should have the notifications permission, either from a
  // chrome-extension URL or from its web extent.
  EXPECT_TRUE(info_map->CheckURLAccessToExtensionPermission(
      app->GetResourceURL("a.html"), Extension::kNotificationPermission));
  EXPECT_TRUE(info_map->CheckURLAccessToExtensionPermission(
      app_url, Extension::kNotificationPermission));
  EXPECT_FALSE(info_map->CheckURLAccessToExtensionPermission(
      app_url, Extension::kTabPermission));

  // The extension should have the tabs permission.
  EXPECT_TRUE(info_map->CheckURLAccessToExtensionPermission(
      extension->GetResourceURL("a.html"), Extension::kTabPermission));
  EXPECT_FALSE(info_map->CheckURLAccessToExtensionPermission(
      extension->GetResourceURL("a.html"), Extension::kNotificationPermission));

  // Random URL should not have any permissions.
  EXPECT_FALSE(info_map->CheckURLAccessToExtensionPermission(
      GURL("http://evil.com/a.html"), Extension::kNotificationPermission));
  EXPECT_FALSE(info_map->CheckURLAccessToExtensionPermission(
      GURL("http://evil.com/a.html"), Extension::kTabPermission));
}

}  // namespace
