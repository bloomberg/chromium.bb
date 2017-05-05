// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/indexed_ruleset.h"

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "components/subresource_filter/core/common/first_party_origin.h"
#include "components/subresource_filter/core/common/proto/rules.pb.h"
#include "components/subresource_filter/core/common/url_pattern.h"
#include "components/subresource_filter/core/common/url_rule_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

using namespace testing;

class SubresourceFilterIndexedRulesetTest : public ::testing::Test {
 public:
  SubresourceFilterIndexedRulesetTest() { Reset(); }

 protected:
  bool ShouldAllow(base::StringPiece url,
                   base::StringPiece document_origin = nullptr,
                   proto::ElementType element_type = kOther,
                   bool disable_generic_rules = false) const {
    DCHECK(matcher_);
    return !matcher_->ShouldDisallowResourceLoad(
        GURL(url), FirstPartyOrigin(GetOrigin(document_origin)), element_type,
        disable_generic_rules);
  }

  bool ShouldDeactivate(
      base::StringPiece document_url,
      base::StringPiece parent_document_origin = nullptr,
      proto::ActivationType activation_type = kNoActivation) const {
    DCHECK(matcher_);
    return matcher_->ShouldDisableFilteringForDocument(
        GURL(document_url), GetOrigin(parent_document_origin), activation_type);
  }

  bool AddUrlRule(const proto::UrlRule& rule) {
    return indexer_->AddUrlRule(rule);
  }

  bool AddSimpleRule(base::StringPiece url_pattern) {
    return AddUrlRule(MakeUrlRule(UrlPattern(url_pattern, kSubstring)));
  }

  bool AddSimpleWhitelistRule(base::StringPiece url_pattern) {
    auto rule = MakeUrlRule(UrlPattern(url_pattern, kSubstring));
    rule.set_semantics(proto::RULE_SEMANTICS_WHITELIST);
    return AddUrlRule(rule);
  }

  bool AddSimpleWhitelistRule(base::StringPiece url_pattern,
                              int32_t activation_types) {
    auto rule = MakeUrlRule(url_pattern);
    rule.set_semantics(proto::RULE_SEMANTICS_WHITELIST);
    rule.clear_element_types();
    rule.set_activation_types(activation_types);
    return AddUrlRule(rule);
  }

  void Finish() {
    indexer_->Finish();
    matcher_.reset(
        new IndexedRulesetMatcher(indexer_->data(), indexer_->size()));
  }

  void Reset() {
    matcher_.reset(nullptr);
    indexer_.reset(new RulesetIndexer);
  }

  std::unique_ptr<RulesetIndexer> indexer_;
  std::unique_ptr<IndexedRulesetMatcher> matcher_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterIndexedRulesetTest);
};

TEST_F(SubresourceFilterIndexedRulesetTest, OneRuleWithoutMetaInfo) {
  const struct {
    UrlPattern url_pattern;
    const char* url;
    bool expect_allowed;
  } kTestCases[] = {
      // SUBSTRING
      {{"abcd", kSubstring}, "http://ex.com/abcd", false},
      {{"abcd", kSubstring}, "http://ex.com/dcab", true},
      {{"42", kSubstring}, "http://ex.com/adcd/picture42.png", false},
      {{"&test", kSubstring},
       "http://ex.com/params?para1=false&test=true",
       false},
      {{"-test-42.", kSubstring}, "http://ex.com/unit-test-42.1", false},
      {{"/abcdtest160x600.", kSubstring},
       "http://ex.com/abcdtest160x600.png",
       false},

      // WILDCARDED
      {{"http://ex.com/abcd/picture*.png"},
       "http://ex.com/abcd/picture42.png",
       false},
      {{"ex.com", kSubdomain, kAnchorNone}, "http://ex.com", false},
      {{"ex.com", kSubdomain, kAnchorNone}, "http://test.ex.com", false},
      {{"ex.com", kSubdomain, kAnchorNone}, "https://test.ex.com.com", false},
      {{"ex.com", kSubdomain, kAnchorNone}, "https://test.rest.ex.com", false},
      {{"ex.com", kSubdomain, kAnchorNone}, "https://test_ex.com", true},

      {{"http://ex.com", kBoundary, kAnchorNone}, "http://ex.com/", false},
      {{"http://ex.com", kBoundary, kAnchorNone}, "http://ex.com/42", false},
      {{"http://ex.com", kBoundary, kAnchorNone},
       "http://ex.com/42/http://ex.com/",
       false},
      {{"http://ex.com", kBoundary, kAnchorNone},
       "http://ex.com/42/http://ex.info/",
       false},
      {{"http://ex.com/", kBoundary, kBoundary}, "http://ex.com", false},
      {{"http://ex.com/", kBoundary, kBoundary}, "http://ex.com/42", true},
      {{"http://ex.com/", kBoundary, kBoundary},
       "http://ex.info/42/http://ex.com/",
       true},
      {{"http://ex.com/", kBoundary, kBoundary},
       "http://ex.info/42/http://ex.com/",
       true},
      {{"http://ex.com/", kBoundary, kBoundary}, "http://ex.com/", false},
      {{"http://ex.com/", kBoundary, kBoundary}, "http://ex.com/42.swf", true},
      {{"http://ex.com/", kBoundary, kBoundary},
       "http://ex.info/redirect/http://ex.com/",
       true},
      {{"pdf", kAnchorNone, kBoundary}, "http://ex.com/abcd.pdf", false},
      {{"pdf", kAnchorNone, kBoundary}, "http://ex.com/pdfium", true},
      {{"http://ex.com^"}, "http://ex.com/", false},
      {{"http://ex.com^"}, "http://ex.com:8000/", false},
      {{"http://ex.com^"}, "http://ex.com.ru", true},
      {{"^ex.com^"},
       "http://ex.com:8000/42.loss?a=12&b=%D1%82%D0%B5%D1%81%D1%82",
       false},
      {{"^42.loss^"},
       "http://ex.com:8000/42.loss?a=12&b=%D1%82%D0%B5%D1%81%D1%82",
       false},

      // TODO(pkalinnikov): The '^' at the end should match end-of-string.
      //
      // {"^%D1%82%D0%B5%D1%81%D1%82^",
      //  "http://ex.com:8000/42.loss?a=12&b=%D1%82%D0%B5%D1%81%D1%82",
      //  false},
      // {"/abcd/*/picture^", "http://ex.com/abcd/42/picture", false},

      {{"/abcd/*/picture^"}, "http://ex.com/abcd/42/loss/picture?param", false},
      {{"/abcd/*/picture^"}, "http://ex.com/abcd//picture/42", false},
      {{"/abcd/*/picture^"}, "http://ex.com/abcd/picture", true},
      {{"/abcd/*/picture^"}, "http://ex.com/abcd/42/pictureraph", true},
      {{"/abcd/*/picture^"}, "http://ex.com/abcd/42/picture.swf", true},
      {{"test.ex.com^", kSubdomain, kAnchorNone},
       "http://test.ex.com/42.swf",
       false},
      {{"test.ex.com^", kSubdomain, kAnchorNone},
       "http://server1.test.ex.com/42.swf",
       false},
      {{"test.ex.com^", kSubdomain, kAnchorNone},
       "https://test.ex.com:8000/",
       false},
      {{"test.ex.com^", kSubdomain, kAnchorNone},
       "http://test.ex.com.ua/42.swf",
       true},
      {{"test.ex.com^", kSubdomain, kAnchorNone},
       "http://ex.com/redirect/http://test.ex.com/",
       true},

      {{"/abcd/*"}, "https://ex.com/abcd/", false},
      {{"/abcd/*"}, "http://ex.com/abcd/picture.jpeg", false},
      {{"/abcd/*"}, "https://ex.com/abcd", true},
      {{"/abcd/*"}, "http://abcd.ex.com", true},
      {{"*/abcd/"}, "https://ex.com/abcd/", false},
      {{"*/abcd/"}, "http://ex.com/abcd/picture.jpeg", false},
      {{"*/abcd/"}, "https://ex.com/test-abcd/", true},
      {{"*/abcd/"}, "http://abcd.ex.com", true},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message() << "UrlPattern: " << test_case.url_pattern
                                      << "; URL: " << test_case.url);

    ASSERT_TRUE(AddUrlRule(MakeUrlRule(test_case.url_pattern)));
    Finish();

    EXPECT_EQ(test_case.expect_allowed, ShouldAllow(test_case.url));
    Reset();
  }
}

TEST_F(SubresourceFilterIndexedRulesetTest, OneRuleWithThirdParty) {
  const struct {
    const char* url_pattern;
    proto::SourceType source_type;

    const char* url;
    const char* document_origin;
    bool expect_allowed;
  } kTestCases[] = {
      {"ex.com", kThirdParty, "http://ex.com", "http://exmpl.org", false},
      {"ex.com", kThirdParty, "http://ex.com", "http://ex.com", true},
      {"ex.com", kThirdParty, "http://ex.com/path?k=v", "http://exmpl.org",
       false},
      {"ex.com", kThirdParty, "http://ex.com/path?k=v", "http://ex.com", true},
      {"ex.com", kFirstParty, "http://ex.com/path?k=v", "http://ex.com", false},
      {"ex.com", kFirstParty, "http://ex.com/path?k=v", "http://exmpl.com",
       true},
      {"ex.com", kAnyParty, "http://ex.com/path?k=v", "http://ex.com", false},
      {"ex.com", kAnyParty, "http://ex.com/path?k=v", "http://exmpl.com",
       false},
      {"ex.com", kThirdParty, "http://subdomain.ex.com", "http://ex.com", true},
      {"ex.com", kThirdParty, "http://ex.com", nullptr, false},

      // Public Suffix List tests.
      {"ex.com", kThirdParty, "http://two.ex.com", "http://one.ex.com", true},
      {"ex.com", kThirdParty, "http://ex.com", "http://one.ex.com", true},
      {"ex.com", kThirdParty, "http://two.ex.com", "http://ex.com", true},
      {"ex.com", kThirdParty, "http://ex.com", "http://ex.org", false},
      {"appspot.com", kThirdParty, "http://two.appspot.org",
       "http://one.appspot.com", true},
  };

  for (auto test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "UrlPattern: " << test_case.url_pattern
                 << "; SourceType: " << static_cast<int>(test_case.source_type)
                 << "; URL: " << test_case.url
                 << "; DocumentOrigin: " << test_case.document_origin);

    auto rule = MakeUrlRule(UrlPattern(test_case.url_pattern, kSubstring));
    rule.set_source_type(test_case.source_type);
    ASSERT_TRUE(AddUrlRule(rule));
    Finish();

    EXPECT_EQ(test_case.expect_allowed,
              ShouldAllow(test_case.url, test_case.document_origin));
    Reset();
  }
}

TEST_F(SubresourceFilterIndexedRulesetTest, OneRuleWithDomainList) {
  constexpr const char* kUrl = "http://example.com";

  const struct {
    std::vector<std::string> domains;
    const char* document_origin;
    bool expect_allowed;
  } kTestCases[] = {
      {std::vector<std::string>(), nullptr, false},
      {std::vector<std::string>(), "http://domain.com", false},

      {{"domain.com"}, nullptr, true},
      {{"domain.com"}, "http://domain.com", false},
      {{"ddomain.com"}, "http://domain.com", true},
      {{"domain.com"}, "http://ddomain.com", true},
      {{"domain.com"}, "http://sub.domain.com", false},
      {{"sub.domain.com"}, "http://domain.com", true},
      {{"sub.domain.com"}, "http://sub.domain.com", false},
      {{"sub.domain.com"}, "http://a.b.c.sub.domain.com", false},
      {{"sub.domain.com"}, "http://sub.domain.com.com", true},

      // TODO(pkalinnikov): Probably need to canonicalize domain patterns to
      // avoid subtleties like below.
      {{"domain.com"}, "http://domain.com.", false},
      {{"domain.com"}, "http://.domain.com", false},
      {{"domain.com"}, "http://.domain.com.", false},
      {{".domain.com"}, "http://.domain.com", false},
      {{"domain.com."}, "http://domain.com", true},
      {{"domain.com."}, "http://domain.com.", false},

      {{"domain..com"}, "http://domain.com", true},
      {{"domain.com"}, "http://domain..com", true},
      {{"domain..com"}, "http://domain..com", false},

      {{"~domain.com"}, nullptr, false},
      {{"~domain.com"}, "http://domain.com", true},
      {{"~ddomain.com"}, "http://domain.com", false},
      {{"~domain.com"}, "http://ddomain.com", false},
      {{"~domain.com"}, "http://sub.domain.com", true},
      {{"~sub.domain.com"}, "http://domain.com", false},
      {{"~sub.domain.com"}, "http://sub.domain.com", true},
      {{"~sub.domain.com"}, "http://a.b.c.sub.domain.com", true},
      {{"~sub.domain.com"}, "http://sub.domain.com.com", false},

      {{"domain1.com", "domain2.com"}, nullptr, true},
      {{"domain1.com", "domain2.com"}, "http://domain1.com", false},
      {{"domain1.com", "domain2.com"}, "http://domain2.com", false},
      {{"domain1.com", "domain2.com"}, "http://domain3.com", true},
      {{"domain1.com", "domain2.com"}, "http://not_domain1.com", true},
      {{"domain1.com", "domain2.com"}, "http://sub.domain1.com", false},
      {{"domain1.com", "domain2.com"}, "http://a.b.c.sub.domain2.com", false},

      {{"~domain1.com", "~domain2.com"}, "http://domain1.com", true},
      {{"~domain1.com", "~domain2.com"}, "http://domain2.com", true},
      {{"~domain1.com", "~domain2.com"}, "http://domain3.com", false},

      {{"domain.com", "~sub.domain.com"}, "http://domain.com", false},
      {{"domain.com", "~sub.domain.com"}, "http://sub.domain.com", true},
      {{"domain.com", "~sub.domain.com"}, "http://a.b.sub.domain.com", true},
      {{"domain.com", "~sub.domain.com"}, "http://ssub.domain.com", false},

      {{"domain.com", "~a.domain.com", "~b.domain.com"},
       "http://domain.com",
       false},
      {{"domain.com", "~a.domain.com", "~b.domain.com"},
       "http://a.domain.com",
       true},
      {{"domain.com", "~a.domain.com", "~b.domain.com"},
       "http://b.domain.com",
       true},

      {{"domain.com", "~a.domain.com", "b.a.domain.com"},
       "http://domain.com",
       false},
      {{"domain.com", "~a.domain.com", "b.a.domain.com"},
       "http://a.domain.com",
       true},
      {{"domain.com", "~a.domain.com", "b.a.domain.com"},
       "http://b.a.domain.com",
       false},
      {{"domain.com", "~a.domain.com", "b.a.domain.com"},
       "http://c.b.a.domain.com",
       false},

      // The following test addresses a former bug in domain list matcher. When
      // "domain.com" was matched, the positive filters lookup stopped, and the
      // next domain was considered as a negative. The initial character was
      // skipped (supposing it's a '~') and the remainder was considered a
      // domain. So "ddomain.com" would be matched and thus the whole rule would
      // be classified as non-matching, which is not correct.
      {{"domain.com", "ddomain.com", "~sub.domain.com"},
       "http://domain.com",
       false},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "Domains: " << ::testing::PrintToString(test_case.domains)
                 << "; DocumentOrigin: " << test_case.document_origin);

    auto rule = MakeUrlRule(UrlPattern(kUrl, kSubstring));
    AddDomains(test_case.domains, &rule);
    ASSERT_TRUE(AddUrlRule(rule));
    Finish();

    EXPECT_EQ(test_case.expect_allowed,
              ShouldAllow(kUrl, test_case.document_origin));
    Reset();
  }
}

TEST_F(SubresourceFilterIndexedRulesetTest, OneRuleWithLongDomainList) {
  constexpr const char* kUrl = "http://example.com";
  constexpr size_t kDomains = 200;

  std::vector<std::string> domains;
  for (size_t i = 0; i < kDomains; ++i) {
    const std::string domain = "domain" + std::to_string(i) + ".com";
    domains.push_back(domain);
    domains.push_back("~sub." + domain);
    domains.push_back("a.sub." + domain);
    domains.push_back("b.sub." + domain);
    domains.push_back("c.sub." + domain);
    domains.push_back("~aa.sub." + domain);
    domains.push_back("~ab.sub." + domain);
    domains.push_back("~ba.sub." + domain);
    domains.push_back("~bb.sub." + domain);
    domains.push_back("~sub.sub.c.sub." + domain);
  }

  auto rule = MakeUrlRule(UrlPattern(kUrl, kSubstring));
  AddDomains(domains, &rule);
  ASSERT_TRUE(AddUrlRule(rule));
  Finish();

  for (size_t i = 0; i < kDomains; ++i) {
    SCOPED_TRACE(::testing::Message() << "Iteration: " << i);
    const std::string domain = "domain" + std::to_string(i) + ".com";

    EXPECT_FALSE(ShouldAllow(kUrl, "http://" + domain));
    EXPECT_TRUE(ShouldAllow(kUrl, "http://sub." + domain));
    EXPECT_FALSE(ShouldAllow(kUrl, "http://a.sub." + domain));
    EXPECT_FALSE(ShouldAllow(kUrl, "http://b.sub." + domain));
    EXPECT_FALSE(ShouldAllow(kUrl, "http://c.sub." + domain));
    EXPECT_TRUE(ShouldAllow(kUrl, "http://aa.sub." + domain));
    EXPECT_TRUE(ShouldAllow(kUrl, "http://ab.sub." + domain));
    EXPECT_TRUE(ShouldAllow(kUrl, "http://ba.sub." + domain));
    EXPECT_TRUE(ShouldAllow(kUrl, "http://bb.sub." + domain));
    EXPECT_FALSE(ShouldAllow(kUrl, "http://sub.c.sub." + domain));
    EXPECT_TRUE(ShouldAllow(kUrl, "http://sub.sub.c.sub." + domain));
  }
}

TEST_F(SubresourceFilterIndexedRulesetTest, OneRuleWithElementTypes) {
  constexpr auto kAll = kAllElementTypes;
  const struct {
    const char* url_pattern;
    int32_t element_types;

    const char* url;
    proto::ElementType element_type;
    bool expect_allowed;
  } kTestCases[] = {
      {"ex.com", kAll, "http://ex.com/img.jpg", kImage, false},
      {"ex.com", kAll & ~kPopup, "http://ex.com/img", kPopup, true},

      {"ex.com", kImage, "http://ex.com/img.jpg", kImage, false},
      {"ex.com", kAll & ~kImage, "http://ex.com/img.jpg", kImage, true},
      {"ex.com", kScript, "http://ex.com/img.jpg", kImage, true},
      {"ex.com", kAll & ~kScript, "http://ex.com/img.jpg", kImage, false},

      {"ex.com", kImage | kFont, "http://ex.com/font", kFont, false},
      {"ex.com", kImage | kFont, "http://ex.com/image", kImage, false},
      {"ex.com", kImage | kFont, "http://ex.com/video",
       proto::ELEMENT_TYPE_MEDIA, true},
      {"ex.com", kAll & ~kFont & ~kScript, "http://ex.com/font", kFont, true},
      {"ex.com", kAll & ~kFont & ~kScript, "http://ex.com/scr", kScript, true},
      {"ex.com", kAll & ~kFont & ~kScript, "http://ex.com/img", kImage, false},

      {"ex.com", kAll, "http://ex.com", proto::ELEMENT_TYPE_OTHER, false},
      {"ex.com", kAll, "http://ex.com", proto::ELEMENT_TYPE_UNSPECIFIED, true},
      {"ex.com", kWebSocket, "ws://ex.com", proto::ELEMENT_TYPE_WEBSOCKET,
       false},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(
        ::testing::Message()
        << "UrlPattern: " << test_case.url_pattern
        << "; ElementTypes: " << static_cast<int>(test_case.element_types)
        << "; URL: " << test_case.url
        << "; ElementType: " << static_cast<int>(test_case.element_type));

    auto rule = MakeUrlRule(UrlPattern(test_case.url_pattern, kSubstring));
    rule.set_element_types(test_case.element_types);
    ASSERT_TRUE(AddUrlRule(rule));
    Finish();

    EXPECT_EQ(test_case.expect_allowed,
              ShouldAllow(test_case.url, nullptr /* document_origin */,
                          test_case.element_type));
    Reset();
  }
}

TEST_F(SubresourceFilterIndexedRulesetTest, OneRuleWithActivationTypes) {
  constexpr proto::ActivationType kNone = proto::ACTIVATION_TYPE_UNSPECIFIED;

  const struct {
    const char* url_pattern;
    int32_t activation_types;

    const char* document_url;
    proto::ActivationType activation_type;
    bool expect_disabled;
  } kTestCases[] = {
      {"example.com", kDocument, "http://example.com", kDocument, true},
      {"xample.com", kDocument, "http://example.com", kDocument, true},
      {"exampl.com", kDocument, "http://example.com", kDocument, false},

      {"example.com", kGenericBlock, "http://example.com", kDocument, false},
      {"example.com", kDocument, "http://example.com", kNone, false},
      {"example.com", kGenericBlock, "http://example.com", kNone, false},

      // Invalid GURL.
      {"example.com", kDocument, "http;//example.com", kDocument, false},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(
        ::testing::Message()
        << "UrlPattern: " << test_case.url_pattern
        << "; ActivationTypes: " << static_cast<int>(test_case.activation_types)
        << "; DocumentURL: " << test_case.document_url
        << "; ActivationType: " << static_cast<int>(test_case.activation_type));

    ASSERT_TRUE(AddSimpleWhitelistRule(test_case.url_pattern,
                                       test_case.activation_types));
    Finish();

    EXPECT_EQ(test_case.expect_disabled,
              ShouldDeactivate(test_case.document_url,
                               nullptr /* parent_document_origin */,
                               test_case.activation_type));
    EXPECT_EQ(test_case.expect_disabled,
              ShouldDeactivate(test_case.document_url, "http://example.com/",
                               test_case.activation_type));
    EXPECT_EQ(test_case.expect_disabled,
              ShouldDeactivate(test_case.document_url, "http://xmpl.com/",
                               test_case.activation_type));
    Reset();
  }
}

TEST_F(SubresourceFilterIndexedRulesetTest, RuleWithElementAndActivationTypes) {
  auto rule = MakeUrlRule(UrlPattern("allow.ex.com", kSubstring));
  rule.set_semantics(proto::RULE_SEMANTICS_WHITELIST);
  rule.set_activation_types(kDocument);

  ASSERT_TRUE(AddUrlRule(rule));
  ASSERT_TRUE(AddSimpleRule("ex.com"));
  Finish();

  EXPECT_FALSE(ShouldAllow("http://ex.com"));
  EXPECT_TRUE(ShouldAllow("http://allow.ex.com"));
  EXPECT_FALSE(ShouldDeactivate("http://allow.ex.com",
                                nullptr /* parent_document_origin */,
                                kGenericBlock));
  EXPECT_TRUE(ShouldDeactivate(
      "http://allow.ex.com", nullptr /* parent_document_origin */, kDocument));
}

TEST_F(SubresourceFilterIndexedRulesetTest, MatchWithDisableGenericRules) {
  const struct {
    const char* url_pattern;
    std::vector<std::string> domains;
  } kRules[] = {
      // Generic rules.
      {"some_text", std::vector<std::string>()},
      {"another_text", {"~example.com"}},
      {"final_text", {"~example1.com", "~example2.com"}},
      // Domain specific rules.
      {"some_text", {"example1.com"}},
      {"more_text", {"example.com", "~exclude.example.com"}},
      {"last_text", {"example1.com", "sub.example2.com"}},
  };

  for (const auto& rule_data : kRules) {
    auto rule = MakeUrlRule(UrlPattern(rule_data.url_pattern, kSubstring));
    AddDomains(rule_data.domains, &rule);
    ASSERT_TRUE(AddUrlRule(rule))
        << "UrlPattern: " << rule_data.url_pattern
        << "; Domains: " << ::testing::PrintToString(rule_data.domains);
  }

  // Note: Some of the rules have common domains (e.g., example1.com), which are
  // ultimately shared by FlatBuffers' CreateSharedString. The test also makes
  // sure that the data structure works properly with such optimization.
  Finish();

  const struct {
    const char* url;
    const char* document_origin;
    bool should_allow_with_disable_generic_rules;
    bool should_allow_with_enable_all_rules;
  } kTestCases[] = {
      {"http://ex.com/some_text", "http://example.com", true, false},
      {"http://ex.com/some_text", "http://example1.com", false, false},

      {"http://ex.com/another_text", "http://example.com", true, true},
      {"http://ex.com/another_text", "http://example1.com", true, false},

      {"http://ex.com/final_text", "http://example.com", true, false},
      {"http://ex.com/final_text", "http://example1.com", true, true},
      {"http://ex.com/final_text", "http://example2.com", true, true},

      {"http://ex.com/more_text", "http://example.com", false, false},
      {"http://ex.com/more_text", "http://exclude.example.com", true, true},
      {"http://ex.com/more_text", "http://example1.com", true, true},

      {"http://ex.com/last_text", "http://example.com", true, true},
      {"http://ex.com/last_text", "http://example1.com", false, false},
      {"http://ex.com/last_text", "http://example2.com", true, true},
      {"http://ex.com/last_text", "http://sub.example2.com", false, false},
  };

  constexpr bool kDisableGenericRules = true;
  constexpr bool kEnableAllRules = false;
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "URL: " << test_case.url
                 << "; DocumentOrigin: " << test_case.document_origin);

    EXPECT_EQ(test_case.should_allow_with_disable_generic_rules,
              ShouldAllow(test_case.url, test_case.document_origin, kOther,
                          kDisableGenericRules));
    EXPECT_EQ(test_case.should_allow_with_enable_all_rules,
              ShouldAllow(test_case.url, test_case.document_origin, kOther,
                          kEnableAllRules));
  }
}

TEST_F(SubresourceFilterIndexedRulesetTest, EmptyRuleset) {
  Finish();
  EXPECT_TRUE(ShouldAllow(nullptr));
  EXPECT_TRUE(ShouldAllow("http://example.com"));
  EXPECT_TRUE(ShouldAllow("http://another.example.com?param=val"));
}

TEST_F(SubresourceFilterIndexedRulesetTest, NoRuleApplies) {
  ASSERT_TRUE(AddSimpleRule("?filter_out="));
  ASSERT_TRUE(AddSimpleRule("&filter_out="));
  Finish();

  EXPECT_TRUE(ShouldAllow("http://example.com"));
  EXPECT_TRUE(ShouldAllow("http://example.com?filter_not"));
}

TEST_F(SubresourceFilterIndexedRulesetTest, SimpleBlacklist) {
  ASSERT_TRUE(AddSimpleRule("?param="));
  Finish();

  EXPECT_TRUE(ShouldAllow("https://example.com"));
  EXPECT_FALSE(ShouldAllow("http://example.org?param=image1"));
}

TEST_F(SubresourceFilterIndexedRulesetTest, SimpleWhitelist) {
  ASSERT_TRUE(AddSimpleWhitelistRule("example.com/?filter_out="));
  Finish();

  EXPECT_TRUE(ShouldAllow("https://example.com?filter_out=true"));
}

TEST_F(SubresourceFilterIndexedRulesetTest, SimpleBlacklistAndWhitelist) {
  ASSERT_TRUE(AddSimpleRule("?filter="));
  ASSERT_TRUE(AddSimpleWhitelistRule("whitelisted.com/?filter="));
  Finish();

  EXPECT_FALSE(ShouldAllow("http://blacklisted.com?filter=on"));
  EXPECT_TRUE(ShouldAllow("https://whitelisted.com?filter=on"));
  EXPECT_TRUE(ShouldAllow("https://notblacklisted.com"));
}

TEST_F(SubresourceFilterIndexedRulesetTest, BlacklistAndActivationType) {
  ASSERT_TRUE(AddSimpleRule("example.com"));
  ASSERT_TRUE(AddSimpleWhitelistRule("example.com", kDocument));
  Finish();

  EXPECT_TRUE(ShouldDeactivate("https://example.com", nullptr, kDocument));
  EXPECT_FALSE(ShouldDeactivate("https://xample.com", nullptr, kDocument));
  EXPECT_FALSE(ShouldAllow("https://example.com"));
  EXPECT_TRUE(ShouldAllow("https://xample.com"));
}

TEST_F(SubresourceFilterIndexedRulesetTest, RulesWithUnsupportedTypes) {
  const struct {
    int element_types;
    int activation_types;
  } kRules[] = {
      {proto::ELEMENT_TYPE_MAX << 1, 0},
      {0, proto::ACTIVATION_TYPE_MAX << 1},
      {proto::ELEMENT_TYPE_MAX << 1, proto::ACTIVATION_TYPE_MAX << 1},

      {kPopup, 0},
      {0, proto::ACTIVATION_TYPE_ELEMHIDE},
      {0, proto::ACTIVATION_TYPE_GENERICHIDE},
      {0, proto::ACTIVATION_TYPE_ELEMHIDE | proto::ACTIVATION_TYPE_GENERICHIDE},
      {proto::ELEMENT_TYPE_POPUP, proto::ACTIVATION_TYPE_ELEMHIDE},
  };

  for (const auto& rule_data : kRules) {
    auto rule = MakeUrlRule(UrlPattern("example.com", kSubstring));
    rule.set_element_types(rule_data.element_types);
    rule.set_activation_types(rule_data.activation_types);
    EXPECT_FALSE(AddUrlRule(rule))
        << "ElementTypes: " << static_cast<int>(rule_data.element_types)
        << "; ActivationTypes: "
        << static_cast<int>(rule_data.activation_types);
  }
  ASSERT_TRUE(AddSimpleRule("exmpl.com"));
  Finish();

  EXPECT_TRUE(ShouldAllow("http://example.com/"));
  EXPECT_FALSE(ShouldAllow("https://exmpl.com/"));
}

TEST_F(SubresourceFilterIndexedRulesetTest,
       RulesWithSupportedAndUnsupportedTypes) {
  const struct {
    int element_types;
    int activation_types;
  } kRules[] = {
      {kImage | (proto::ELEMENT_TYPE_MAX << 1), 0},
      {kScript | kPopup, 0},
      {0, kDocument | (proto::ACTIVATION_TYPE_MAX << 1)},
  };

  for (const auto& rule_data : kRules) {
    auto rule = MakeUrlRule(UrlPattern("example.com", kSubstring));
    rule.set_element_types(rule_data.element_types);
    rule.set_activation_types(rule_data.activation_types);
    if (rule_data.activation_types)
      rule.set_semantics(proto::RULE_SEMANTICS_WHITELIST);
    EXPECT_TRUE(AddUrlRule(rule))
        << "ElementTypes: " << static_cast<int>(rule_data.element_types)
        << "; ActivationTypes: "
        << static_cast<int>(rule_data.activation_types);
  }
  Finish();

  EXPECT_FALSE(ShouldAllow("http://example.com/", nullptr, kImage));
  EXPECT_FALSE(ShouldAllow("http://example.com/", nullptr, kScript));
  EXPECT_TRUE(ShouldAllow("http://example.com/", nullptr, kPopup));
  EXPECT_TRUE(ShouldAllow("http://example.com/"));

  EXPECT_TRUE(ShouldDeactivate("http://example.com", nullptr, kDocument));
  EXPECT_FALSE(ShouldDeactivate("http://example.com", nullptr, kGenericBlock));
}

}  // namespace subresource_filter
