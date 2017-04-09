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
  const std::string& LastConsoleMessage() { return console_message_; }

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
    console_message_ = violation_params.console_message;
  }
  std::string console_message_;
  std::vector<std::string> scheme_to_bypass_;
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

}  // namespace content
