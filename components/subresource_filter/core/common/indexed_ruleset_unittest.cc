// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/indexed_ruleset.h"

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "components/subresource_filter/core/common/first_party_origin.h"
#include "components/subresource_filter/core/common/proto/rules.pb.h"
#include "components/subresource_filter/core/common/url_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

namespace {

constexpr proto::AnchorType kAnchorNone = proto::ANCHOR_TYPE_NONE;
constexpr proto::AnchorType kBoundary = proto::ANCHOR_TYPE_BOUNDARY;
constexpr proto::AnchorType kSubdomain = proto::ANCHOR_TYPE_SUBDOMAIN;
constexpr proto::UrlPatternType kSubstring = proto::URL_PATTERN_TYPE_SUBSTRING;

constexpr proto::SourceType kAnyParty = proto::SOURCE_TYPE_ANY;
constexpr proto::SourceType kFirstParty = proto::SOURCE_TYPE_FIRST_PARTY;
constexpr proto::SourceType kThirdParty = proto::SOURCE_TYPE_THIRD_PARTY;

constexpr proto::ElementType kAllElementTypes = proto::ELEMENT_TYPE_ALL;
constexpr proto::ElementType kOther = proto::ELEMENT_TYPE_OTHER;
constexpr proto::ElementType kImage = proto::ELEMENT_TYPE_IMAGE;
constexpr proto::ElementType kFont = proto::ELEMENT_TYPE_FONT;
constexpr proto::ElementType kScript = proto::ELEMENT_TYPE_SCRIPT;
constexpr proto::ElementType kPopup = proto::ELEMENT_TYPE_POPUP;
constexpr proto::ElementType kWebSocket = proto::ELEMENT_TYPE_WEBSOCKET;

constexpr proto::ActivationType kDocument = proto::ACTIVATION_TYPE_DOCUMENT;
constexpr proto::ActivationType kGenericBlock =
    proto::ACTIVATION_TYPE_GENERICBLOCK;

// Note: Returns unique origin on origin_string == nullptr.
url::Origin GetOrigin(const char* origin_string) {
  return origin_string ? url::Origin(GURL(origin_string)) : url::Origin();
}

class UrlRuleBuilder {
 public:
  explicit UrlRuleBuilder(const UrlPattern& url_pattern,
                          bool is_whitelist = false)
      : UrlRuleBuilder(url_pattern, kAnyParty, is_whitelist) {}

  UrlRuleBuilder(const UrlPattern& url_pattern,
                 proto::SourceType source_type,
                 bool is_whitelist) {
    rule_.set_semantics(is_whitelist ? proto::RULE_SEMANTICS_WHITELIST
                                     : proto::RULE_SEMANTICS_BLACKLIST);

    rule_.set_source_type(source_type);
    rule_.set_element_types(kAllElementTypes);

    rule_.set_url_pattern_type(url_pattern.type());
    rule_.set_anchor_left(url_pattern.anchor_left());
    rule_.set_anchor_right(url_pattern.anchor_right());
    rule_.set_match_case(url_pattern.match_case());
    rule_.set_url_pattern(url_pattern.url_pattern().as_string());
  }

  UrlRuleBuilder& AddDomain(std::string domain_pattern) {
    DCHECK(!domain_pattern.empty());
    auto* domain = rule_.add_domains();
    if (domain_pattern[0] == '~') {
      domain_pattern.erase(0, 1);
      domain->set_exclude(true);
    }
    domain->set_domain(domain_pattern);
    return *this;
  }

  UrlRuleBuilder& AddDomains(const std::vector<std::string>& domains) {
    for (const std::string domain : domains)
      AddDomain(domain);
    return *this;
  }

  const proto::UrlRule& rule() const { return rule_; }
  proto::UrlRule& rule() { return rule_; }

 private:
  proto::UrlRule rule_;

  DISALLOW_COPY_AND_ASSIGN(UrlRuleBuilder);
};

}  // namespace

class SubresourceFilterIndexedRulesetTest : public testing::Test {
 public:
  SubresourceFilterIndexedRulesetTest() = default;

 protected:
  bool ShouldAllow(const char* url,
                   const char* document_origin = nullptr,
                   proto::ElementType element_type = kOther,
                   bool disable_generic_rules = false) const {
    DCHECK_NE(matcher_.get(), nullptr);
    url::Origin origin = GetOrigin(document_origin);
    FirstPartyOrigin first_party(origin);
    return !matcher_->ShouldDisallowResourceLoad(
        GURL(url), first_party, element_type, disable_generic_rules);
  }

  bool ShouldAllow(const char* url,
                   const char* document_origin,
                   bool disable_generic_rules) const {
    return ShouldAllow(url, document_origin, kOther, disable_generic_rules);
  }

  bool ShouldDeactivate(const char* document_url,
                        const char* parent_document_origin = nullptr,
                        proto::ActivationType activation_type =
                            proto::ACTIVATION_TYPE_UNSPECIFIED) const {
    DCHECK(matcher_);
    url::Origin origin = GetOrigin(parent_document_origin);
    return matcher_->ShouldDisableFilteringForDocument(GURL(document_url),
                                                       origin, activation_type);
  }

  void AddUrlRule(const proto::UrlRule& rule) {
    ASSERT_TRUE(indexer_.AddUrlRule(rule)) << "URL pattern: "
                                           << rule.url_pattern();
  }

  void AddSimpleRule(const UrlPattern& url_pattern, bool is_whitelist) {
    AddUrlRule(UrlRuleBuilder(url_pattern, is_whitelist).rule());
  }

  void AddBlacklistRule(const UrlPattern& url_pattern,
                        proto::SourceType source_type = kAnyParty) {
    AddUrlRule(UrlRuleBuilder(url_pattern, source_type, false).rule());
  }

  void AddWhitelistRuleWithActivationTypes(const UrlPattern& url_pattern,
                                           int32_t activation_types) {
    UrlRuleBuilder builder(url_pattern, kAnyParty, true);
    builder.rule().set_element_types(proto::ELEMENT_TYPE_UNSPECIFIED);
    builder.rule().set_activation_types(activation_types);
    AddUrlRule(builder.rule());
  }

  void Finish() {
    indexer_.Finish();
    matcher_.reset(new IndexedRulesetMatcher(indexer_.data(), indexer_.size()));
  }

  void Reset() {
    matcher_.reset(nullptr);
    indexer_.~RulesetIndexer();
    new (&indexer_) RulesetIndexer();
  }

  RulesetIndexer indexer_;
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
      {{"abcd", kSubstring}, "http://example.com/abcd", false},
      {{"abcd", kSubstring}, "http://example.com/dcab", true},
      {{"42", kSubstring}, "http://example.com/adcd/picture42.png", false},
      {{"&test", kSubstring},
       "http://example.com/params?para1=false&test=true",
       false},
      {{"-test-42.", kSubstring}, "http://example.com/unit-test-42.1", false},
      {{"/abcdtest160x600.", kSubstring},
       "http://example.com/abcdtest160x600.png",
       false},

      // WILDCARDED
      {{"http://example.com/abcd/picture*.png"},
       "http://example.com/abcd/picture42.png",
       false},
      {{"example.com", kSubdomain, kAnchorNone}, "http://example.com", false},
      {{"example.com", kSubdomain, kAnchorNone},
       "http://test.example.com",
       false},
      {{"example.com", kSubdomain, kAnchorNone},
       "https://test.example.com.com",
       false},
      {{"example.com", kSubdomain, kAnchorNone},
       "https://test.rest.example.com",
       false},
      {{"example.com", kSubdomain, kAnchorNone},
       "https://test_example.com",
       true},

      {{"http://example.com", kBoundary, kAnchorNone},
       "http://example.com/",
       false},
      {{"http://example.com", kBoundary, kAnchorNone},
       "http://example.com/42",
       false},
      {{"http://example.com", kBoundary, kAnchorNone},
       "http://example.com/42/http://example.com/",
       false},
      {{"http://example.com", kBoundary, kAnchorNone},
       "http://example.com/42/http://example.info/",
       false},
      {{"http://example.com/", kBoundary, kBoundary},
       "http://example.com",
       false},
      {{"http://example.com/", kBoundary, kBoundary},
       "http://example.com/42",
       true},
      {{"http://example.com/", kBoundary, kBoundary},
       "http://example.info/42/http://example.com/",
       true},
      {{"http://example.com/", kBoundary, kBoundary},
       "http://example.info/42/http://example.com/",
       true},
      {{"http://example.com/", kBoundary, kBoundary},
       "http://example.com/",
       false},
      {{"http://example.com/", kBoundary, kBoundary},
       "http://example.com/42.swf",
       true},
      {{"http://example.com/", kBoundary, kBoundary},
       "http://example.info/redirect/http://example.com/",
       true},
      {{"pdf", kAnchorNone, kBoundary}, "http://example.com/abcd.pdf", false},
      {{"pdf", kAnchorNone, kBoundary}, "http://example.com/pdfium", true},
      {{"http://example.com^"}, "http://example.com/", false},
      {{"http://example.com^"}, "http://example.com:8000/", false},
      {{"http://example.com^"}, "http://example.com.ru", true},
      {{"^example.com^"},
       "http://example.com:8000/42.loss?a=12&b=%D1%82%D0%B5%D1%81%D1%82",
       false},
      {{"^42.loss^"},
       "http://example.com:8000/42.loss?a=12&b=%D1%82%D0%B5%D1%81%D1%82",
       false},

      // FIXME(pkalinnikov): The '^' at the end should match end-of-string.
      // {"^%D1%82%D0%B5%D1%81%D1%82^",
      //  "http://example.com:8000/42.loss?a=12&b=%D1%82%D0%B5%D1%81%D1%82",
      //  false},
      // {"/abcd/*/picture^", "http://example.com/abcd/42/picture", false},

      {{"/abcd/*/picture^"},
       "http://example.com/abcd/42/loss/picture?param",
       false},
      {{"/abcd/*/picture^"}, "http://example.com/abcd//picture/42", false},
      {{"/abcd/*/picture^"}, "http://example.com/abcd/picture", true},
      {{"/abcd/*/picture^"}, "http://example.com/abcd/42/pictureraph", true},
      {{"/abcd/*/picture^"}, "http://example.com/abcd/42/picture.swf", true},
      {{"test.example.com^", kSubdomain, kAnchorNone},
       "http://test.example.com/42.swf",
       false},
      {{"test.example.com^", kSubdomain, kAnchorNone},
       "http://server1.test.example.com/42.swf",
       false},
      {{"test.example.com^", kSubdomain, kAnchorNone},
       "https://test.example.com:8000/",
       false},
      {{"test.example.com^", kSubdomain, kAnchorNone},
       "http://test.example.com.ua/42.swf",
       true},
      {{"test.example.com^", kSubdomain, kAnchorNone},
       "http://example.com/redirect/http://test.example.com/",
       true},

      {{"/abcd/*"}, "https://example.com/abcd/", false},
      {{"/abcd/*"}, "http://example.com/abcd/picture.jpeg", false},
      {{"/abcd/*"}, "https://example.com/abcd", true},
      {{"/abcd/*"}, "http://abcd.example.com", true},
      {{"*/abcd/"}, "https://example.com/abcd/", false},
      {{"*/abcd/"}, "http://example.com/abcd/picture.jpeg", false},
      {{"*/abcd/"}, "https://example.com/test-abcd/", true},
      {{"*/abcd/"}, "http://abcd.example.com", true},

      // FIXME(pkalinnikov): Implement REGEXP matching.
      // REGEXP
      // {"/test|rest\\d+/", "http://example.com/test42", false},
      // {"/test|rest\\d+/", "http://example.com/test", false},
      // {"/test|rest\\d+/", "http://example.com/rest42", false},
      // {"/test|rest\\d+/", "http://example.com/rest", true},
      // {"/example\\.com/.*\\/[a-zA-Z0-9]{3}/", "http://example.com/abcd/42y",
      //  false},
      // {"/example\\.com/.*\\/[a-zA-Z0-9]{3}/", "http://example.com/abcd/%42y",
      //  true},
      // {"||example.com^*/test.htm", "http://example.com/unit/test.html",
      // false},
      // {"||example.com^*/test.htm", "http://examole.com/test.htm", true},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "Rule: " << test_case.url_pattern
                                    << "; URL: " << test_case.url);

    AddBlacklistRule(test_case.url_pattern);
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
      {"example.com", kThirdParty, "http://example.com", "http://exmpl.org",
       false},
      {"example.com", kThirdParty, "http://example.com", "http://example.com",
       true},
      {"example.com", kThirdParty, "http://example.com/path?k=v",
       "http://exmpl.org", false},
      {"example.com", kThirdParty, "http://example.com/path?k=v",
       "http://example.com", true},
      {"example.com", kFirstParty, "http://example.com/path?k=v",
       "http://example.com", false},
      {"example.com", kFirstParty, "http://example.com/path?k=v",
       "http://exmpl.com", true},
      {"example.com", kAnyParty, "http://example.com/path?k=v",
       "http://example.com", false},
      {"example.com", kAnyParty, "http://example.com/path?k=v",
       "http://exmpl.com", false},
      {"example.com", kThirdParty, "http://subdomain.example.com",
       "http://example.com", true},
      {"example.com", kThirdParty, "http://example.com", nullptr, false},

      // Public Suffix List tests.
      {"example.com", kThirdParty, "http://two.example.com",
       "http://one.example.com", true},
      {"example.com", kThirdParty, "http://example.com",
       "http://one.example.com", true},
      {"example.com", kThirdParty, "http://two.example.com",
       "http://example.com", true},
      {"example.com", kThirdParty, "http://example.com", "http://example.org",
       false},
      {"appspot.com", kThirdParty, "http://two.appspot.org",
       "http://one.appspot.com", true},
  };

  for (auto test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Rule: " << test_case.url_pattern << "; source: "
                 << (int)test_case.source_type << "; URL: " << test_case.url
                 << "; document: " << test_case.document_origin);

    AddBlacklistRule(UrlPattern(test_case.url_pattern, kSubstring),
                     test_case.source_type);
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
    SCOPED_TRACE(testing::Message()
                 << "Domains: " << base::JoinString(test_case.domains, "|")
                 << "; document: " << test_case.document_origin);

    UrlRuleBuilder builder(UrlPattern(kUrl, kSubstring));
    builder.AddDomains(test_case.domains);
    AddUrlRule(builder.rule());
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

  UrlRuleBuilder builder(UrlPattern(kUrl, kSubstring));
  builder.AddDomains(domains);
  AddUrlRule(builder.rule());
  Finish();

  for (size_t i = 0; i < kDomains; ++i) {
    SCOPED_TRACE(testing::Message() << "Iteration: " << i);
    const std::string domain = "domain" + std::to_string(i) + ".com";

    EXPECT_FALSE(ShouldAllow(kUrl, ("http://" + domain).c_str()));
    EXPECT_TRUE(ShouldAllow(kUrl, ("http://sub." + domain).c_str()));
    EXPECT_FALSE(ShouldAllow(kUrl, ("http://a.sub." + domain).c_str()));
    EXPECT_FALSE(ShouldAllow(kUrl, ("http://b.sub." + domain).c_str()));
    EXPECT_FALSE(ShouldAllow(kUrl, ("http://c.sub." + domain).c_str()));
    EXPECT_TRUE(ShouldAllow(kUrl, ("http://aa.sub." + domain).c_str()));
    EXPECT_TRUE(ShouldAllow(kUrl, ("http://ab.sub." + domain).c_str()));
    EXPECT_TRUE(ShouldAllow(kUrl, ("http://ba.sub." + domain).c_str()));
    EXPECT_TRUE(ShouldAllow(kUrl, ("http://bb.sub." + domain).c_str()));
    EXPECT_FALSE(ShouldAllow(kUrl, ("http://sub.c.sub." + domain).c_str()));
    EXPECT_TRUE(ShouldAllow(kUrl, ("http://sub.sub.c.sub." + domain).c_str()));
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
    SCOPED_TRACE(testing::Message()
                 << "Rule: " << test_case.url_pattern << "; ElementTypes: "
                 << (int)test_case.element_types << "; URL: " << test_case.url
                 << "; ElementType: " << (int)test_case.element_type);

    UrlRuleBuilder builder(UrlPattern(test_case.url_pattern, kSubstring));
    builder.rule().set_element_types(test_case.element_types);
    AddUrlRule(builder.rule());
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
    SCOPED_TRACE(testing::Message()
                 << "Rule: " << test_case.url_pattern
                 << "; ActivationTypes: " << (int)test_case.activation_types
                 << "; DocURL: " << test_case.document_url
                 << "; ActivationType: " << (int)test_case.activation_type);

    AddWhitelistRuleWithActivationTypes(
        UrlPattern(test_case.url_pattern, kSubstring),
        test_case.activation_types);
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
  UrlRuleBuilder builder(UrlPattern("allow.ex.com"), true /* is_whitelist */);
  builder.rule().set_activation_types(kDocument);

  AddUrlRule(builder.rule());
  AddBlacklistRule(UrlPattern("ex.com"));
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
  // Generic rules.
  ASSERT_NO_FATAL_FAILURE(
      AddUrlRule(UrlRuleBuilder(UrlPattern("some_text", kSubstring)).rule()));
  ASSERT_NO_FATAL_FAILURE(
      AddUrlRule(UrlRuleBuilder(UrlPattern("another_text", kSubstring))
                     .AddDomain("~example.com")
                     .rule()));

  // Domain specific rules.
  ASSERT_NO_FATAL_FAILURE(
      AddUrlRule(UrlRuleBuilder(UrlPattern("some_text", kSubstring))
                     .AddDomain("example1.com")
                     .rule()));
  ASSERT_NO_FATAL_FAILURE(
      AddUrlRule(UrlRuleBuilder(UrlPattern("more_text", kSubstring))
                     .AddDomain("example.com")
                     .AddDomain("~exclude.example.com")
                     .rule()));
  ASSERT_NO_FATAL_FAILURE(
      AddUrlRule(UrlRuleBuilder(UrlPattern("last_text", kSubstring))
                     .AddDomain("example1.com")
                     .AddDomain("sub.example2.com")
                     .rule()));
  // Note: Some of the rules have common domains (e.g., example1.com), which are
  // ultimately shared by FlatBuffers' CreateSharedString. The test also makes
  // sure that the data structure works properly with such optimization.

  Finish();

  const struct {
    const char* url_pattern;
    const char* document_origin;
    bool should_allow_with_disable_generic_rules;
    bool should_allow_with_enable_all_rules;
  } kTestCases[] = {
      {"http://ex.com/some_text", "http://example.com", true, false},
      {"http://ex.com/some_text", "http://example1.com", false, false},

      {"http://ex.com/another_text", "http://example.com", true, true},
      {"http://ex.com/another_text", "http://example1.com", true, false},

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
    SCOPED_TRACE(testing::Message()
                 << "Url: " << test_case.url_pattern
                 << "; document: " << test_case.document_origin);

    EXPECT_EQ(test_case.should_allow_with_disable_generic_rules,
              ShouldAllow(test_case.url_pattern, test_case.document_origin,
                          kDisableGenericRules));
    EXPECT_EQ(test_case.should_allow_with_enable_all_rules,
              ShouldAllow(test_case.url_pattern, test_case.document_origin,
                          kEnableAllRules));
  }
}

TEST_F(SubresourceFilterIndexedRulesetTest, EmptyRuleset) {
  Finish();
  EXPECT_TRUE(ShouldAllow("http://example.com"));
  EXPECT_TRUE(ShouldAllow("http://another.example.com?param=val"));
  EXPECT_TRUE(ShouldAllow(nullptr));
}

TEST_F(SubresourceFilterIndexedRulesetTest, NoRuleApplies) {
  AddSimpleRule(UrlPattern("?filtered_content=", kSubstring), false);
  AddSimpleRule(UrlPattern("&filtered_content=", kSubstring), false);
  Finish();

  EXPECT_TRUE(ShouldAllow("http://example.com"));
  EXPECT_TRUE(ShouldAllow("http://example.com?filtered_not"));
}

TEST_F(SubresourceFilterIndexedRulesetTest, SimpleBlacklist) {
  AddSimpleRule(UrlPattern("?param=", kSubstring), false);
  Finish();

  EXPECT_TRUE(ShouldAllow("https://example.com"));
  EXPECT_FALSE(ShouldAllow("http://example.org?param=image1"));
}

TEST_F(SubresourceFilterIndexedRulesetTest, SimpleWhitelist) {
  AddSimpleRule(UrlPattern("example.com/?filtered_content=", kSubstring), true);
  Finish();

  EXPECT_TRUE(ShouldAllow("https://example.com?filtered_content=image1"));
}

TEST_F(SubresourceFilterIndexedRulesetTest, BlacklistWhitelist) {
  AddSimpleRule(UrlPattern("?filter=", kSubstring), false);
  AddSimpleRule(UrlPattern("whitelisted.com/?filter=", kSubstring), true);
  Finish();

  EXPECT_TRUE(ShouldAllow("https://whitelisted.com?filter=off"));
  EXPECT_TRUE(ShouldAllow("https://notblacklisted.com"));
  EXPECT_FALSE(ShouldAllow("http://blacklisted.com?filter=on"));
}

TEST_F(SubresourceFilterIndexedRulesetTest, BlacklistAndActivationType) {
  AddSimpleRule(UrlPattern("example.com", kSubstring), false);
  AddWhitelistRuleWithActivationTypes(UrlPattern("example.com", kSubstring),
                                      kDocument);
  Finish();

  EXPECT_TRUE(ShouldDeactivate("https://example.com", nullptr, kDocument));
  EXPECT_FALSE(ShouldDeactivate("https://xample.com", nullptr, kDocument));
  EXPECT_FALSE(ShouldAllow("https://example.com"));
  EXPECT_TRUE(ShouldAllow("https://xample.com"));
}

TEST_F(SubresourceFilterIndexedRulesetTest, RuleWithUnsupportedTypes) {
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

  for (const auto& rule : kRules) {
    UrlRuleBuilder builder(UrlPattern("example.com"));
    builder.rule().set_element_types(rule.element_types);
    builder.rule().set_activation_types(rule.activation_types);
    EXPECT_FALSE(indexer_.AddUrlRule(builder.rule()));
  }
  AddSimpleRule(UrlPattern("exmpl.com", kSubstring), false);

  Finish();
  EXPECT_TRUE(ShouldAllow("http://example.com/"));
  EXPECT_FALSE(ShouldAllow("https://exmpl.com/"));
}

TEST_F(SubresourceFilterIndexedRulesetTest,
       RuleWithSupportedAndUnsupportedTypes) {
  const struct {
    int element_types;
    int activation_types;
  } kRules[] = {
      {kImage | (proto::ELEMENT_TYPE_MAX << 1), 0},
      {kScript | kPopup, 0},
      {0, kDocument | (proto::ACTIVATION_TYPE_MAX << 1)},
  };

  for (const auto& rule : kRules) {
    UrlRuleBuilder builder(UrlPattern("example.com"));
    builder.rule().set_element_types(rule.element_types);
    builder.rule().set_activation_types(rule.activation_types);
    if (rule.activation_types)
      builder.rule().set_semantics(proto::RULE_SEMANTICS_WHITELIST);
    EXPECT_TRUE(indexer_.AddUrlRule(builder.rule()));
  }
  Finish();

  EXPECT_FALSE(ShouldAllow("http://example.com/", nullptr, kImage));
  EXPECT_FALSE(ShouldAllow("http://example.com/", nullptr, kScript));
  EXPECT_TRUE(ShouldAllow("http://example.com/"));

  EXPECT_TRUE(ShouldDeactivate("http://example.com", nullptr, kDocument));
  EXPECT_FALSE(ShouldDeactivate("http://example.com", nullptr, kGenericBlock));
}

}  // namespace subresource_filter
