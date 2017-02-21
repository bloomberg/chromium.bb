// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_security_policy/csp_source_list.h"
#include "content/common/content_security_policy/csp_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class CSPContextTest : public CSPContext {
 public:
  void AddSchemeToBypassCSP(const std::string& scheme) {
    scheme_to_bypass_.push_back(scheme);
  }

  bool SchemeShouldBypassCSP(const base::StringPiece& scheme) override {
    return std::find(scheme_to_bypass_.begin(), scheme_to_bypass_.end(),
                     scheme) != scheme_to_bypass_.end();
  }

 private:
  std::vector<std::string> scheme_to_bypass_;
};

// Allow() is an abbreviation of CSPSourceList::Allow(). Useful for writting
// test expectations on one line.
bool Allow(const CSPSourceList& source_list,
           const GURL& url,
           CSPContext* context,
           bool is_redirect = false) {
  return CSPSourceList::Allow(source_list, url, context, is_redirect);
}

}  // namespace

TEST(CSPSourceListTest, MultipleSource) {
  CSPContextTest context;
  context.SetSelf(url::Origin(GURL("http://example.com")));
  CSPSourceList source_list(
      false,  // allow_self
      false,  // allow_star:
      {CSPSource("", "a.com", false, url::PORT_UNSPECIFIED, false, ""),
       CSPSource("", "b.com", false, url::PORT_UNSPECIFIED, false, "")});
  EXPECT_TRUE(Allow(source_list, GURL("http://a.com"), &context));
  EXPECT_TRUE(Allow(source_list, GURL("http://b.com"), &context));
  EXPECT_FALSE(Allow(source_list, GURL("http://c.com"), &context));
}

TEST(CSPSourceList, AllowStar) {
  CSPContextTest context;
  context.SetSelf(url::Origin(GURL("http://example.com")));
  CSPSourceList source_list(false,                      // allow_self
                            true,                       // allow_star:
                            std::vector<CSPSource>());  // source_list
  EXPECT_TRUE(Allow(source_list, GURL("http://not-example.com"), &context));
  EXPECT_TRUE(Allow(source_list, GURL("https://not-example.com"), &context));
  EXPECT_TRUE(Allow(source_list, GURL("http-so://not-example.com"), &context));
  EXPECT_TRUE(Allow(source_list, GURL("https-so://not-example.com"), &context));
  EXPECT_TRUE(Allow(source_list, GURL("ws://not-example.com"), &context));
  EXPECT_TRUE(Allow(source_list, GURL("wss://not-example.com"), &context));
  EXPECT_TRUE(Allow(source_list, GURL("ftp://not-example.com"), &context));

  EXPECT_FALSE(Allow(source_list, GURL("file://not-example.com"), &context));
  EXPECT_FALSE(Allow(source_list, GURL("applewebdata://a.test"), &context));

  // With a protocol of 'file', '*' allow 'file:'
  context.SetSelf(url::Origin(GURL("file://example.com")));
  EXPECT_TRUE(Allow(source_list, GURL("file://not-example.com"), &context));
  EXPECT_FALSE(Allow(source_list, GURL("applewebdata://a.test"), &context));
}

TEST(CSPSourceList, AllowSelf) {
  CSPContextTest context;
  context.SetSelf(url::Origin(GURL("http://example.com")));
  CSPSourceList source_list(true,                       // allow_self
                            false,                      // allow_star:
                            std::vector<CSPSource>());  // source_list
  EXPECT_TRUE(Allow(source_list, GURL("http://example.com"), &context));
  EXPECT_FALSE(Allow(source_list, GURL("http://not-example.com"), &context));
  EXPECT_TRUE(Allow(source_list, GURL("https://example.com"), &context));
  EXPECT_FALSE(Allow(source_list, GURL("ws://example.com"), &context));
}

TEST(CSPSourceList, AllowSelfWithFilesystem) {
  CSPContextTest context;
  context.SetSelf(url::Origin(GURL("https://a.test")));
  CSPSourceList source_list(true,                       // allow_self
                            false,                      // allow_star:
                            std::vector<CSPSource>());  // source_list

  GURL filesystem_url("filesystem:https://a.test/file.txt");

  EXPECT_TRUE(Allow(source_list, GURL("https://a.test/"), &context));
  EXPECT_FALSE(Allow(source_list, filesystem_url, &context));

  // Register 'https' as bypassing CSP, which should trigger the inner URL
  // behavior.
  context.AddSchemeToBypassCSP("https");

  EXPECT_TRUE(Allow(source_list, GURL("https://a.test/"), &context));
  EXPECT_TRUE(Allow(source_list, filesystem_url, &context));
}

TEST(CSPSourceList, BlobDisallowedWhenBypassingSelfScheme) {
  CSPContextTest context;
  context.SetSelf(url::Origin(GURL("https://a.test")));
  CSPSource blob(
      CSPSource("blob", "", false, url::PORT_UNSPECIFIED, false, ""));
  CSPSourceList source_list(true,     // allow_self
                            false,    // allow_star:
                            {blob});  // source_list

  GURL blob_url_self("blob:https://a.test/1be95204-93d6-4GUID");
  GURL blob_url_not_self("blob:https://b.test/1be95204-93d6-4GUID");

  EXPECT_TRUE(Allow(source_list, blob_url_self, &context));
  EXPECT_TRUE(Allow(source_list, blob_url_not_self, &context));

  // Register 'https' as bypassing CSP, which should trigger the inner URL
  // behavior.
  context.AddSchemeToBypassCSP("https");

  EXPECT_TRUE(Allow(source_list, blob_url_self, &context));
  // TODO(arthursonzogni, mkwst): This should be true
  // see http://crbug.com/692046
  EXPECT_FALSE(Allow(source_list, blob_url_not_self, &context));
}

TEST(CSPSourceList, FilesystemDisallowedWhenBypassingSelfScheme) {
  CSPContextTest context;
  context.SetSelf(url::Origin(GURL("https://a.test")));
  CSPSource filesystem(
      CSPSource("filesystem", "", false, url::PORT_UNSPECIFIED, false, ""));
  CSPSourceList source_list(true,           // allow_self
                            false,          // allow_star:
                            {filesystem});  // source_list

  GURL filesystem_url_self("filesystem:https://a.test/file.txt");
  GURL filesystem_url_not_self("filesystem:https://b.test/file.txt");

  EXPECT_TRUE(Allow(source_list, filesystem_url_self, &context));
  EXPECT_TRUE(Allow(source_list, filesystem_url_not_self, &context));

  // Register 'https' as bypassing CSP, which should trigger the inner URL
  // behavior.
  context.AddSchemeToBypassCSP("https");

  EXPECT_TRUE(Allow(source_list, filesystem_url_self, &context));
  // TODO(arthursonzogni, mkwst): This should be true
  // see http://crbug.com/692046
  EXPECT_FALSE(Allow(source_list, filesystem_url_not_self, &context));
}

TEST(CSPSourceList, AllowSelfWithUnspecifiedPort) {
  CSPContext context;
  context.SetSelf(url::Origin(GURL("chrome://print")));
  CSPSourceList source_list(true,                       // allow_self
                            false,                      // allow_star:
                            std::vector<CSPSource>());  // source_list

  EXPECT_TRUE(Allow(
      source_list,
      GURL("chrome://print/pdf_preview.html?chrome://print/1/0/print.pdf"),
      &context));
}

TEST(CSPSourceList, AllowNone) {
  CSPContextTest context;
  context.SetSelf(url::Origin(GURL("http://example.com")));
  CSPSourceList source_list(false,                      // allow_self
                            false,                      // allow_star:
                            std::vector<CSPSource>());  // source_list
  EXPECT_FALSE(Allow(source_list, GURL("http://example.com"), &context));
  EXPECT_FALSE(Allow(source_list, GURL("https://example.test/"), &context));
}

}  // namespace content
