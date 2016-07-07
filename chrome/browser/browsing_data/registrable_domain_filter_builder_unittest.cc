// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/registrable_domain_filter_builder.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "net/cookies/canonical_cookie.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
const char kGoogleDomain[] = "google.com";
// sp.nom.br is an eTLD, so this is a regular valid registrable domain, just
// like google.com.
const char kLongETLDDomain[] = "website.sp.nom.br";
// This domain will also not be found in registries, and since it has only
// one component, it will not be recognized as a valid registrable domain.
const char kInternalHostname[] = "fileserver";
// This domain will not be found in registries. It will be assumed that
// it belongs to an unknown registry, and since it has two components,
// they will be treated as the second level domain and TLD. Most importantly,
// it will NOT be treated as a subdomain of "fileserver".
const char kUnknownRegistryDomain[] = "second-level-domain.fileserver";
// IP addresses are supported.
const char kIPAddress[] = "192.168.1.1";

struct TestCase {
  std::string url;
  bool should_match;
};

void RunTestCase(TestCase test_case,
                 const base::Callback<bool(const GURL&)>& filter) {
  GURL url(test_case.url);
  EXPECT_TRUE(url.is_valid()) << test_case.url << " is not valid.";
  EXPECT_EQ(test_case.should_match, filter.Run(GURL(test_case.url)))
      << test_case.url;
}

void RunTestCase(
    TestCase test_case,
    const base::Callback<bool(const ContentSettingsPattern&)>& filter) {
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString(test_case.url);
  EXPECT_TRUE(pattern.IsValid()) << test_case.url << " is not valid.";
  EXPECT_EQ(test_case.should_match, filter.Run(pattern)) << pattern.ToString();
}

void RunTestCase(
    TestCase test_case,
    const base::Callback<bool(const net::CanonicalCookie&)>& filter) {
  // Test with regular cookie, http only, domain, and secure.
  std::string cookie_line = "A=2";
  GURL test_url(test_case.url);
  EXPECT_TRUE(test_url.is_valid()) << test_case.url;
  std::unique_ptr<net::CanonicalCookie> cookie = net::CanonicalCookie::Create(
      test_url, cookie_line, base::Time::Now(), net::CookieOptions());
  EXPECT_TRUE(cookie) << cookie_line << " from " << test_case.url
                      << " is not a valid cookie";
  if (cookie)
    EXPECT_EQ(test_case.should_match, filter.Run(*cookie))
        << cookie->DebugString();

  cookie_line = std::string("A=2;domain=") + test_url.host();
  cookie = net::CanonicalCookie::Create(
      test_url, cookie_line, base::Time::Now(), net::CookieOptions());
  if (cookie)
    EXPECT_EQ(test_case.should_match, filter.Run(*cookie))
        << cookie->DebugString();

  cookie_line = std::string("A=2; HttpOnly;") + test_url.host();
  cookie = net::CanonicalCookie::Create(
      test_url, cookie_line, base::Time::Now(), net::CookieOptions());
  if (cookie)
    EXPECT_EQ(test_case.should_match, filter.Run(*cookie))
        << cookie->DebugString();

  cookie_line = std::string("A=2; HttpOnly; Secure;") + test_url.host();
  cookie = net::CanonicalCookie::Create(
      test_url, cookie_line, base::Time::Now(), net::CookieOptions());
  if (cookie)
    EXPECT_EQ(test_case.should_match, filter.Run(*cookie))
        << cookie->DebugString();
}

void RunTestCase(
    TestCase test_case,
    const base::Callback<bool(const std::string&)>& filter) {
  std::string channel_id_server_id = test_case.url;
  EXPECT_EQ(test_case.should_match, filter.Run(channel_id_server_id))
      << channel_id_server_id << " should "
      << (test_case.should_match ? "" : "NOT ") << "be matched by the filter.";
}

}  // namespace

TEST(RegistrableDomainFilterBuilderTest, Noop) {
  // An no-op filter matches everything.
  base::Callback<bool(const GURL&)> filter =
      RegistrableDomainFilterBuilder::BuildNoopFilter();

  TestCase test_cases[] = {
      {"https://www.google.com", true},
      {"https://www.chrome.com", true},
      {"http://www.google.com/foo/bar", true},
      {"https://website.sp.nom.br", true},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(RegistrableDomainFilterBuilderTest, GURLWhitelist) {
  RegistrableDomainFilterBuilder builder(
      RegistrableDomainFilterBuilder::WHITELIST);
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));
  base::Callback<bool(const GURL&)> filter = builder.BuildGeneralFilter();

  TestCase test_cases[] = {
      // We match any URL on the specified domains.
      {"http://www.google.com/foo/bar", true},
      {"https://www.sub.google.com/foo/bar", true},
      {"https://sub.google.com", true},
      {"http://www.sub.google.com:8000/foo/bar", true},
      {"https://website.sp.nom.br", true},
      {"https://www.website.sp.nom.br", true},
      {"http://192.168.1.1", true},
      {"http://192.168.1.1:80", true},

      // Internal hostnames do not have subdomains.
      {"http://fileserver", true },
      {"http://fileserver/foo/bar", true },
      {"http://website.fileserver/foo/bar", false },

      // This is a valid registrable domain with the TLD "fileserver", which
      // is unrelated to the internal hostname "fileserver".
      {"http://second-level-domain.fileserver/foo", true},
      {"http://www.second-level-domain.fileserver/index.html", true},

      // Different domains.
      {"https://www.youtube.com", false},
      {"https://www.google.net", false},
      {"http://192.168.1.2", false},

      // Check both a bare eTLD.
      {"https://sp.nom.br", false},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(RegistrableDomainFilterBuilderTest, GURLBlacklist) {
  RegistrableDomainFilterBuilder builder(
      RegistrableDomainFilterBuilder::BLACKLIST);
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));
  base::Callback<bool(const GURL&)> filter = builder.BuildGeneralFilter();

  TestCase test_cases[] = {
      // We match any URL that are not on the specified domains.
      {"http://www.google.com/foo/bar", false},
      {"https://www.sub.google.com/foo/bar", false},
      {"https://sub.google.com", false},
      {"http://www.sub.google.com:8000/foo/bar", false},
      {"https://website.sp.nom.br", false},
      {"https://www.website.sp.nom.br", false},
      {"http://192.168.1.1", false},
      {"http://192.168.1.1:80", false},

      // Internal hostnames do not have subdomains.
      {"http://fileserver", false },
      {"http://fileserver/foo/bar", false },
      {"http://website.fileserver/foo/bar", true },

      // This is a valid registrable domain with the TLD "fileserver", which
      // is unrelated to the internal hostname "fileserver".
      {"http://second-level-domain.fileserver/foo", false},
      {"http://www.second-level-domain.fileserver/index.html", false},

      // Different domains.
      {"https://www.youtube.com", true},
      {"https://www.google.net", true},
      {"http://192.168.1.2", true},

      // Check our bare eTLD.
      {"https://sp.nom.br", true},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(RegistrableDomainFilterBuilderTest, WhitelistContentSettings) {
  RegistrableDomainFilterBuilder builder(
      RegistrableDomainFilterBuilder::WHITELIST);
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));
  base::Callback<bool(const ContentSettingsPattern&)> filter =
      builder.BuildWebsiteSettingsPatternMatchesFilter();

  TestCase test_cases[] = {
      // Whitelist matches any patterns that include the whitelisted
      // registerable domains.
      {"https://www.google.com", true},
      {"http://www.google.com", true},
      {"http://www.google.com/index.html", true},
      {"http://www.google.com/foo/bar", true},
      {"www.sub.google.com/bar", true},
      {"http://www.sub.google.com:8000/foo/bar", true},
      {"https://[*.]google.com", true},
      {"https://[*.]google.com:443", true},
      {"[*.]google.com", true},
      {"[*.]google.com/foo/bar", true},
      {"[*.]google.com:80", true},
      {"www.google.com/?q=test", true},
      {"192.168.1.1", true},
      {"192.168.1.1:80", true},
      // We check that we treat the eTLDs correctly.
      {"https://website.sp.nom.br", true},
      {"https://www.website.sp.nom.br", true},

      // Different eTLDs, and a bare eTLD.
      {"[*.]google", false},
      {"[*.]google.net", false},
      {"https://[*.]google.org", false},
      {"https://[*.]foo.bar.com", false},
      {"https://sp.nom.br", false},
      {"http://192.168.1.2", false},

      // Internal hostnames do not have subdomains.
      {"http://fileserver", true },
      {"http://[*.]fileserver", false },
      {"http://website.fileserver", false },

      // This is a valid registrable domain with the TLD "fileserver", which
      // is unrelated to the internal hostname "fileserver".
      {"http://second-level-domain.fileserver", true},
      {"http://[*.]second-level-domain.fileserver", true},
      {"http://www.second-level-domain.fileserver", true},
      {"http://[*.]www.second-level-domain.fileserver", true},

      // These patterns are more general than our registerable domain filter,
      // as they apply to more sites. So we don't match them. The content
      // settings categories that we'll be seeing from browsing_data_remover
      // aren't going to have these, but I test for it anyways.
      {"*", false},
      {"*:80", false}
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(RegistrableDomainFilterBuilderTest, BlacklistContentSettings) {
  RegistrableDomainFilterBuilder builder(
      RegistrableDomainFilterBuilder::BLACKLIST);
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));
  base::Callback<bool(const ContentSettingsPattern&)> filter =
      builder.BuildWebsiteSettingsPatternMatchesFilter();

  TestCase test_cases[] = {
      // Blacklist matches any patterns that isn't in the blacklist
      // registerable domains.
      {"https://www.google.com", false},
      {"http://www.google.com", false},
      {"http://www.google.com/index.html", false},
      {"http://www.google.com/foo/bar", false},
      {"www.sub.google.com/bar", false},
      {"http://www.sub.google.com:8000/foo/bar", false},
      {"https://[*.]google.com", false},
      {"https://[*.]google.com:443", false},
      {"[*.]google.com", false},
      {"[*.]google.com/foo/bar", false},
      {"[*.]google.com:80", false},
      {"www.google.com/?q=test", false},
      {"192.168.1.1", false},
      {"192.168.1.1:80", false},
      // We check that we treat the eTLDs correctly.
      {"https://website.sp.nom.br", false},
      {"https://www.website.sp.nom.br", false},

      // Different eTLDs, and a bare eTLD.
      {"[*.]google", true},
      {"[*.]google.net", true},
      {"https://[*.]google.org", true},
      {"https://[*.]foo.bar.com", true},
      {"https://sp.nom.br", true},
      {"http://192.168.1.2", true},

      // Internal hostnames do not have subdomains.
      {"fileserver", false },
      {"http://[*.]fileserver", true },
      {"website.fileserver", true },

      // This is a valid registrable domain with the TLD "fileserver", which
      // is unrelated to the internal hostname "fileserver".
      {"http://second-level-domain.fileserver", false},
      {"http://[*.]second-level-domain.fileserver", false},
      {"http://www.second-level-domain.fileserver", false},
      {"http://[*.]www.second-level-domain.fileserver", false},

      // These patterns are more general than our registerable domain filter,
      // as they apply to more sites. So we don't match them. The content
      // settings categories that we'll be seeing from browsing_data_remover
      // aren't going to have these, but I test for it anyways.
      {"*", true},
      {"*:80", true}
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(RegistrableDomainFilterBuilderTest, MatchesCookiesWhitelist) {
  RegistrableDomainFilterBuilder builder(
      RegistrableDomainFilterBuilder::WHITELIST);
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));
  base::Callback<bool(const net::CanonicalCookie&)> filter =
      builder.BuildCookieFilter();

  TestCase test_cases[] = {
      // Any cookie with the same registerable domain as the origins is matched.
      {"https://www.google.com", true},
      {"http://www.google.com", true},
      {"http://www.google.com:300", true},
      {"https://mail.google.com", true},
      {"http://mail.google.com", true},
      {"http://google.com", true},
      {"https://website.sp.nom.br", true},
      {"https://sub.website.sp.nom.br", true},
      {"http://192.168.1.1", true},
      {"http://192.168.1.1:10", true},

      // Different eTLDs.
      {"https://www.google.org", false},
      {"https://www.google.co.uk", false},

      // We treat eTLD+1 and bare eTLDs as different domains.
      {"https://www.sp.nom.br", false},
      {"https://sp.nom.br", false},

      // Different hosts in general.
      {"https://www.chrome.com", false},
      {"http://192.168.2.1", false},

      // Internal hostnames do not have subdomains.
      {"https://fileserver", true },
      {"http://fileserver/foo/bar", true },
      {"http://website.fileserver", false },

      // This is a valid registrable domain with the TLD "fileserver", which
      // is unrelated to the internal hostname "fileserver".
      {"http://second-level-domain.fileserver", true},
      {"https://subdomain.second-level-domain.fileserver", true},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(RegistrableDomainFilterBuilderTest, MatchesCookiesBlacklist) {
  RegistrableDomainFilterBuilder builder(
      RegistrableDomainFilterBuilder::BLACKLIST);
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));
  base::Callback<bool(const net::CanonicalCookie&)> filter =
      builder.BuildCookieFilter();

  TestCase test_cases[] = {
      // Any cookie that doesn't have the same registerable domain is matched.
      {"https://www.google.com", false},
      {"http://www.google.com", false},
      {"http://www.google.com:300", false},
      {"https://mail.google.com", false},
      {"http://mail.google.com", false},
      {"http://google.com", false},
      {"https://website.sp.nom.br", false},
      {"https://sub.website.sp.nom.br", false},
      {"http://192.168.1.1", false},
      {"http://192.168.1.1:10", false},

      // Different eTLDs.
      {"https://www.google.org", true},
      {"https://www.google.co.uk", true},

      // We treat eTLD+1 and bare eTLDs as different domains.
      {"https://www.sp.nom.br", true},
      {"https://sp.nom.br", true},

      // Different hosts in general.
      {"https://www.chrome.com", true},
      {"http://192.168.2.1", true},

      // Internal hostnames do not have subdomains.
      {"https://fileserver", false },
      {"http://fileserver/foo/bar", false },
      {"http://website.fileserver", true },

      // This is a valid registrable domain with the TLD "fileserver", which
      // is unrelated to the internal hostname "fileserver".
      {"http://second-level-domain.fileserver", false},
      {"https://subdomain.second-level-domain.fileserver", false},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(RegistrableDomainFilterBuilderTest, MatchesChannelIDsWhitelist) {
  RegistrableDomainFilterBuilder builder(
      RegistrableDomainFilterBuilder::WHITELIST);
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));
  base::Callback<bool(const std::string&)> filter =
      builder.BuildChannelIDFilter();

  TestCase test_cases[] = {
      // Channel ID server identifiers can be second level domains, ...
      {"google.com", true},
      {"website.sp.nom.br", true},
      {"second-level-domain.fileserver", true},

      // ... IP addresses, or internal hostnames.
      {"192.168.1.1", true},
      {"fileserver", true},

      // Channel IDs not in the whitelist are not matched.
      {"example.com", false},
      {"192.168.1.2", false},
      {"website.fileserver", false},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(RegistrableDomainFilterBuilderTest, MatchesChannelIDsBlacklist) {
  RegistrableDomainFilterBuilder builder(
      RegistrableDomainFilterBuilder::BLACKLIST);
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));
  base::Callback<bool(const std::string&)> filter =
      builder.BuildChannelIDFilter();

  TestCase test_cases[] = {
      // Channel ID server identifiers can be second level domains, ...
      {"google.com", false},
      {"website.sp.nom.br", false},
      {"second-level-domain.fileserver", false},

      // ...IP addresses, or internal hostnames.
      {"192.168.1.1", false},
      {"fileserver", false},

      // Channel IDs that are not blacklisted are matched.
      {"example.com", true},
      {"192.168.1.2", true},
      {"website.fileserver", true},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}
