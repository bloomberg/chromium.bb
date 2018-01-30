// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_security_policy/csp_context.h"
#include "content/common/content_security_policy_header.h"
#include "content/common/navigation_params.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
class CSPContextTest : public CSPContext {
 public:
  CSPContextTest() : CSPContext() {}

  const std::vector<CSPViolationParams>& violations() { return violations_; }

  void AddSchemeToBypassCSP(const std::string& scheme) {
    scheme_to_bypass_.push_back(scheme);
  }

  bool SchemeShouldBypassCSP(const base::StringPiece& scheme) override {
    return std::find(scheme_to_bypass_.begin(), scheme_to_bypass_.end(),
                     scheme) != scheme_to_bypass_.end();
  }

 private:
  void ReportContentSecurityPolicyViolation(
      const CSPViolationParams& violation_params) override {
    violations_.push_back(violation_params);
  }
  std::vector<CSPViolationParams> violations_;
  std::vector<std::string> scheme_to_bypass_;

  DISALLOW_COPY_AND_ASSIGN(CSPContextTest);
};

ContentSecurityPolicyHeader EmptyCspHeader() {
  return ContentSecurityPolicyHeader(
      std::string(), blink::kWebContentSecurityPolicyTypeEnforce,
      blink::kWebContentSecurityPolicySourceHTTP);
}

}  // namespace

TEST(ContentSecurityPolicy, NoDirective) {
  CSPContextTest context;
  std::vector<std::string> report_end_points;  // empty
  ContentSecurityPolicy policy(EmptyCspHeader(), std::vector<CSPDirective>(),
                               report_end_points, false);

  EXPECT_TRUE(ContentSecurityPolicy::Allow(policy, CSPDirective::FormAction,
                                           GURL("http://www.example.com"),
                                           false, &context, SourceLocation()));
  ASSERT_EQ(0u, context.violations().size());
}

TEST(ContentSecurityPolicy, ReportViolation) {
  CSPContextTest context;

  // source = "www.example.com"
  CSPSource source("", "www.example.com", false, url::PORT_UNSPECIFIED, false,
                   "");
  CSPSourceList source_list(false, false, {source});
  CSPDirective directive(CSPDirective::FormAction, source_list);
  std::vector<std::string> report_end_points;  // empty
  ContentSecurityPolicy policy(EmptyCspHeader(), {directive}, report_end_points,
                               false);

  EXPECT_FALSE(ContentSecurityPolicy::Allow(policy, CSPDirective::FormAction,
                                            GURL("http://www.not-example.com"),
                                            false, &context, SourceLocation()));

  ASSERT_EQ(1u, context.violations().size());
  const char console_message[] =
      "Refused to send form data to 'http://www.not-example.com/' because it "
      "violates the following Content Security Policy directive: \"form-action "
      "www.example.com\".\n";
  EXPECT_EQ(console_message, context.violations()[0].console_message);
}

TEST(ContentSecurityPolicy, DirectiveFallback) {
  CSPSource source_a("http", "a.com", false, url::PORT_UNSPECIFIED, false, "");
  CSPSource source_b("http", "b.com", false, url::PORT_UNSPECIFIED, false, "");
  CSPSourceList source_list_a(false, false, {source_a});
  CSPSourceList source_list_b(false, false, {source_b});

  std::vector<std::string> report_end_points;  // Empty.

  {
    CSPContextTest context;
    ContentSecurityPolicy policy(
        EmptyCspHeader(),
        {CSPDirective(CSPDirective::DefaultSrc, source_list_a)},
        report_end_points, false);
    EXPECT_FALSE(ContentSecurityPolicy::Allow(policy, CSPDirective::FrameSrc,
                                              GURL("http://b.com"), false,
                                              &context, SourceLocation()));
    ASSERT_EQ(1u, context.violations().size());
    const char console_message[] =
        "Refused to frame 'http://b.com/' because it violates "
        "the following Content Security Policy directive: \"default-src "
        "http://a.com\". Note that 'frame-src' was not explicitly "
        "set, so 'default-src' is used as a fallback.\n";
    EXPECT_EQ(console_message, context.violations()[0].console_message);
    EXPECT_TRUE(ContentSecurityPolicy::Allow(policy, CSPDirective::FrameSrc,
                                             GURL("http://a.com"), false,
                                             &context, SourceLocation()));
  }
  {
    CSPContextTest context;
    ContentSecurityPolicy policy(
        EmptyCspHeader(), {CSPDirective(CSPDirective::ChildSrc, source_list_a)},
        report_end_points, false);
    EXPECT_FALSE(ContentSecurityPolicy::Allow(policy, CSPDirective::FrameSrc,
                                              GURL("http://b.com"), false,
                                              &context, SourceLocation()));
    ASSERT_EQ(1u, context.violations().size());
    const char console_message[] =
        "Refused to frame 'http://b.com/' because it violates "
        "the following Content Security Policy directive: \"child-src "
        "http://a.com\". Note that 'frame-src' was not explicitly "
        "set, so 'child-src' is used as a fallback.\n";
    EXPECT_EQ(console_message, context.violations()[0].console_message);
    EXPECT_TRUE(ContentSecurityPolicy::Allow(policy, CSPDirective::FrameSrc,
                                             GURL("http://a.com"), false,
                                             &context, SourceLocation()));
  }
  {
    CSPContextTest context;
    CSPSourceList source_list(false, false, {source_a, source_b});
    ContentSecurityPolicy policy(
        EmptyCspHeader(),
        {CSPDirective(CSPDirective::FrameSrc, {source_list_a}),
         CSPDirective(CSPDirective::ChildSrc, {source_list_b})},
        report_end_points, false);
    EXPECT_TRUE(ContentSecurityPolicy::Allow(policy, CSPDirective::FrameSrc,
                                             GURL("http://a.com"), false,
                                             &context, SourceLocation()));
    EXPECT_FALSE(ContentSecurityPolicy::Allow(policy, CSPDirective::FrameSrc,
                                              GURL("http://b.com"), false,
                                              &context, SourceLocation()));
    ASSERT_EQ(1u, context.violations().size());
    const char console_message[] =
        "Refused to frame 'http://b.com/' because it violates "
        "the following Content Security Policy directive: \"frame-src "
        "http://a.com\".\n";
    EXPECT_EQ(console_message, context.violations()[0].console_message);
  }
}

TEST(ContentSecurityPolicy, RequestsAllowedWhenBypassingCSP) {
  CSPContextTest context;
  std::vector<std::string> report_end_points;  // empty
  CSPSource source("https", "example.com", false, url::PORT_UNSPECIFIED, false,
                   "");
  CSPSourceList source_list(false, false, {source});
  ContentSecurityPolicy policy(
      EmptyCspHeader(), {CSPDirective(CSPDirective::DefaultSrc, source_list)},
      report_end_points, false);

  EXPECT_TRUE(ContentSecurityPolicy::Allow(policy, CSPDirective::FrameSrc,
                                           GURL("https://example.com/"), false,
                                           &context, SourceLocation()));
  EXPECT_FALSE(ContentSecurityPolicy::Allow(policy, CSPDirective::FrameSrc,
                                            GURL("https://not-example.com/"),
                                            false, &context, SourceLocation()));

  // Register 'https' as bypassing CSP, which should now bypass is entirely.
  context.AddSchemeToBypassCSP("https");

  EXPECT_TRUE(ContentSecurityPolicy::Allow(policy, CSPDirective::FrameSrc,
                                           GURL("https://example.com/"), false,
                                           &context, SourceLocation()));
  EXPECT_TRUE(ContentSecurityPolicy::Allow(policy, CSPDirective::FrameSrc,
                                           GURL("https://not-example.com/"),
                                           false, &context, SourceLocation()));
}

TEST(ContentSecurityPolicy, FilesystemAllowedWhenBypassingCSP) {
  CSPContextTest context;
  std::vector<std::string> report_end_points;  // empty
  CSPSource source("https", "example.com", false, url::PORT_UNSPECIFIED, false,
                   "");
  CSPSourceList source_list(false, false, {source});
  ContentSecurityPolicy policy(
      EmptyCspHeader(), {CSPDirective(CSPDirective::DefaultSrc, source_list)},
      report_end_points, false);

  EXPECT_FALSE(ContentSecurityPolicy::Allow(
      policy, CSPDirective::FrameSrc,
      GURL("filesystem:https://example.com/file.txt"), false, &context,
      SourceLocation()));
  EXPECT_FALSE(ContentSecurityPolicy::Allow(
      policy, CSPDirective::FrameSrc,
      GURL("filesystem:https://not-example.com/file.txt"), false, &context,
      SourceLocation()));

  // Register 'https' as bypassing CSP, which should now bypass is entirely.
  context.AddSchemeToBypassCSP("https");

  EXPECT_TRUE(ContentSecurityPolicy::Allow(
      policy, CSPDirective::FrameSrc,
      GURL("filesystem:https://example.com/file.txt"), false, &context,
      SourceLocation()));
  EXPECT_TRUE(ContentSecurityPolicy::Allow(
      policy, CSPDirective::FrameSrc,
      GURL("filesystem:https://not-example.com/file.txt"), false, &context,
      SourceLocation()));
}

TEST(ContentSecurityPolicy, BlobAllowedWhenBypassingCSP) {
  CSPContextTest context;
  std::vector<std::string> report_end_points;  // empty
  CSPSource source("https", "example.com", false, url::PORT_UNSPECIFIED, false,
                   "");
  CSPSourceList source_list(false, false, {source});
  ContentSecurityPolicy policy(
      EmptyCspHeader(), {CSPDirective(CSPDirective::DefaultSrc, source_list)},
      report_end_points, false);

  EXPECT_FALSE(ContentSecurityPolicy::Allow(policy, CSPDirective::FrameSrc,
                                            GURL("blob:https://example.com/"),
                                            false, &context, SourceLocation()));
  EXPECT_FALSE(ContentSecurityPolicy::Allow(
      policy, CSPDirective::FrameSrc, GURL("blob:https://not-example.com/"),
      false, &context, SourceLocation()));

  // Register 'https' as bypassing CSP, which should now bypass is entirely.
  context.AddSchemeToBypassCSP("https");

  EXPECT_TRUE(ContentSecurityPolicy::Allow(policy, CSPDirective::FrameSrc,
                                           GURL("blob:https://example.com/"),
                                           false, &context, SourceLocation()));
  EXPECT_TRUE(ContentSecurityPolicy::Allow(
      policy, CSPDirective::FrameSrc, GURL("blob:https://not-example.com/"),
      false, &context, SourceLocation()));
}

TEST(ContentSecurityPolicy, ShouldUpgradeInsecureRequest) {
  std::vector<std::string> report_end_points;  // empty
  CSPSource source("https", "example.com", false, url::PORT_UNSPECIFIED, false,
                   "");
  CSPSourceList source_list(false, false, {source});
  ContentSecurityPolicy policy(
      EmptyCspHeader(), {CSPDirective(CSPDirective::DefaultSrc, source_list)},
      report_end_points, false);

  EXPECT_FALSE(ContentSecurityPolicy::ShouldUpgradeInsecureRequest(policy));

  policy.directives.push_back(
      CSPDirective(CSPDirective::UpgradeInsecureRequests, CSPSourceList()));
  EXPECT_TRUE(ContentSecurityPolicy::ShouldUpgradeInsecureRequest(policy));
}

}  // namespace content
