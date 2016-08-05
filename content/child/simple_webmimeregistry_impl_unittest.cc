// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for SimpleWebMimeRegistryImpl.

#include "content/child/simple_webmimeregistry_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"

TEST(SimpleWebMimeRegistryImpl, mimeTypeTest)
{
  content::SimpleWebMimeRegistryImpl registry;

  EXPECT_TRUE(registry.supportsImagePrefixedMIMEType("image/gif"));
  EXPECT_TRUE(registry.supportsImagePrefixedMIMEType("image/svg+xml"));
  EXPECT_FALSE(registry.supportsImageMIMEType("image/svg+xml"));
  EXPECT_TRUE(registry.supportsImageMIMEType("image/gif"));
}

TEST(SimpleWebMimeRegistryImpl, PluginMimeTypes) {
  content::SimpleWebMimeRegistryImpl registry;

  // Since we've removed MIME type guessing based on plugin-declared file
  // extensions, ensure that the SimpleWebMimeRegistry already contains
  // the extensions used by common PPAPI plugins.
  EXPECT_EQ("application/pdf",
            registry.wellKnownMimeTypeForExtension("pdf").utf8());
  EXPECT_EQ("application/x-shockwave-flash",
            registry.wellKnownMimeTypeForExtension("swf").utf8());
}
