// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_security_policy/csp_context.h"
#include "content/common/content_security_policy_header.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
class CSPContextTest : public CSPContext {
 public:
  const std::string& LastConsoleMessage() { return console_message_; }

 private:
  void LogToConsole(const std::string& message) override {
    console_message_ = message;
  }
  std::string console_message_;
};

}  // namespace

TEST(ContentSecurityPolicy, NoDirective) {
  CSPContextTest context;
  std::vector<std::string> report_end_points;  // empty
  ContentSecurityPolicy policy(blink::WebContentSecurityPolicyTypeEnforce,
                               blink::WebContentSecurityPolicySourceHTTP,
                               std::vector<CSPDirective>(), report_end_points,
                               "" /* header */);

  EXPECT_TRUE(ContentSecurityPolicy::Allow(policy, CSPDirective::FormAction,
                                           GURL("http://www.example.com"),
                                           &context));
  EXPECT_EQ("", context.LastConsoleMessage());
}

TEST(ContentSecurityPolicy, ReportViolation) {
  CSPContextTest context;

  // source = "www.example.com"
  CSPSource source("", "www.example.com", false, url::PORT_UNSPECIFIED, false,
                   "");
  CSPSourceList source_list(false, false, {source});
  CSPDirective directive(CSPDirective::FormAction, source_list);
  std::vector<std::string> report_end_points;  // empty
  ContentSecurityPolicy policy(blink::WebContentSecurityPolicyTypeEnforce,
                               blink::WebContentSecurityPolicySourceHTTP,
                               {directive}, report_end_points, "" /* header */);

  EXPECT_FALSE(ContentSecurityPolicy::Allow(policy, CSPDirective::FormAction,
                                            GURL("http://www.not-example.com"),
                                            &context));

  const char console_message[] =
      "Refused to send form data to 'http://www.not-example.com/' because it "
      "violates the following Content Security Policy directive: \"form-action "
      "www.example.com\".\n";
  EXPECT_EQ(console_message, context.LastConsoleMessage());
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
        blink::WebContentSecurityPolicyTypeEnforce,
        blink::WebContentSecurityPolicySourceHTTP,
        {CSPDirective(CSPDirective::DefaultSrc, source_list_a)},
        report_end_points, "" /* header */);
    EXPECT_FALSE(ContentSecurityPolicy::Allow(policy, CSPDirective::FrameSrc,
                                              GURL("http://b.com"), &context));
    const char console_message[] =
        "Refused to frame 'http://b.com/' because it violates "
        "the following Content Security Policy directive: \"default-src "
        "http://a.com\". Note that 'frame-src' was not explicitly "
        "set, so 'default-src' is used as a fallback.\n";
    EXPECT_EQ(console_message, context.LastConsoleMessage());
    EXPECT_TRUE(ContentSecurityPolicy::Allow(policy, CSPDirective::FrameSrc,
                                             GURL("http://a.com"), &context));
  }
  {
    CSPContextTest context;
    ContentSecurityPolicy policy(
        blink::WebContentSecurityPolicyTypeEnforce,
        blink::WebContentSecurityPolicySourceHTTP,
        {CSPDirective(CSPDirective::ChildSrc, source_list_a)},
        report_end_points, "" /* header */);
    EXPECT_FALSE(ContentSecurityPolicy::Allow(policy, CSPDirective::FrameSrc,
                                              GURL("http://b.com"), &context));
    const char console_message[] =
        "Refused to frame 'http://b.com/' because it violates "
        "the following Content Security Policy directive: \"child-src "
        "http://a.com\". Note that 'frame-src' was not explicitly "
        "set, so 'child-src' is used as a fallback.\n";
    EXPECT_EQ(console_message, context.LastConsoleMessage());
    EXPECT_TRUE(ContentSecurityPolicy::Allow(policy, CSPDirective::FrameSrc,
                                             GURL("http://a.com"), &context));
  }
  {
    CSPContextTest context;
    CSPSourceList source_list(false, false, {source_a, source_b});
    ContentSecurityPolicy policy(
        blink::WebContentSecurityPolicyTypeEnforce,
        blink::WebContentSecurityPolicySourceHTTP,
        {CSPDirective(CSPDirective::FrameSrc, {source_list_a}),
         CSPDirective(CSPDirective::ChildSrc, {source_list_b})},
        report_end_points, "" /* header */);
    EXPECT_TRUE(ContentSecurityPolicy::Allow(policy, CSPDirective::FrameSrc,
                                             GURL("http://a.com"), &context));
    EXPECT_FALSE(ContentSecurityPolicy::Allow(policy, CSPDirective::FrameSrc,
                                              GURL("http://b.com"), &context));
    const char console_message[] =
        "Refused to frame 'http://b.com/' because it violates "
        "the following Content Security Policy directive: \"frame-src "
        "http://a.com\".\n";
    EXPECT_EQ(console_message, context.LastConsoleMessage());
  }
}

}  // namespace content
