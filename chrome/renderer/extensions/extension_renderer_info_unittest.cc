// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/renderer/extensions/extension_renderer_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

scoped_refptr<Extension> CreateTestExtension(const std::string& name,
                                             const std::string& launch_url,
                                             const std::string& extent) {
#if defined(OS_WIN)
  FilePath path(FILE_PATH_LITERAL("c:\\"));
#else
  FilePath path(FILE_PATH_LITERAL("/"));
#endif
  path = path.AppendASCII(name);

  DictionaryValue manifest;
  manifest.SetString("name", name);
  manifest.SetString("version", "1");

  if (!launch_url.empty())
    manifest.SetString("app.launch.web_url", launch_url);

  if (!extent.empty()) {
    ListValue* urls = new ListValue();
    manifest.Set("app.urls", urls);
    urls->Append(Value::CreateStringValue(extent));
  }

  const bool kRequireKey = false;
  std::string error;
  scoped_refptr<Extension> extension(
      Extension::Create(path, Extension::INTERNAL, manifest, kRequireKey,
                        &error));
  EXPECT_TRUE(extension.get()) << error;
  return extension;
}

} // namespace

TEST(ExtensionRendererInfoTest, ExtensionRendererInfo) {
  scoped_refptr<Extension> ext1(CreateTestExtension(
      "a", "https://chrome.google.com/launch", "https://chrome.google.com/"));

  scoped_refptr<Extension> ext2(CreateTestExtension(
      "a", "http://code.google.com/p/chromium",
      "http://code.google.com/p/chromium/"));

  scoped_refptr<Extension> ext3(CreateTestExtension(
      "b", "http://dev.chromium.org/", "http://dev.chromium.org/"));

  scoped_refptr<Extension> ext4(CreateTestExtension("c", "", ""));

  ASSERT_TRUE(ext1 && ext2 && ext3 && ext4);

  ExtensionRendererInfo extensions;

  // Add an extension.
  extensions.Update(ext1);
  EXPECT_EQ(1u, extensions.size());
  EXPECT_EQ(ext1, extensions.GetByID(ext1->id()));

  // Since extension2 has same ID, it should overwrite extension1.
  extensions.Update(ext2);
  EXPECT_EQ(1u, extensions.size());
  EXPECT_EQ(ext2, extensions.GetByID(ext1->id()));

  // Add the other extensions.
  extensions.Update(ext3);
  extensions.Update(ext4);
  EXPECT_EQ(3u, extensions.size());

  // Get extension by its chrome-extension:// URL
  EXPECT_EQ(ext2, extensions.GetByURL(
      ext2->GetResourceURL("test.html")));
  EXPECT_EQ(ext3, extensions.GetByURL(
      ext3->GetResourceURL("test.html")));
  EXPECT_EQ(ext4, extensions.GetByURL(
      ext4->GetResourceURL("test.html")));

  // Get extension by web extent.
  EXPECT_EQ(ext2, extensions.GetByURL(
      GURL("http://code.google.com/p/chromium/monkey")));
  EXPECT_EQ(ext3, extensions.GetByURL(
    GURL("http://dev.chromium.org/design-docs/")));
  EXPECT_FALSE(extensions.GetByURL(
      GURL("http://blog.chromium.org/")));

  // Test InSameExtent().
  EXPECT_TRUE(extensions.InSameExtent(
      GURL("http://code.google.com/p/chromium/monkey/"),
      GURL("http://code.google.com/p/chromium/")));
  EXPECT_FALSE(extensions.InSameExtent(
      GURL("http://code.google.com/p/chromium/"),
      GURL("https://code.google.com/p/chromium/")));
  EXPECT_FALSE(extensions.InSameExtent(
      GURL("http://code.google.com/p/chromium/"),
      GURL("http://dev.chromium.org/design-docs/")));

  // Both of these should be NULL, which mean true for InSameExtent.
  EXPECT_TRUE(extensions.InSameExtent(
      GURL("http://www.google.com/"),
      GURL("http://blog.chromium.org/")));

  // Remove one of the extensions.
  extensions.Remove(ext2->id());
  EXPECT_EQ(2u, extensions.size());
  EXPECT_FALSE(extensions.GetByID(ext2->id()));
}
