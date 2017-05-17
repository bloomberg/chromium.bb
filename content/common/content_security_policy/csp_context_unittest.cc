// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "content/common/content_security_policy/csp_context.h"
#include "content/common/content_security_policy_header.h"
#include "content/common/navigation_params.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class CSPContextTest : public CSPContext {
 public:
  const CSPViolationParams& last_violation() { return last_violation_; }

  void AddSchemeToBypassCSP(const std::string& scheme) {
    scheme_to_bypass_.insert(scheme);
  }

  bool SchemeShouldBypassCSP(const base::StringPiece& scheme) override {
    return scheme_to_bypass_.count(scheme.as_string());
  }

  void set_sanitize_data_for_use_in_csp_violation(bool value) {
    sanitize_data_for_use_in_csp_violation_ = value;
  }

  void SanitizeDataForUseInCspViolation(
      bool is_redirect,
      CSPDirective::Name directive,
      GURL* blocked_url,
      SourceLocation* source_location) const override {
    if (!sanitize_data_for_use_in_csp_violation_)
      return;
    *blocked_url = blocked_url->GetOrigin();
    *source_location =
        SourceLocation(GURL(source_location->url).GetOrigin().spec(), 0u, 0u);
  }

 private:
  void ReportContentSecurityPolicyViolation(
      const CSPViolationParams& violation_params) override {
    last_violation_ = violation_params;
  }
  CSPViolationParams last_violation_;
  std::set<std::string> scheme_to_bypass_;
  bool sanitize_data_for_use_in_csp_violation_ = false;
};

// Build a new policy made of only one directive and no report endpoints.
ContentSecurityPolicy BuildPolicy(CSPDirective::Name directive_name,
                                  std::vector<CSPSource> sources) {
  return ContentSecurityPolicy(
      ContentSecurityPolicyHeader(std::string(),  // header
                                  blink::kWebContentSecurityPolicyTypeEnforce,
                                  blink::kWebContentSecurityPolicySourceHTTP),
      {CSPDirective(directive_name, CSPSourceList(false, false, sources))},
      std::vector<std::string>());  // report_end_points
}

}  // namespace

TEST(CSPContextTest, SchemeShouldBypassCSP) {
  CSPSource source("", "example.com", false, url::PORT_UNSPECIFIED, false, "");
  CSPContextTest context;
  context.AddContentSecurityPolicy(
      BuildPolicy(CSPDirective::DefaultSrc, {source}));

  EXPECT_FALSE(context.IsAllowedByCsp(CSPDirective::FrameSrc,
                                      GURL("data:text/html,<html></html>"),
                                      false, SourceLocation()));

  context.AddSchemeToBypassCSP("data");

  EXPECT_TRUE(context.IsAllowedByCsp(CSPDirective::FrameSrc,
                                     GURL("data:text/html,<html></html>"),
                                     false, SourceLocation()));
}

TEST(CSPContextTest, MultiplePolicies) {
  CSPContextTest context;
  context.SetSelf(url::Origin(GURL("http://example.com")));

  CSPSource source_a("", "a.com", false, url::PORT_UNSPECIFIED, false, "");
  CSPSource source_b("", "b.com", false, url::PORT_UNSPECIFIED, false, "");
  CSPSource source_c("", "c.com", false, url::PORT_UNSPECIFIED, false, "");

  context.AddContentSecurityPolicy(
      BuildPolicy(CSPDirective::FrameSrc, {source_a, source_b}));
  context.AddContentSecurityPolicy(
      BuildPolicy(CSPDirective::FrameSrc, {source_a, source_c}));

  EXPECT_TRUE(context.IsAllowedByCsp(
      CSPDirective::FrameSrc, GURL("http://a.com"), false, SourceLocation()));
  EXPECT_FALSE(context.IsAllowedByCsp(
      CSPDirective::FrameSrc, GURL("http://b.com"), false, SourceLocation()));
  EXPECT_FALSE(context.IsAllowedByCsp(
      CSPDirective::FrameSrc, GURL("http://c.com"), false, SourceLocation()));
  EXPECT_FALSE(context.IsAllowedByCsp(
      CSPDirective::FrameSrc, GURL("http://d.com"), false, SourceLocation()));
}

TEST(CSPContextTest, SanitizeDataForUseInCspViolation) {
  CSPContextTest context;
  context.SetSelf(url::Origin(GURL("http://a.com")));

  // Content-Security-Policy: frame-src "a.com/iframe"
  context.AddContentSecurityPolicy(
      BuildPolicy(CSPDirective::FrameSrc,
                  {CSPSource("", "a.com", false, url::PORT_UNSPECIFIED, false,
                             "/iframe")}));

  GURL blocked_url("http://a.com/login?password=1234");
  SourceLocation source_location("http://a.com/login", 10u, 20u);

  // When the |blocked_url| and |source_location| aren't sensitive information.
  {
    EXPECT_FALSE(context.IsAllowedByCsp(CSPDirective::FrameSrc, blocked_url,
                                        false, source_location));
    EXPECT_EQ(context.last_violation().blocked_url, blocked_url);
    EXPECT_EQ(context.last_violation().source_location.url,
              "http://a.com/login");
    EXPECT_EQ(context.last_violation().source_location.line_number, 10u);
    EXPECT_EQ(context.last_violation().source_location.column_number, 20u);
    EXPECT_EQ(context.last_violation().console_message,
              "Refused to frame 'http://a.com/login?password=1234' because it "
              "violates the following Content Security Policy directive: "
              "\"frame-src a.com/iframe\".\n");
  }

  context.set_sanitize_data_for_use_in_csp_violation(true);

  // When the |blocked_url| and |source_location| are sensitive information.
  {
    EXPECT_FALSE(context.IsAllowedByCsp(CSPDirective::FrameSrc, blocked_url,
                                        false, source_location));
    EXPECT_EQ(context.last_violation().blocked_url, blocked_url.GetOrigin());
    EXPECT_EQ(context.last_violation().source_location.url, "http://a.com/");
    EXPECT_EQ(context.last_violation().source_location.line_number, 0u);
    EXPECT_EQ(context.last_violation().source_location.column_number, 0u);
    EXPECT_EQ(context.last_violation().console_message,
              "Refused to frame 'http://a.com/' because it violates the "
              "following Content Security Policy directive: \"frame-src "
              "a.com/iframe\".\n");
  }
}

}  // namespace content
