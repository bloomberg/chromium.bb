// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;

namespace {

scoped_refptr<Extension> CreateTestExtension(const std::string& name,
                                             const std::string& launch_url,
                                             const std::string& extent) {
#if defined(OS_WIN)
  base::FilePath path(FILE_PATH_LITERAL("c:\\"));
#else
  base::FilePath path(FILE_PATH_LITERAL("/"));
#endif
  path = path.AppendASCII(name);

  base::DictionaryValue manifest;
  manifest.SetString("name", name);
  manifest.SetString("version", "1");

  if (!launch_url.empty())
    manifest.SetString("app.launch.web_url", launch_url);

  if (!extent.empty()) {
    base::ListValue* urls = new base::ListValue();
    manifest.Set("app.urls", urls);
    urls->Append(new base::StringValue(extent));
  }

  std::string error;
  scoped_refptr<Extension> extension(
      Extension::Create(path, extensions::Manifest::INTERNAL, manifest,
                        Extension::NO_FLAGS, &error));
  EXPECT_TRUE(extension.get()) << error;
  return extension;
}

}  // namespace

TEST(ExtensionSetTest, ExtensionSet) {
  scoped_refptr<Extension> ext1(CreateTestExtension(
      "a", "https://chrome.google.com/launch", "https://chrome.google.com/"));

  scoped_refptr<Extension> ext2(CreateTestExtension(
      "a", "http://code.google.com/p/chromium",
      "http://code.google.com/p/chromium/"));

  scoped_refptr<Extension> ext3(CreateTestExtension(
      "b", "http://dev.chromium.org/", "http://dev.chromium.org/"));

  scoped_refptr<Extension> ext4(CreateTestExtension("c", "", ""));

  ASSERT_TRUE(ext1 && ext2 && ext3 && ext4);

  ExtensionSet extensions;

  // Add an extension.
  extensions.Insert(ext1);
  EXPECT_EQ(1u, extensions.size());
  EXPECT_EQ(ext1, extensions.GetByID(ext1->id()));

  // Since extension2 has same ID, it should overwrite extension1.
  extensions.Insert(ext2);
  EXPECT_EQ(1u, extensions.size());
  EXPECT_EQ(ext2, extensions.GetByID(ext1->id()));

  // Add the other extensions.
  extensions.Insert(ext3);
  extensions.Insert(ext4);
  EXPECT_EQ(3u, extensions.size());

  // Get extension by its chrome-extension:// URL
  EXPECT_EQ(ext2, extensions.GetExtensionOrAppByURL(
      ExtensionURLInfo(ext2->GetResourceURL("test.html"))));
  EXPECT_EQ(ext3, extensions.GetExtensionOrAppByURL(
      ExtensionURLInfo(ext3->GetResourceURL("test.html"))));
  EXPECT_EQ(ext4, extensions.GetExtensionOrAppByURL(
      ExtensionURLInfo(ext4->GetResourceURL("test.html"))));

  // Get extension by web extent.
  EXPECT_EQ(ext2, extensions.GetExtensionOrAppByURL(
      ExtensionURLInfo(GURL("http://code.google.com/p/chromium/monkey"))));
  EXPECT_EQ(ext3, extensions.GetExtensionOrAppByURL(
      ExtensionURLInfo(GURL("http://dev.chromium.org/design-docs/"))));
  EXPECT_FALSE(extensions.GetExtensionOrAppByURL(
      ExtensionURLInfo(GURL("http://blog.chromium.org/"))));

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

  // Make a union of a set with 3 more extensions (only 2 are new).
  scoped_refptr<Extension> ext5(CreateTestExtension("d", "", ""));
  scoped_refptr<Extension> ext6(CreateTestExtension("e", "", ""));
  ASSERT_TRUE(ext5 && ext6);

  scoped_ptr<ExtensionSet> to_add(new ExtensionSet());
  to_add->Insert(ext3);  // Already in |extensions|, should not affect size.
  to_add->Insert(ext5);
  to_add->Insert(ext6);

  ASSERT_TRUE(extensions.Contains(ext3->id()));
  ASSERT_TRUE(extensions.InsertAll(*to_add));
  EXPECT_EQ(4u, extensions.size());

  ASSERT_FALSE(extensions.InsertAll(*to_add));  // Re-adding same set no-ops.
  EXPECT_EQ(4u, extensions.size());
}
