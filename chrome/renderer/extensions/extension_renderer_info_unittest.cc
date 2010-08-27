// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_extent.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/renderer/extensions/extension_renderer_info.h"
#include "testing/gtest/include/gtest/gtest.h"

static void AddPattern(ExtensionExtent* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS;
  extent->AddPattern(URLPattern(schemes, pattern));
}

TEST(ExtensionRendererInfoTest, ExtensionRendererInfo) {
  std::string one("1"), two("2"), three("3");

  ViewMsg_ExtensionsUpdated_Params msg;
  msg.extensions.resize(3);

  msg.extensions[0].id = one;
  ExtensionExtent web1;
  AddPattern(&web1, "https://chrome.google.com/*");
  msg.extensions[0].web_extent = web1;

  msg.extensions[1].id = two;
  ExtensionExtent web2;
  AddPattern(&web2, "http://code.google.com/p/chromium/*");
  msg.extensions[1].web_extent = web2;

  msg.extensions[2].id = three;
  ExtensionExtent web3;
  AddPattern(&web3, "http://dev.chromium.org/*");
  msg.extensions[2].web_extent = web3;

  ExtensionRendererInfo::UpdateExtensions(msg);
  EXPECT_EQ(ExtensionRendererInfo::extensions_->size(), 3u);

  ExtensionRendererInfo* ext1 = &ExtensionRendererInfo::extensions_->at(0);
  ExtensionRendererInfo* ext2 = &ExtensionRendererInfo::extensions_->at(1);
  ExtensionRendererInfo* ext3 = &ExtensionRendererInfo::extensions_->at(2);
  EXPECT_EQ(ext1->id(), one);
  EXPECT_EQ(ext2->id(), two);
  EXPECT_EQ(ext3->id(), three);

  EXPECT_EQ(ext1, ExtensionRendererInfo::GetByID(one));
  EXPECT_EQ(ext1, ExtensionRendererInfo::GetByURL(
      GURL("https://chrome.google.com/extensions/")));
  EXPECT_EQ(ext2, ExtensionRendererInfo::GetByURL(
    GURL("http://code.google.com/p/chromium/issues/")));
  EXPECT_EQ(ext3, ExtensionRendererInfo::GetByURL(
    GURL("http://dev.chromium.org/design-docs/")));
  EXPECT_EQ(NULL, ExtensionRendererInfo::GetByURL(
    GURL("http://blog.chromium.org/")));

  EXPECT_TRUE(ExtensionRendererInfo::InSameExtent(
      GURL("https://chrome.google.com/extensions/"),
      GURL("https://chrome.google.com/")));
  EXPECT_FALSE(ExtensionRendererInfo::InSameExtent(
      GURL("https://chrome.google.com/extensions/"),
      GURL("http://chrome.google.com/")));
  EXPECT_FALSE(ExtensionRendererInfo::InSameExtent(
      GURL("https://chrome.google.com/extensions/"),
      GURL("http://dev.chromium.org/design-docs/")));

  // Both of these should be NULL, which mean true for InSameExtent.
  EXPECT_TRUE(ExtensionRendererInfo::InSameExtent(
      GURL("http://www.google.com/"),
      GURL("http://blog.chromium.org/")));
}
