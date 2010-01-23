// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/browser_prefs.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class BlacklistTest : public testing::Test {
 protected:
  virtual void SetUp() {
    FilePath source_path;
    PathService::Get(chrome::DIR_TEST_DATA, &source_path);
    source_path = source_path.AppendASCII("profiles")
        .AppendASCII("blacklist_prefs").AppendASCII("Preferences");

    prefs_.reset(new PrefService(source_path));
    Profile::RegisterUserPrefs(prefs_.get());
    browser::RegisterAllPrefs(prefs_.get(), prefs_.get());
  }

  scoped_ptr<PrefService> prefs_;
};

TEST_F(BlacklistTest, Generic) {
  Blacklist blacklist(prefs_.get());
  Blacklist::EntryList entries(blacklist.entries_begin(),
                               blacklist.entries_end());

  ASSERT_EQ(7U, entries.size());

  // All entries include global attributes.
  // NOTE: Silly bitwise-or with zero to workaround a Mac compiler bug.
  EXPECT_EQ(Blacklist::kBlockUnsecure|0, entries[0]->attributes());
  EXPECT_FALSE(entries[0]->is_exception());
  EXPECT_EQ("@poor-security-site.com", entries[0]->pattern());

  // NOTE: Silly bitwise-or with zero to workaround a Mac compiler bug.
  EXPECT_EQ(Blacklist::kBlockCookies|0, entries[1]->attributes());
  EXPECT_FALSE(entries[1]->is_exception());
  EXPECT_EQ("@.ad-serving-place.com", entries[1]->pattern());

  EXPECT_EQ(Blacklist::kDontSendUserAgent|Blacklist::kDontSendReferrer,
            entries[2]->attributes());
  EXPECT_FALSE(entries[2]->is_exception());
  EXPECT_EQ("www.site.com/anonymous/folder/@", entries[2]->pattern());

  // NOTE: Silly bitwise-or with zero to workaround a Mac compiler bug.
  EXPECT_EQ(Blacklist::kBlockAll|0, entries[3]->attributes());
  EXPECT_FALSE(entries[3]->is_exception());
  EXPECT_EQ("www.site.com/bad/url", entries[3]->pattern());

  // NOTE: Silly bitwise-or with zero to workaround a Mac compiler bug.
  EXPECT_EQ(Blacklist::kBlockAll|0, entries[4]->attributes());
  EXPECT_FALSE(entries[4]->is_exception());
  EXPECT_EQ("@/script?@", entries[4]->pattern());

  // NOTE: Silly bitwise-or with zero to workaround a Mac compiler bug.
  EXPECT_EQ(Blacklist::kBlockAll|0, entries[5]->attributes());
  EXPECT_FALSE(entries[5]->is_exception());
  EXPECT_EQ("@?badparam@", entries[5]->pattern());

  // NOTE: Silly bitwise-or with zero to workaround a Mac compiler bug.
  EXPECT_EQ(Blacklist::kBlockAll|0, entries[6]->attributes());
  EXPECT_TRUE(entries[6]->is_exception());
  EXPECT_EQ("www.site.com/bad/url/good", entries[6]->pattern());

  Blacklist::ProviderList providers(blacklist.providers_begin(),
                                    blacklist.providers_end());

  ASSERT_EQ(1U, providers.size());
  EXPECT_EQ("Sample", providers[0]->name());
  EXPECT_EQ("http://www.example.com", providers[0]->url());

  // No match for chrome, about or empty URLs.
  EXPECT_FALSE(blacklist.FindMatch(GURL()));
  EXPECT_FALSE(blacklist.FindMatch(GURL("chrome://new-tab")));
  EXPECT_FALSE(blacklist.FindMatch(GURL("about:blank")));

  // Expected rule matches.
  Blacklist::Match* match = blacklist.FindMatch(GURL("http://www.site.com/bad/url"));
  EXPECT_TRUE(match);
  if (match) {
    EXPECT_EQ(Blacklist::kBlockAll|0, match->attributes());
    EXPECT_EQ(1U, match->entries().size());
    delete match;
  }

  match = blacklist.FindMatch(GURL("http://www.site.com/anonymous"));
  EXPECT_FALSE(match);
  if (match)
    delete match;

  match = blacklist.FindMatch(GURL("http://www.site.com/anonymous/folder"));
  EXPECT_FALSE(match);
  if (match)
    delete match;

  match = blacklist.FindMatch(
      GURL("http://www.site.com/anonymous/folder/subfolder"));
  EXPECT_TRUE(match);
  if (match) {
    EXPECT_EQ(Blacklist::kDontSendUserAgent|Blacklist::kDontSendReferrer,
              match->attributes());
    EXPECT_EQ(1U, match->entries().size());
    delete match;
  }

  // No matches for URLs without query string
  match = blacklist.FindMatch(GURL("http://badparam.com/"));
  EXPECT_FALSE(match);
  if (match)
    delete match;

  match = blacklist.FindMatch(GURL("http://script.bad.org/"));
  EXPECT_FALSE(match);
  if (match)
    delete match;

  // Expected rule matches.
  match = blacklist.FindMatch(GURL("http://host.com/script?q=x"));
  EXPECT_TRUE(match);
  if (match) {
    EXPECT_EQ(Blacklist::kBlockAll, match->attributes());
    EXPECT_EQ(1U, match->entries().size());
    delete match;
  }

  match = blacklist.FindMatch(GURL("http://host.com/img?badparam=x"));
  EXPECT_TRUE(match);
  if (match) {
    EXPECT_EQ(Blacklist::kBlockAll, match->attributes());
    EXPECT_EQ(1U, match->entries().size());
    delete match;
  }

  // Whitelisting tests.
  match = blacklist.FindMatch(GURL("http://www.site.com/bad/url/good"));
  EXPECT_TRUE(match);
  if (match) {
    EXPECT_EQ(0U, match->attributes());
    EXPECT_EQ(1U, match->entries().size());
    delete match;
  }

  // StripCookies Test. Note that "\r\n" line terminators are used
  // because the underlying net util uniformizes those when stripping
  // headers.
  std::string header1("Host: www.example.com\r\n");
  std::string header2("Upgrade: TLS/1.0, HTTP/1.1\r\n"
                      "Connection: Upgrade\r\n");
  std::string header3("Date: Mon, 12 Mar 2001 19:20:33 GMT\r\n"
                      "Expires: Mon, 12 Mar 2001 19:20:33 GMT\r\n"
                      "Content-Type: text/html\r\n"
                      "Set-Cookie: B=460soc0taq8c1&b=2; "
                      "expires=Thu, 15 Apr 2010 20:00:00 GMT; path=/;\r\n");
  std::string header4("Date: Mon, 12 Mar 2001 19:20:33 GMT\r\n"
                      "Expires: Mon, 12 Mar 2001 19:20:33 GMT\r\n"
                      "Content-Type: text/html\r\n");

  EXPECT_TRUE(header1 == Blacklist::StripCookies(header1));
  EXPECT_TRUE(header2 == Blacklist::StripCookies(header2));
  EXPECT_TRUE(header4 == Blacklist::StripCookies(header3));

  // GetURLAsLookupString Test.
  std::string url_spec1("example.com/some/path");
  std::string url_spec2("example.com/script?param=1");

  EXPECT_TRUE(url_spec1 == Blacklist::GetURLAsLookupString(
              GURL("http://example.com/some/path")));
  EXPECT_TRUE(url_spec1 == Blacklist::GetURLAsLookupString(
              GURL("ftp://example.com/some/path")));
  EXPECT_TRUE(url_spec1 == Blacklist::GetURLAsLookupString(
              GURL("http://example.com:8080/some/path")));
  EXPECT_TRUE(url_spec1 == Blacklist::GetURLAsLookupString(
              GURL("http://user:login@example.com/some/path")));
  EXPECT_TRUE(url_spec2 == Blacklist::GetURLAsLookupString(
              GURL("http://example.com/script?param=1")));
}

TEST_F(BlacklistTest, PatternMatch) {
  // @ matches all but empty strings.
  EXPECT_TRUE(Blacklist::Matches("@", "foo.com"));
  EXPECT_TRUE(Blacklist::Matches("@", "path"));
  EXPECT_TRUE(Blacklist::Matches("@", "foo.com/path"));
  EXPECT_TRUE(Blacklist::Matches("@", "x"));
  EXPECT_FALSE(Blacklist::Matches("@", ""));
  EXPECT_FALSE(Blacklist::Matches("@", std::string()));

  // Prefix match.
  EXPECT_TRUE(Blacklist::Matches("prefix@", "prefix.com"));
  EXPECT_TRUE(Blacklist::Matches("prefix@", "prefix.com/path"));
  EXPECT_TRUE(Blacklist::Matches("prefix@", "prefix/path"));
  EXPECT_TRUE(Blacklist::Matches("prefix@", "prefix/prefix"));
  EXPECT_FALSE(Blacklist::Matches("prefix@", "prefix"));
  EXPECT_FALSE(Blacklist::Matches("prefix@", "Xprefix"));
  EXPECT_FALSE(Blacklist::Matches("prefix@", "Y.Xprefix"));
  EXPECT_FALSE(Blacklist::Matches("prefix@", "Y/Xprefix"));

  // Postfix match.
  EXPECT_TRUE(Blacklist::Matches("@postfix", "something.postfix"));
  EXPECT_TRUE(Blacklist::Matches("@postfix", "something/postfix"));
  EXPECT_TRUE(Blacklist::Matches("@postfix", "foo.com/something/postfix"));
  EXPECT_FALSE(Blacklist::Matches("@postfix", "postfix"));
  EXPECT_FALSE(Blacklist::Matches("@postfix", "postfixZ"));
  EXPECT_FALSE(Blacklist::Matches("@postfix", "postfixZ.Y"));

  // Infix matches.
  EXPECT_TRUE(Blacklist::Matches("@evil@", "www.evil.com"));
  EXPECT_TRUE(Blacklist::Matches("@evil@", "www.evil.com/whatever"));
  EXPECT_TRUE(Blacklist::Matches("@evil@", "www.whatever.com/evilpath"));
  EXPECT_TRUE(Blacklist::Matches("@evil@", "www.evil.whatever.com"));
  EXPECT_FALSE(Blacklist::Matches("@evil@", "evil"));
  EXPECT_FALSE(Blacklist::Matches("@evil@", "evil/"));
  EXPECT_FALSE(Blacklist::Matches("@evil@", "/evil"));

  // Outfix matches.
  EXPECT_TRUE(Blacklist::Matches("really@bad", "really/bad"));
  EXPECT_TRUE(Blacklist::Matches("really@bad", "really.com/bad"));
  EXPECT_TRUE(Blacklist::Matches("really@bad", "really.com/path/bad"));
  EXPECT_TRUE(Blacklist::Matches("really@bad", "really.evil.com/path/bad"));
  EXPECT_FALSE(Blacklist::Matches("really@bad", "really.bad.com"));
  EXPECT_FALSE(Blacklist::Matches("really@bad", "reallybad"));
  EXPECT_FALSE(Blacklist::Matches("really@bad", ".reallybad"));
  EXPECT_FALSE(Blacklist::Matches("really@bad", "reallybad."));
  EXPECT_FALSE(Blacklist::Matches("really@bad", "really.bad."));
  EXPECT_FALSE(Blacklist::Matches("really@bad", ".really.bad"));
}
