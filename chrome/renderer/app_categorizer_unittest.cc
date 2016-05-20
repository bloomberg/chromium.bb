// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/app_categorizer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char* kChatAppURLs[] = {
  "https://hangouts.google.com/hangouts/foo",
  "https://hAnGoUtS.gOoGlE.com/HaNgOuTs/foo",
  "https://meet.google.com/hangouts/foo",
  "https://talkgadget.google.com/hangouts/foo",
  "https://staging.talkgadget.google.com/hangouts/foo",
  "https://plus.google.com/hangouts/foo",
  "https://plus.sandbox.google.com/hangouts/foo"
};

const char* kChatManifestFSs[] = {
  "filesystem:https://hangouts.google.com/foo",
  "filesystem:https://hAnGoUtS.gOoGlE.com/foo",
  "filesystem:https://meet.google.com/foo",
  "filesystem:https://talkgadget.google.com/foo",
  "filesystem:https://staging.talkgadget.google.com/foo",
  "filesystem:https://plus.google.com/foo",
  "filesystem:https://plus.sandbox.google.com/foo"
};

const char* kBadChatAppURLs[] = {
  "http://talkgadget.google.com/hangouts/foo",  // not https
  "https://talkgadget.evil.com/hangouts/foo"    // domain not whitelisted
};

const char* kPhotosAppURLs[] = {
  "https://foo.plus.google.com",
  "https://foo.plus.sandbox.google.com"
};

const char* kPhotosManifestURLs[] = {
  "https://ssl.gstatic.com/photos/nacl/foo",
  "https://ssl.gstatic.com/s2/oz/nacl/foo"
};

const char* kBadPhotosAppURLs[] = {
  "https://plus.google.com/foo",
  "https://plus.google.com/foo",
  "https://plus.google.com/foo",
  "http://plus.google.com/foo", // http scheme
  "https://plus.evil.com/foo",  // domain not whitelisted
};

const char* kBadPhotosManifestURLs[] = {
  "http://ssl.gstatic.com/photos/nacl/foo",         // http scheme
  "https://lss.gstatic.com/photos/nacl/foo",        // bad hostname
  "https://ssl.gstatic.com/wrong/photos/nacl/foo",  // bad path
  "https://ssl.gstatic.com/photos/nacl/foo",
  "https://ssl.gstatic.com/photos/nacl/foo",
};

}  // namespace

TEST(AppCategorizerTest, IsHangoutsUrl) {
  for (size_t i = 0; i < arraysize(kChatAppURLs); ++i) {
    EXPECT_TRUE(AppCategorizer::IsHangoutsUrl(GURL(kChatAppURLs[i])));
  }

  for (size_t i = 0; i < arraysize(kBadChatAppURLs); ++i) {
    EXPECT_FALSE(AppCategorizer::IsHangoutsUrl(GURL(kBadChatAppURLs[i])));
  }
}

TEST(AppCategorizerTest, IsWhitelistedApp) {
  // Hangouts app
  {
    EXPECT_EQ(arraysize(kChatAppURLs), arraysize(kChatManifestFSs));
    for (size_t i = 0; i < arraysize(kChatAppURLs); ++i) {
      EXPECT_TRUE(AppCategorizer::IsWhitelistedApp(
          GURL(kChatManifestFSs[i]), GURL(kChatAppURLs[i])));
    }
    for (size_t i = 0; i < arraysize(kBadChatAppURLs); ++i) {
      EXPECT_FALSE(AppCategorizer::IsWhitelistedApp(
          GURL("filesystem:https://irrelevant.com/"),
          GURL(kBadChatAppURLs[i])));
    }

    // Manifest URL not filesystem
    EXPECT_FALSE(AppCategorizer::IsWhitelistedApp(
        GURL("https://hangouts.google.com/foo"),
        GURL("https://hangouts.google.com/hangouts/foo")));

    // Manifest URL not https
    EXPECT_FALSE(AppCategorizer::IsWhitelistedApp(
        GURL("filesystem:http://hangouts.google.com/foo"),
        GURL("https://hangouts.google.com/hangouts/foo")));

    // Manifest URL hostname does not match that of the app URL
    EXPECT_FALSE(AppCategorizer::IsWhitelistedApp(
        GURL("filesystem:https://meet.google.com/foo"),
        GURL("https://hangouts.google.com/hangouts/foo")));
  }

  // Photos app
  {
    EXPECT_EQ(arraysize(kPhotosAppURLs), arraysize(kPhotosManifestURLs));
    for (size_t i = 0; i < arraysize(kPhotosAppURLs); ++i) {
      EXPECT_TRUE(AppCategorizer::IsWhitelistedApp(
          GURL(kPhotosManifestURLs[i]), GURL(kPhotosAppURLs[i])));
    }
    // The app/manifest two sides do not have any coorelation for the Photos app
    for (size_t i = 0; i < arraysize(kPhotosAppURLs); ++i) {
      EXPECT_TRUE(AppCategorizer::IsWhitelistedApp(
          GURL(kPhotosManifestURLs[(i + 1) % arraysize(kPhotosAppURLs)]),
          GURL(kPhotosAppURLs[i])));
    }

    EXPECT_EQ(arraysize(kBadPhotosAppURLs), arraysize(kBadPhotosManifestURLs));
    for (size_t i = 0; i < arraysize(kBadPhotosAppURLs); ++i) {
      EXPECT_FALSE(AppCategorizer::IsWhitelistedApp(
          GURL(kBadPhotosManifestURLs[i]), GURL(kBadPhotosAppURLs[i])));
    }
  }
}
