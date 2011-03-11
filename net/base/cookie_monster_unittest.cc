// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

#include <string>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "net/base/cookie_monster.h"
#include "net/base/cookie_monster_store_test.h" // For CookieStore Mock
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using base::Time;
using base::TimeDelta;

namespace {

class ParsedCookieTest : public testing::Test { };
class CookieMonsterTest : public testing::Test { };

// Helper for DeleteAllForHost test; repopulates CM with same layout
// each time.
const char* kTopLevelDomainPlus1 = "http://www.harvard.edu";
const char* kTopLevelDomainPlus2 = "http://www.math.harvard.edu";
const char* kTopLevelDomainPlus2Secure = "https://www.math.harvard.edu";
const char* kTopLevelDomainPlus3 =
    "http://www.bourbaki.math.harvard.edu";
const char* kOtherDomain = "http://www.mit.edu";

void PopulateCmForDeleteAllForHost(scoped_refptr<net::CookieMonster> cm) {
  GURL url_top_level_domain_plus_1(kTopLevelDomainPlus1);
  GURL url_top_level_domain_plus_2(kTopLevelDomainPlus2);
  GURL url_top_level_domain_plus_2_secure(kTopLevelDomainPlus2Secure);
  GURL url_top_level_domain_plus_3(kTopLevelDomainPlus3);
  GURL url_other(kOtherDomain);

  cm->DeleteAll(true);

  // Static population for probe:
  //    * Three levels of domain cookie (.b.a, .c.b.a, .d.c.b.a)
  //    * Three levels of host cookie (w.b.a, w.c.b.a, w.d.c.b.a)
  //    * http_only cookie (w.c.b.a)
  //    * Two secure cookies (.c.b.a, w.c.b.a)
  //    * Two domain path cookies (.c.b.a/dir1, .c.b.a/dir1/dir2)
  //    * Two host path cookies (w.c.b.a/dir1, w.c.b.a/dir1/dir2)

  // Domain cookies
  EXPECT_TRUE(cm->SetCookieWithDetails(url_top_level_domain_plus_1,
                                       "dom_1", "X", ".harvard.edu", "/",
                                       base::Time(), false, false));
  EXPECT_TRUE(cm->SetCookieWithDetails(url_top_level_domain_plus_2,
                                       "dom_2", "X", ".math.harvard.edu", "/",
                                       base::Time(), false, false));
  EXPECT_TRUE(cm->SetCookieWithDetails(url_top_level_domain_plus_3,
                                       "dom_3", "X",
                                       ".bourbaki.math.harvard.edu", "/",
                                       base::Time(), false, false));

  // Host cookies
  EXPECT_TRUE(cm->SetCookieWithDetails(url_top_level_domain_plus_1,
                                       "host_1", "X", "", "/",
                                       base::Time(), false, false));
  EXPECT_TRUE(cm->SetCookieWithDetails(url_top_level_domain_plus_2,
                                       "host_2", "X", "", "/",
                                       base::Time(), false, false));
  EXPECT_TRUE(cm->SetCookieWithDetails(url_top_level_domain_plus_3,
                                       "host_3", "X", "", "/",
                                       base::Time(), false, false));

  // Http_only cookie
  EXPECT_TRUE(cm->SetCookieWithDetails(url_top_level_domain_plus_2,
                                       "httpo_check", "X", "", "/",
                                       base::Time(), false, true));

  // Secure cookies
  EXPECT_TRUE(cm->SetCookieWithDetails(url_top_level_domain_plus_2_secure,
                                       "sec_dom", "X", ".math.harvard.edu",
                                       "/", base::Time(), true, false));
  EXPECT_TRUE(cm->SetCookieWithDetails(url_top_level_domain_plus_2_secure,
                                       "sec_host", "X", "", "/",
                                       base::Time(), true, false));

  // Domain path cookies
  EXPECT_TRUE(cm->SetCookieWithDetails(url_top_level_domain_plus_2,
                                       "dom_path_1", "X",
                                       ".math.harvard.edu", "/dir1",
                                       base::Time(), false, false));
  EXPECT_TRUE(cm->SetCookieWithDetails(url_top_level_domain_plus_2,
                                       "dom_path_2", "X",
                                       ".math.harvard.edu", "/dir1/dir2",
                                       base::Time(), false, false));

  // Host path cookies
  EXPECT_TRUE(cm->SetCookieWithDetails(url_top_level_domain_plus_2,
                                       "host_path_1", "X",
                                       "", "/dir1",
                                       base::Time(), false, false));
  EXPECT_TRUE(cm->SetCookieWithDetails(url_top_level_domain_plus_2,
                                       "host_path_2", "X",
                                       "", "/dir1/dir2",
                                       base::Time(), false, false));

  EXPECT_EQ(13U, cm->GetAllCookies().size());
}

}  // namespace

TEST(ParsedCookieTest, TestBasic) {
  net::CookieMonster::ParsedCookie pc("a=b");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_FALSE(pc.IsSecure());
  EXPECT_EQ("a", pc.Name());
  EXPECT_EQ("b", pc.Value());
}

TEST(ParsedCookieTest, TestQuoted) {
  // These are some quoting cases which the major browsers all
  // handle differently.  I've tested Internet Explorer 6, Opera 9.6,
  // Firefox 3, and Safari Windows 3.2.1.  We originally tried to match
  // Firefox closely, however we now match Internet Explorer and Safari.
  const char* values[] = {
    // Trailing whitespace after a quoted value.  The whitespace after
    // the quote is stripped in all browsers.
    "\"zzz \"  ",              "\"zzz \"",
    // Handling a quoted value with a ';', like FOO="zz;pp"  ;
    // IE and Safari: "zz;
    // Firefox and Opera: "zz;pp"
    "\"zz;pp\" ;",             "\"zz",
    // Handling a value with multiple quoted parts, like FOO="zzz "   "ppp" ;
    // IE and Safari: "zzz "   "ppp";
    // Firefox: "zzz ";
    // Opera: <rejects cookie>
    "\"zzz \"   \"ppp\" ",     "\"zzz \"   \"ppp\"",
    // A quote in a value that didn't start quoted.  like FOO=A"B ;
    // IE, Safari, and Firefox: A"B;
    // Opera: <rejects cookie>
    "A\"B",                    "A\"B",
  };

  for (size_t i = 0; i < arraysize(values); i += 2) {
    std::string input(values[i]);
    std::string expected(values[i + 1]);

    net::CookieMonster::ParsedCookie pc(
        "aBc=" + input + " ; path=\"/\"  ; httponly ");
    EXPECT_TRUE(pc.IsValid());
    EXPECT_FALSE(pc.IsSecure());
    EXPECT_TRUE(pc.IsHttpOnly());
    EXPECT_TRUE(pc.HasPath());
    EXPECT_EQ("aBc", pc.Name());
    EXPECT_EQ(expected, pc.Value());

    // If a path was quoted, the path attribute keeps the quotes.  This will
    // make the cookie effectively useless, but path parameters aren't supposed
    // to be quoted.  Bug 1261605.
    EXPECT_EQ("\"/\"", pc.Path());
  }
}

TEST(ParsedCookieTest, TestNameless) {
  net::CookieMonster::ParsedCookie pc("BLAHHH; path=/; secure;");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ("", pc.Name());
  EXPECT_EQ("BLAHHH", pc.Value());
}

TEST(ParsedCookieTest, TestAttributeCase) {
  net::CookieMonster::ParsedCookie pc("BLAHHH; Path=/; sECuRe; httpONLY");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.IsHttpOnly());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ("", pc.Name());
  EXPECT_EQ("BLAHHH", pc.Value());
  EXPECT_EQ(3U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, TestDoubleQuotedNameless) {
  net::CookieMonster::ParsedCookie pc("\"BLA\\\"HHH\"; path=/; secure;");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ("", pc.Name());
  EXPECT_EQ("\"BLA\\\"HHH\"", pc.Value());
  EXPECT_EQ(2U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, QuoteOffTheEnd) {
  net::CookieMonster::ParsedCookie pc("a=\"B");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("a", pc.Name());
  EXPECT_EQ("\"B", pc.Value());
  EXPECT_EQ(0U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, MissingName) {
  net::CookieMonster::ParsedCookie pc("=ABC");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("", pc.Name());
  EXPECT_EQ("ABC", pc.Value());
  EXPECT_EQ(0U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, MissingValue) {
  net::CookieMonster::ParsedCookie pc("ABC=;  path = /wee");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("ABC", pc.Name());
  EXPECT_EQ("", pc.Value());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/wee", pc.Path());
  EXPECT_EQ(1U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, Whitespace) {
  net::CookieMonster::ParsedCookie pc("  A  = BC  ;secure;;;   httponly");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("A", pc.Name());
  EXPECT_EQ("BC", pc.Value());
  EXPECT_FALSE(pc.HasPath());
  EXPECT_FALSE(pc.HasDomain());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.IsHttpOnly());
  // We parse anything between ; as attributes, so we end up with two
  // attributes with an empty string name and value.
  EXPECT_EQ(4U, pc.NumberOfAttributes());
}
TEST(ParsedCookieTest, MultipleEquals) {
  net::CookieMonster::ParsedCookie pc("  A=== BC  ;secure;;;   httponly");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("A", pc.Name());
  EXPECT_EQ("== BC", pc.Value());
  EXPECT_FALSE(pc.HasPath());
  EXPECT_FALSE(pc.HasDomain());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.IsHttpOnly());
  EXPECT_EQ(4U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, QuotedTrailingWhitespace) {
  net::CookieMonster::ParsedCookie pc("ANCUUID=\"zohNumRKgI0oxyhSsV3Z7D\"  ; "
                                      "expires=Sun, 18-Apr-2027 21:06:29 GMT ; "
                                      "path=/  ;  ");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("ANCUUID", pc.Name());
  // Stripping whitespace after the quotes matches all other major browsers.
  EXPECT_EQ("\"zohNumRKgI0oxyhSsV3Z7D\"", pc.Value());
  EXPECT_TRUE(pc.HasExpires());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ(2U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, TrailingWhitespace) {
  net::CookieMonster::ParsedCookie pc("ANCUUID=zohNumRKgI0oxyhSsV3Z7D  ; "
                                      "expires=Sun, 18-Apr-2027 21:06:29 GMT ; "
                                      "path=/  ;  ");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("ANCUUID", pc.Name());
  EXPECT_EQ("zohNumRKgI0oxyhSsV3Z7D", pc.Value());
  EXPECT_TRUE(pc.HasExpires());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ(2U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, TooManyPairs) {
  std::string blankpairs;
  blankpairs.resize(net::CookieMonster::ParsedCookie::kMaxPairs - 1, ';');

  net::CookieMonster::ParsedCookie pc1(blankpairs + "secure");
  EXPECT_TRUE(pc1.IsValid());
  EXPECT_TRUE(pc1.IsSecure());

  net::CookieMonster::ParsedCookie pc2(blankpairs + ";secure");
  EXPECT_TRUE(pc2.IsValid());
  EXPECT_FALSE(pc2.IsSecure());
}

// TODO some better test cases for invalid cookies.
TEST(ParsedCookieTest, InvalidWhitespace) {
  net::CookieMonster::ParsedCookie pc("    ");
  EXPECT_FALSE(pc.IsValid());
}

TEST(ParsedCookieTest, InvalidTooLong) {
  std::string maxstr;
  maxstr.resize(net::CookieMonster::ParsedCookie::kMaxCookieSize, 'a');

  net::CookieMonster::ParsedCookie pc1(maxstr);
  EXPECT_TRUE(pc1.IsValid());

  net::CookieMonster::ParsedCookie pc2(maxstr + "A");
  EXPECT_FALSE(pc2.IsValid());
}

TEST(ParsedCookieTest, InvalidEmpty) {
  net::CookieMonster::ParsedCookie pc("");
  EXPECT_FALSE(pc.IsValid());
}

TEST(ParsedCookieTest, EmbeddedTerminator) {
  net::CookieMonster::ParsedCookie pc1("AAA=BB\0ZYX");
  net::CookieMonster::ParsedCookie pc2("AAA=BB\rZYX");
  net::CookieMonster::ParsedCookie pc3("AAA=BB\nZYX");
  EXPECT_TRUE(pc1.IsValid());
  EXPECT_EQ("AAA", pc1.Name());
  EXPECT_EQ("BB", pc1.Value());
  EXPECT_TRUE(pc2.IsValid());
  EXPECT_EQ("AAA", pc2.Name());
  EXPECT_EQ("BB", pc2.Value());
  EXPECT_TRUE(pc3.IsValid());
  EXPECT_EQ("AAA", pc3.Name());
  EXPECT_EQ("BB", pc3.Value());
}

TEST(ParsedCookieTest, ParseTokensAndValues) {
  EXPECT_EQ("hello",
            net::CookieMonster::ParsedCookie::ParseTokenString(
                "hello\nworld"));
  EXPECT_EQ("fs!!@",
            net::CookieMonster::ParsedCookie::ParseTokenString(
                "fs!!@;helloworld"));
  EXPECT_EQ("hello world\tgood",
            net::CookieMonster::ParsedCookie::ParseTokenString(
                "hello world\tgood\rbye"));
  EXPECT_EQ("A",
            net::CookieMonster::ParsedCookie::ParseTokenString(
                "A=B=C;D=E"));
  EXPECT_EQ("hello",
            net::CookieMonster::ParsedCookie::ParseValueString(
                "hello\nworld"));
  EXPECT_EQ("fs!!@",
            net::CookieMonster::ParsedCookie::ParseValueString(
                "fs!!@;helloworld"));
  EXPECT_EQ("hello world\tgood",
            net::CookieMonster::ParsedCookie::ParseValueString(
                "hello world\tgood\rbye"));
  EXPECT_EQ("A=B=C",
            net::CookieMonster::ParsedCookie::ParseValueString(
                "A=B=C;D=E"));
}

static const char kUrlGoogle[] = "http://www.google.izzle";
static const char kUrlGoogleSpecific[] = "http://www.gmail.google.izzle";
static const char kUrlGoogleSecure[] = "https://www.google.izzle";
static const char kUrlFtp[] = "ftp://ftp.google.izzle/";
static const char kValidCookieLine[] = "A=B; path=/";
static const char kValidDomainCookieLine[] = "A=B; path=/; domain=google.izzle";

TEST(CookieMonsterTest, DomainTest) {
  GURL url_google(kUrlGoogle);

  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(store, NULL));
  EXPECT_TRUE(cm->SetCookie(url_google, "A=B"));
  EXPECT_EQ("A=B", cm->GetCookies(url_google));
  EXPECT_TRUE(cm->SetCookie(url_google, "C=D; domain=.google.izzle"));
  EXPECT_EQ("A=B; C=D", cm->GetCookies(url_google));

  // Verify that A=B was set as a host cookie rather than a domain
  // cookie -- should not be accessible from a sub sub-domain.
  EXPECT_EQ("C=D", cm->GetCookies(GURL("http://foo.www.google.izzle")));

  // Test and make sure we find domain cookies on the same domain.
  EXPECT_TRUE(cm->SetCookie(url_google, "E=F; domain=.www.google.izzle"));
  EXPECT_EQ("A=B; C=D; E=F", cm->GetCookies(url_google));

  // Test setting a domain= that doesn't start w/ a dot, should
  // treat it as a domain cookie, as if there was a pre-pended dot.
  EXPECT_TRUE(cm->SetCookie(url_google, "G=H; domain=www.google.izzle"));
  EXPECT_EQ("A=B; C=D; E=F; G=H", cm->GetCookies(url_google));

  // Test domain enforcement, should fail on a sub-domain or something too deep.
  EXPECT_FALSE(cm->SetCookie(url_google, "I=J; domain=.izzle"));
  EXPECT_EQ("", cm->GetCookies(GURL("http://a.izzle")));
  EXPECT_FALSE(cm->SetCookie(url_google, "K=L; domain=.bla.www.google.izzle"));
  EXPECT_EQ("C=D; E=F; G=H",
            cm->GetCookies(GURL("http://bla.www.google.izzle")));
  EXPECT_EQ("A=B; C=D; E=F; G=H", cm->GetCookies(url_google));

  // Nothing was persisted to the backing store.
  EXPECT_EQ(0u, store->commands().size());
}

// FireFox recognizes domains containing trailing periods as valid.
// IE and Safari do not. Assert the expected policy here.
TEST(CookieMonsterTest, DomainWithTrailingDotTest) {
  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(store, NULL));
  GURL url_google("http://www.google.com");

  EXPECT_FALSE(cm->SetCookie(url_google, "a=1; domain=.www.google.com."));
  EXPECT_FALSE(cm->SetCookie(url_google, "b=2; domain=.www.google.com.."));
  EXPECT_EQ("", cm->GetCookies(url_google));

  // Nothing was persisted to the backing store.
  EXPECT_EQ(0u, store->commands().size());
}

// Test that cookies can bet set on higher level domains.
// http://b/issue?id=896491
TEST(CookieMonsterTest, ValidSubdomainTest) {
  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(store, NULL));
  GURL url_abcd("http://a.b.c.d.com");
  GURL url_bcd("http://b.c.d.com");
  GURL url_cd("http://c.d.com");
  GURL url_d("http://d.com");

  EXPECT_TRUE(cm->SetCookie(url_abcd, "a=1; domain=.a.b.c.d.com"));
  EXPECT_TRUE(cm->SetCookie(url_abcd, "b=2; domain=.b.c.d.com"));
  EXPECT_TRUE(cm->SetCookie(url_abcd, "c=3; domain=.c.d.com"));
  EXPECT_TRUE(cm->SetCookie(url_abcd, "d=4; domain=.d.com"));

  EXPECT_EQ("a=1; b=2; c=3; d=4", cm->GetCookies(url_abcd));
  EXPECT_EQ("b=2; c=3; d=4", cm->GetCookies(url_bcd));
  EXPECT_EQ("c=3; d=4", cm->GetCookies(url_cd));
  EXPECT_EQ("d=4", cm->GetCookies(url_d));

  // Check that the same cookie can exist on different sub-domains.
  EXPECT_TRUE(cm->SetCookie(url_bcd, "X=bcd; domain=.b.c.d.com"));
  EXPECT_TRUE(cm->SetCookie(url_bcd, "X=cd; domain=.c.d.com"));
  EXPECT_EQ("b=2; c=3; d=4; X=bcd; X=cd", cm->GetCookies(url_bcd));
  EXPECT_EQ("c=3; d=4; X=cd", cm->GetCookies(url_cd));

  // Nothing was persisted to the backing store.
  EXPECT_EQ(0u, store->commands().size());
}

// Test that setting a cookie which specifies an invalid domain has
// no side-effect. An invalid domain in this context is one which does
// not match the originating domain.
// http://b/issue?id=896472
TEST(CookieMonsterTest, InvalidDomainTest) {
  {
    scoped_refptr<MockPersistentCookieStore> store(
        new MockPersistentCookieStore);

    scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(store, NULL));
    GURL url_foobar("http://foo.bar.com");

    // More specific sub-domain than allowed.
    EXPECT_FALSE(cm->SetCookie(url_foobar, "a=1; domain=.yo.foo.bar.com"));

    EXPECT_FALSE(cm->SetCookie(url_foobar, "b=2; domain=.foo.com"));
    EXPECT_FALSE(cm->SetCookie(url_foobar, "c=3; domain=.bar.foo.com"));

    // Different TLD, but the rest is a substring.
    EXPECT_FALSE(cm->SetCookie(url_foobar, "d=4; domain=.foo.bar.com.net"));

    // A substring that isn't really a parent domain.
    EXPECT_FALSE(cm->SetCookie(url_foobar, "e=5; domain=ar.com"));

    // Completely invalid domains:
    EXPECT_FALSE(cm->SetCookie(url_foobar, "f=6; domain=."));
    EXPECT_FALSE(cm->SetCookie(url_foobar, "g=7; domain=/"));
    EXPECT_FALSE(cm->SetCookie(url_foobar, "h=8; domain=http://foo.bar.com"));
    EXPECT_FALSE(cm->SetCookie(url_foobar, "i=9; domain=..foo.bar.com"));
    EXPECT_FALSE(cm->SetCookie(url_foobar, "j=10; domain=..bar.com"));

    // Make sure there isn't something quirky in the domain canonicalization
    // that supports full URL semantics.
    EXPECT_FALSE(cm->SetCookie(url_foobar, "k=11; domain=.foo.bar.com?blah"));
    EXPECT_FALSE(cm->SetCookie(url_foobar, "l=12; domain=.foo.bar.com/blah"));
    EXPECT_FALSE(cm->SetCookie(url_foobar, "m=13; domain=.foo.bar.com:80"));
    EXPECT_FALSE(cm->SetCookie(url_foobar, "n=14; domain=.foo.bar.com:"));
    EXPECT_FALSE(cm->SetCookie(url_foobar, "o=15; domain=.foo.bar.com#sup"));

    EXPECT_EQ("", cm->GetCookies(url_foobar));

    // Nothing was persisted to the backing store.
    EXPECT_EQ(0u, store->commands().size());
  }

  {
    // Make sure the cookie code hasn't gotten its subdomain string handling
    // reversed, missed a suffix check, etc.  It's important here that the two
    // hosts below have the same domain + registry.
    scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
    GURL url_foocom("http://foo.com.com");
    EXPECT_FALSE(cm->SetCookie(url_foocom, "a=1; domain=.foo.com.com.com"));
    EXPECT_EQ("", cm->GetCookies(url_foocom));
  }
}

// Test the behavior of omitting dot prefix from domain, should
// function the same as FireFox.
// http://b/issue?id=889898
TEST(CookieMonsterTest, DomainWithoutLeadingDotTest) {
  {  // The omission of dot results in setting a domain cookie.
    scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
    GURL url_hosted("http://manage.hosted.filefront.com");
    GURL url_filefront("http://www.filefront.com");
    EXPECT_TRUE(cm->SetCookie(url_hosted, "sawAd=1; domain=filefront.com"));
    EXPECT_EQ("sawAd=1", cm->GetCookies(url_hosted));
    EXPECT_EQ("sawAd=1", cm->GetCookies(url_filefront));
  }

  {  // Even when the domains match exactly, don't consider it host cookie.
    scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
    GURL url("http://www.google.com");
    EXPECT_TRUE(cm->SetCookie(url, "a=1; domain=www.google.com"));
    EXPECT_EQ("a=1", cm->GetCookies(url));
    EXPECT_EQ("a=1", cm->GetCookies(GURL("http://sub.www.google.com")));
    EXPECT_EQ("", cm->GetCookies(GURL("http://something-else.com")));
  }
}

// Test that the domain specified in cookie string is treated case-insensitive
// http://b/issue?id=896475.
TEST(CookieMonsterTest, CaseInsensitiveDomainTest) {
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
  GURL url_google("http://www.google.com");
  EXPECT_TRUE(cm->SetCookie(url_google, "a=1; domain=.GOOGLE.COM"));
  EXPECT_TRUE(cm->SetCookie(url_google, "b=2; domain=.wWw.gOOgLE.coM"));
  EXPECT_EQ("a=1; b=2", cm->GetCookies(url_google));
}

TEST(CookieMonsterTest, TestIpAddress) {
  GURL url_ip("http://1.2.3.4/weee");
  {
    scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
    EXPECT_TRUE(cm->SetCookie(url_ip, kValidCookieLine));
    EXPECT_EQ("A=B", cm->GetCookies(url_ip));
  }

  {  // IP addresses should not be able to set domain cookies.
    scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
    EXPECT_FALSE(cm->SetCookie(url_ip, "b=2; domain=.1.2.3.4"));
    EXPECT_FALSE(cm->SetCookie(url_ip, "c=3; domain=.3.4"));
    EXPECT_EQ("", cm->GetCookies(url_ip));
    // It should be allowed to set a cookie if domain= matches the IP address
    // exactly.  This matches IE/Firefox, even though it seems a bit wrong.
    EXPECT_FALSE(cm->SetCookie(url_ip, "b=2; domain=1.2.3.3"));
    EXPECT_EQ("", cm->GetCookies(url_ip));
    EXPECT_TRUE(cm->SetCookie(url_ip, "b=2; domain=1.2.3.4"));
    EXPECT_EQ("b=2", cm->GetCookies(url_ip));
  }
}

// Test host cookies, and setting of cookies on TLD.
TEST(CookieMonsterTest, TestNonDottedAndTLD) {
  {
    scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
    GURL url("http://com/");
    // Allow setting on "com", (but only as a host cookie).
    EXPECT_TRUE(cm->SetCookie(url, "a=1"));
    EXPECT_FALSE(cm->SetCookie(url, "b=2; domain=.com"));
    EXPECT_FALSE(cm->SetCookie(url, "c=3; domain=com"));
    EXPECT_EQ("a=1", cm->GetCookies(url));
    // Make sure it doesn't show up for a normal .com, it should be a host
    // not a domain cookie.
    EXPECT_EQ("", cm->GetCookies(GURL("http://hopefully-no-cookies.com/")));
    EXPECT_EQ("", cm->GetCookies(GURL("http://.com/")));
  }

  {  // http://com. should be treated the same as http://com.
    scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
    GURL url("http://com./index.html");
    EXPECT_TRUE(cm->SetCookie(url, "a=1"));
    EXPECT_EQ("a=1", cm->GetCookies(url));
    EXPECT_EQ("", cm->GetCookies(GURL("http://hopefully-no-cookies.com./")));
  }

  {  // Should not be able to set host cookie from a subdomain.
    scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
    GURL url("http://a.b");
    EXPECT_FALSE(cm->SetCookie(url, "a=1; domain=.b"));
    EXPECT_FALSE(cm->SetCookie(url, "b=2; domain=b"));
    EXPECT_EQ("", cm->GetCookies(url));
  }

  {  // Same test as above, but explicitly on a known TLD (com).
    scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
    GURL url("http://google.com");
    EXPECT_FALSE(cm->SetCookie(url, "a=1; domain=.com"));
    EXPECT_FALSE(cm->SetCookie(url, "b=2; domain=com"));
    EXPECT_EQ("", cm->GetCookies(url));
  }

  {  // Make sure can't set cookie on TLD which is dotted.
    scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
    GURL url("http://google.co.uk");
    EXPECT_FALSE(cm->SetCookie(url, "a=1; domain=.co.uk"));
    EXPECT_FALSE(cm->SetCookie(url, "b=2; domain=.uk"));
    EXPECT_EQ("", cm->GetCookies(url));
    EXPECT_EQ("", cm->GetCookies(GURL("http://something-else.co.uk")));
    EXPECT_EQ("", cm->GetCookies(GURL("http://something-else.uk")));
  }

  {  // Intranet URLs should only be able to set host cookies.
    scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
    GURL url("http://b");
    EXPECT_TRUE(cm->SetCookie(url, "a=1"));
    EXPECT_FALSE(cm->SetCookie(url, "b=2; domain=.b"));
    EXPECT_FALSE(cm->SetCookie(url, "c=3; domain=b"));
    EXPECT_EQ("a=1", cm->GetCookies(url));
  }
}

// Test reading/writing cookies when the domain ends with a period,
// as in "www.google.com."
TEST(CookieMonsterTest, TestHostEndsWithDot) {
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
  GURL url("http://www.google.com");
  GURL url_with_dot("http://www.google.com.");
  EXPECT_TRUE(cm->SetCookie(url, "a=1"));
  EXPECT_EQ("a=1", cm->GetCookies(url));

  // Do not share cookie space with the dot version of domain.
  // Note: this is not what FireFox does, but it _is_ what IE+Safari do.
  EXPECT_FALSE(cm->SetCookie(url, "b=2; domain=.www.google.com."));
  EXPECT_EQ("a=1", cm->GetCookies(url));

  EXPECT_TRUE(cm->SetCookie(url_with_dot, "b=2; domain=.google.com."));
  EXPECT_EQ("b=2", cm->GetCookies(url_with_dot));

  // Make sure there weren't any side effects.
  EXPECT_EQ(cm->GetCookies(GURL("http://hopefully-no-cookies.com/")), "");
  EXPECT_EQ("", cm->GetCookies(GURL("http://.com/")));
}

TEST(CookieMonsterTest, InvalidScheme) {
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
  EXPECT_FALSE(cm->SetCookie(GURL(kUrlFtp), kValidCookieLine));
}

TEST(CookieMonsterTest, InvalidScheme_Read) {
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
  EXPECT_TRUE(cm->SetCookie(GURL(kUrlGoogle), kValidDomainCookieLine));
  EXPECT_EQ("", cm->GetCookies(GURL(kUrlFtp)));
}

TEST(CookieMonsterTest, PathTest) {
  std::string url("http://www.google.izzle");
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
  EXPECT_TRUE(cm->SetCookie(GURL(url), "A=B; path=/wee"));
  EXPECT_EQ("A=B", cm->GetCookies(GURL(url + "/wee")));
  EXPECT_EQ("A=B", cm->GetCookies(GURL(url + "/wee/")));
  EXPECT_EQ("A=B", cm->GetCookies(GURL(url + "/wee/war")));
  EXPECT_EQ("A=B", cm->GetCookies(GURL(url + "/wee/war/more/more")));
  EXPECT_EQ("", cm->GetCookies(GURL(url + "/weehee")));
  EXPECT_EQ("", cm->GetCookies(GURL(url + "/")));

  // If we add a 0 length path, it should default to /
  EXPECT_TRUE(cm->SetCookie(GURL(url), "A=C; path="));
  EXPECT_EQ("A=B; A=C", cm->GetCookies(GURL(url + "/wee")));
  EXPECT_EQ("A=C", cm->GetCookies(GURL(url + "/")));
}

TEST(CookieMonsterTest, HttpOnlyTest) {
  GURL url_google(kUrlGoogle);
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
  net::CookieOptions options;
  options.set_include_httponly();

  // Create a httponly cookie.
  EXPECT_TRUE(cm->SetCookieWithOptions(url_google, "A=B; httponly", options));

  // Check httponly read protection.
  EXPECT_EQ("", cm->GetCookies(url_google));
  EXPECT_EQ("A=B", cm->GetCookiesWithOptions(url_google, options));

  // Check httponly overwrite protection.
  EXPECT_FALSE(cm->SetCookie(url_google, "A=C"));
  EXPECT_EQ("", cm->GetCookies(url_google));
  EXPECT_EQ("A=B", cm->GetCookiesWithOptions(url_google, options));
  EXPECT_TRUE(cm->SetCookieWithOptions(url_google, "A=C", options));
  EXPECT_EQ("A=C", cm->GetCookies(url_google));

  // Check httponly create protection.
  EXPECT_FALSE(cm->SetCookie(url_google, "B=A; httponly"));
  EXPECT_EQ("A=C", cm->GetCookiesWithOptions(url_google, options));
  EXPECT_TRUE(cm->SetCookieWithOptions(url_google, "B=A; httponly", options));
  EXPECT_EQ("A=C; B=A", cm->GetCookiesWithOptions(url_google, options));
  EXPECT_EQ("A=C", cm->GetCookies(url_google));
}

namespace {

struct CookieDateParsingCase {
  const char* str;
  const bool valid;
  const time_t epoch;
};

struct DomainIsHostOnlyCase {
  const char* str;
  const bool is_host_only;
};

}  // namespace

TEST(CookieMonsterTest, TestCookieDateParsing) {
  const CookieDateParsingCase tests[] = {
    { "Sat, 15-Apr-17 21:01:22 GMT",           true, 1492290082 },
    { "Thu, 19-Apr-2007 16:00:00 GMT",         true, 1176998400 },
    { "Wed, 25 Apr 2007 21:02:13 GMT",         true, 1177534933 },
    { "Thu, 19/Apr\\2007 16:00:00 GMT",        true, 1176998400 },
    { "Fri, 1 Jan 2010 01:01:50 GMT",          true, 1262307710 },
    { "Wednesday, 1-Jan-2003 00:00:00 GMT",    true, 1041379200 },
    { ", 1-Jan-2003 00:00:00 GMT",             true, 1041379200 },
    { " 1-Jan-2003 00:00:00 GMT",              true, 1041379200 },
    { "1-Jan-2003 00:00:00 GMT",               true, 1041379200 },
    { "Wed,18-Apr-07 22:50:12 GMT",            true, 1176936612 },
    { "WillyWonka  , 18-Apr-07 22:50:12 GMT",  true, 1176936612 },
    { "WillyWonka  , 18-Apr-07 22:50:12",      true, 1176936612 },
    { "WillyWonka  ,  18-apr-07   22:50:12",   true, 1176936612 },
    { "Mon, 18-Apr-1977 22:50:13 GMT",         true, 230251813 },
    { "Mon, 18-Apr-77 22:50:13 GMT",           true, 230251813 },
    // If the cookie came in with the expiration quoted (which in terms of
    // the RFC you shouldn't do), we will get string quoted.  Bug 1261605.
    { "\"Sat, 15-Apr-17\\\"21:01:22\\\"GMT\"", true, 1492290082 },
    // Test with full month names and partial names.
    { "Partyday, 18- April-07 22:50:12",       true, 1176936612 },
    { "Partyday, 18 - Apri-07 22:50:12",       true, 1176936612 },
    { "Wednes, 1-Januar-2003 00:00:00 GMT",    true, 1041379200 },
    // Test that we always take GMT even with other time zones or bogus
    // values.  The RFC says everything should be GMT, and in the worst case
    // we are 24 hours off because of zone issues.
    { "Sat, 15-Apr-17 21:01:22",               true, 1492290082 },
    { "Sat, 15-Apr-17 21:01:22 GMT-2",         true, 1492290082 },
    { "Sat, 15-Apr-17 21:01:22 GMT BLAH",      true, 1492290082 },
    { "Sat, 15-Apr-17 21:01:22 GMT-0400",      true, 1492290082 },
    { "Sat, 15-Apr-17 21:01:22 GMT-0400 (EDT)",true, 1492290082 },
    { "Sat, 15-Apr-17 21:01:22 DST",           true, 1492290082 },
    { "Sat, 15-Apr-17 21:01:22 -0400",         true, 1492290082 },
    { "Sat, 15-Apr-17 21:01:22 (hello there)", true, 1492290082 },
    // Test that if we encounter multiple : fields, that we take the first
    // that correctly parses.
    { "Sat, 15-Apr-17 21:01:22 11:22:33",      true, 1492290082 },
    { "Sat, 15-Apr-17 ::00 21:01:22",          true, 1492290082 },
    { "Sat, 15-Apr-17 boink:z 21:01:22",       true, 1492290082 },
    // We take the first, which in this case is invalid.
    { "Sat, 15-Apr-17 91:22:33 21:01:22",      false, 0 },
    // amazon.com formats their cookie expiration like this.
    { "Thu Apr 18 22:50:12 2007 GMT",          true, 1176936612 },
    // Test that hh:mm:ss can occur anywhere.
    { "22:50:12 Thu Apr 18 2007 GMT",          true, 1176936612 },
    { "Thu 22:50:12 Apr 18 2007 GMT",          true, 1176936612 },
    { "Thu Apr 22:50:12 18 2007 GMT",          true, 1176936612 },
    { "Thu Apr 18 22:50:12 2007 GMT",          true, 1176936612 },
    { "Thu Apr 18 2007 22:50:12 GMT",          true, 1176936612 },
    { "Thu Apr 18 2007 GMT 22:50:12",          true, 1176936612 },
    // Test that the day and year can be anywhere if they are unambigious.
    { "Sat, 15-Apr-17 21:01:22 GMT",           true, 1492290082 },
    { "15-Sat, Apr-17 21:01:22 GMT",           true, 1492290082 },
    { "15-Sat, Apr 21:01:22 GMT 17",           true, 1492290082 },
    { "15-Sat, Apr 21:01:22 GMT 2017",         true, 1492290082 },
    { "15 Apr 21:01:22 2017",                  true, 1492290082 },
    { "15 17 Apr 21:01:22",                    true, 1492290082 },
    { "Apr 15 17 21:01:22",                    true, 1492290082 },
    { "Apr 15 21:01:22 17",                    true, 1492290082 },
    { "2017 April 15 21:01:22",                true, 1492290082 },
    { "15 April 2017 21:01:22",                true, 1492290082 },
    // Some invalid dates
    { "98 April 17 21:01:22",                    false, 0 },
    { "Thu, 012-Aug-2008 20:49:07 GMT",          false, 0 },
    { "Thu, 12-Aug-31841 20:49:07 GMT",          false, 0 },
    { "Thu, 12-Aug-9999999999 20:49:07 GMT",     false, 0 },
    { "Thu, 999999999999-Aug-2007 20:49:07 GMT", false, 0 },
    { "Thu, 12-Aug-2007 20:61:99999999999 GMT",  false, 0 },
    { "IAintNoDateFool",                         false, 0 },
  };

  Time parsed_time;
  for (size_t i = 0; i < arraysize(tests); ++i) {
    parsed_time = net::CookieMonster::ParseCookieTime(tests[i].str);
    if (!tests[i].valid) {
      EXPECT_FALSE(!parsed_time.is_null()) << tests[i].str;
      continue;
    }
    EXPECT_TRUE(!parsed_time.is_null()) << tests[i].str;
    EXPECT_EQ(tests[i].epoch, parsed_time.ToTimeT()) << tests[i].str;
  }
}

TEST(CookieMonsterTest, TestDomainIsHostOnly) {
  const DomainIsHostOnlyCase tests[] = {
    { "",               true },
    { "www.google.com", true },
    { ".google.com",    false }
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    EXPECT_EQ(tests[i].is_host_only,
              net::CookieMonster::DomainIsHostOnly(tests[i].str));
  }
}

TEST(CookieMonsterTest, TestCookieDeletion) {
  GURL url_google(kUrlGoogle);
  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(store, NULL));

  // Create a session cookie.
  EXPECT_TRUE(cm->SetCookie(url_google, kValidCookieLine));
  EXPECT_EQ("A=B", cm->GetCookies(url_google));
  // Delete it via Max-Age.
  EXPECT_TRUE(cm->SetCookie(url_google,
                           std::string(kValidCookieLine) + "; max-age=0"));
  EXPECT_EQ("", cm->GetCookies(url_google));

  // Create a session cookie.
  EXPECT_TRUE(cm->SetCookie(url_google, kValidCookieLine));
  EXPECT_EQ("A=B", cm->GetCookies(url_google));
  // Delete it via Expires.
  EXPECT_TRUE(cm->SetCookie(url_google,
                           std::string(kValidCookieLine) +
                           "; expires=Mon, 18-Apr-1977 22:50:13 GMT"));
  EXPECT_EQ("", cm->GetCookies(url_google));

  // Create a persistent cookie.
  EXPECT_TRUE(cm->SetCookie(url_google,
                           std::string(kValidCookieLine) +
                           "; expires=Mon, 18-Apr-22 22:50:13 GMT"));
  ASSERT_EQ(1u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[0].type);

  EXPECT_EQ("A=B", cm->GetCookies(url_google));
  // Delete it via Max-Age.
  EXPECT_TRUE(cm->SetCookie(url_google,
                           std::string(kValidCookieLine) + "; max-age=0"));
  ASSERT_EQ(2u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[1].type);
  EXPECT_EQ("", cm->GetCookies(url_google));

  // Create a persistent cookie.
  EXPECT_TRUE(cm->SetCookie(url_google,
                           std::string(kValidCookieLine) +
                           "; expires=Mon, 18-Apr-22 22:50:13 GMT"));
  ASSERT_EQ(3u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[2].type);
  EXPECT_EQ("A=B", cm->GetCookies(url_google));
  // Delete it via Expires.
  EXPECT_TRUE(cm->SetCookie(url_google,
                           std::string(kValidCookieLine) +
                           "; expires=Mon, 18-Apr-1977 22:50:13 GMT"));
  ASSERT_EQ(4u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[3].type);
  EXPECT_EQ("", cm->GetCookies(url_google));

  // Create a persistent cookie.
  EXPECT_TRUE(cm->SetCookie(url_google,
                           std::string(kValidCookieLine) +
                           "; expires=Mon, 18-Apr-22 22:50:13 GMT"));
  ASSERT_EQ(5u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[4].type);
  EXPECT_EQ("A=B", cm->GetCookies(url_google));
  // Delete it via Expires, with a unix epoch of 0.
  EXPECT_TRUE(cm->SetCookie(url_google,
                           std::string(kValidCookieLine) +
                           "; expires=Thu, 1-Jan-1970 00:00:00 GMT"));
  ASSERT_EQ(6u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[5].type);
  EXPECT_EQ("", cm->GetCookies(url_google));
}

TEST(CookieMonsterTest, TestCookieDeleteAll) {
  GURL url_google(kUrlGoogle);
  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(store, NULL));
  net::CookieOptions options;
  options.set_include_httponly();

  EXPECT_TRUE(cm->SetCookie(url_google, kValidCookieLine));
  EXPECT_EQ("A=B", cm->GetCookies(url_google));

  EXPECT_TRUE(cm->SetCookieWithOptions(url_google, "C=D; httponly", options));
  EXPECT_EQ("A=B; C=D", cm->GetCookiesWithOptions(url_google, options));

  EXPECT_EQ(2, cm->DeleteAll(false));
  EXPECT_EQ("", cm->GetCookiesWithOptions(url_google, options));

  EXPECT_EQ(0u, store->commands().size());

  // Create a persistent cookie.
  EXPECT_TRUE(cm->SetCookie(url_google,
                            std::string(kValidCookieLine) +
                            "; expires=Mon, 18-Apr-22 22:50:13 GMT"));
  ASSERT_EQ(1u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[0].type);

  EXPECT_EQ(1, cm->DeleteAll(true));  // sync_to_store = true.
  ASSERT_EQ(2u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[1].type);

  EXPECT_EQ("", cm->GetCookiesWithOptions(url_google, options));
}

TEST(CookieMonsterTest, TestCookieDeleteAllCreatedAfterTimestamp) {
  GURL url_google(kUrlGoogle);
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
  Time now = Time::Now();

  // Nothing has been added so nothing should be deleted.
  EXPECT_EQ(0, cm->DeleteAllCreatedAfter(now - TimeDelta::FromDays(99), false));

  // Create 3 cookies with creation date of today, yesterday and the day before.
  EXPECT_TRUE(cm->SetCookieWithCreationTime(url_google, "T-0=Now", now));
  EXPECT_TRUE(cm->SetCookieWithCreationTime(url_google, "T-1=Yesterday",
                                           now - TimeDelta::FromDays(1)));
  EXPECT_TRUE(cm->SetCookieWithCreationTime(url_google, "T-2=DayBefore",
                                           now - TimeDelta::FromDays(2)));

  // Try to delete everything from now onwards.
  EXPECT_EQ(1, cm->DeleteAllCreatedAfter(now, false));
  // Now delete the one cookie created in the last day.
  EXPECT_EQ(1, cm->DeleteAllCreatedAfter(now - TimeDelta::FromDays(1), false));
  // Now effectively delete all cookies just created (1 is remaining).
  EXPECT_EQ(1, cm->DeleteAllCreatedAfter(now - TimeDelta::FromDays(99), false));

  // Make sure everything is gone.
  EXPECT_EQ(0, cm->DeleteAllCreatedAfter(Time(), false));
  // Really make sure everything is gone.
  EXPECT_EQ(0, cm->DeleteAll(false));
}

TEST(CookieMonsterTest, TestCookieDeleteAllCreatedBetweenTimestamps) {
  GURL url_google(kUrlGoogle);
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
  Time now = Time::Now();

  // Nothing has been added so nothing should be deleted.
  EXPECT_EQ(0, cm->DeleteAllCreatedAfter(now - TimeDelta::FromDays(99), false));

  // Create 3 cookies with creation date of today, yesterday and the day before.
  EXPECT_TRUE(cm->SetCookieWithCreationTime(url_google, "T-0=Now", now));
  EXPECT_TRUE(cm->SetCookieWithCreationTime(url_google, "T-1=Yesterday",
                                           now - TimeDelta::FromDays(1)));
  EXPECT_TRUE(cm->SetCookieWithCreationTime(url_google, "T-2=DayBefore",
                                           now - TimeDelta::FromDays(2)));
  EXPECT_TRUE(cm->SetCookieWithCreationTime(url_google, "T-3=ThreeDays",
                                           now - TimeDelta::FromDays(3)));
  EXPECT_TRUE(cm->SetCookieWithCreationTime(url_google, "T-7=LastWeek",
                                           now - TimeDelta::FromDays(7)));

  // Try to delete threedays and the daybefore.
  EXPECT_EQ(2, cm->DeleteAllCreatedBetween(now - TimeDelta::FromDays(3),
                                          now - TimeDelta::FromDays(1),
                                          false));

  // Try to delete yesterday, also make sure that delete_end is not
  // inclusive.
  EXPECT_EQ(1, cm->DeleteAllCreatedBetween(now - TimeDelta::FromDays(2),
                                          now,
                                          false));

  // Make sure the delete_begin is inclusive.
  EXPECT_EQ(1, cm->DeleteAllCreatedBetween(now - TimeDelta::FromDays(7),
                                          now,
                                          false));

  // Delete the last (now) item.
  EXPECT_EQ(1, cm->DeleteAllCreatedAfter(Time(), false));

  // Really make sure everything is gone.
  EXPECT_EQ(0, cm->DeleteAll(false));
}

TEST(CookieMonsterTest, TestSecure) {
  GURL url_google(kUrlGoogle);
  GURL url_google_secure(kUrlGoogleSecure);
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));

  EXPECT_TRUE(cm->SetCookie(url_google, "A=B"));
  EXPECT_EQ("A=B", cm->GetCookies(url_google));
  EXPECT_EQ("A=B", cm->GetCookies(url_google_secure));

  EXPECT_TRUE(cm->SetCookie(url_google_secure, "A=B; secure"));
  // The secure should overwrite the non-secure.
  EXPECT_EQ("", cm->GetCookies(url_google));
  EXPECT_EQ("A=B", cm->GetCookies(url_google_secure));

  EXPECT_TRUE(cm->SetCookie(url_google_secure, "D=E; secure"));
  EXPECT_EQ("", cm->GetCookies(url_google));
  EXPECT_EQ("A=B; D=E", cm->GetCookies(url_google_secure));

  EXPECT_TRUE(cm->SetCookie(url_google_secure, "A=B"));
  // The non-secure should overwrite the secure.
  EXPECT_EQ("A=B", cm->GetCookies(url_google));
  EXPECT_EQ("D=E; A=B", cm->GetCookies(url_google_secure));
}

static Time GetFirstCookieAccessDate(net::CookieMonster* cm) {
  const net::CookieList all_cookies(cm->GetAllCookies());
  return all_cookies.front().LastAccessDate();
}

static const int kLastAccessThresholdMilliseconds = 200;

TEST(CookieMonsterTest, TestLastAccess) {
  GURL url_google(kUrlGoogle);
  scoped_refptr<net::CookieMonster> cm(
      new net::CookieMonster(NULL, NULL, kLastAccessThresholdMilliseconds));

  EXPECT_TRUE(cm->SetCookie(url_google, "A=B"));
  const Time last_access_date(GetFirstCookieAccessDate(cm));

  // Reading the cookie again immediately shouldn't update the access date,
  // since we're inside the threshold.
  EXPECT_EQ("A=B", cm->GetCookies(url_google));
  EXPECT_TRUE(last_access_date == GetFirstCookieAccessDate(cm));

  // Reading after a short wait should update the access date.
  base::PlatformThread::Sleep(kLastAccessThresholdMilliseconds + 20);
  EXPECT_EQ("A=B", cm->GetCookies(url_google));
  EXPECT_FALSE(last_access_date == GetFirstCookieAccessDate(cm));
}

static int CountInString(const std::string& str, char c) {
  return std::count(str.begin(), str.end(), c);
}

static void TestHostGarbageCollectHelper(
    int domain_max_cookies,
    int domain_purge_cookies,
    CookieMonster::ExpiryAndKeyScheme key_scheme) {
  GURL url_google(kUrlGoogle);
  const int more_than_enough_cookies =
      (domain_max_cookies + domain_purge_cookies) * 2;
  // Add a bunch of cookies on a single host, should purge them.
  {
    scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
    cm->SetExpiryAndKeyScheme(key_scheme);
    for (int i = 0; i < more_than_enough_cookies; ++i) {
      std::string cookie = base::StringPrintf("a%03d=b", i);
      EXPECT_TRUE(cm->SetCookie(url_google, cookie));
      std::string cookies = cm->GetCookies(url_google);
      // Make sure we find it in the cookies.
      EXPECT_NE(cookies.find(cookie), std::string::npos);
      // Count the number of cookies.
      EXPECT_LE(CountInString(cookies, '='), domain_max_cookies);
    }
  }

  // Add a bunch of cookies on multiple hosts within a single eTLD.
  // Should keep at least kDomainMaxCookies - kDomainPurgeCookies
  // between them.  If we are using the effective domain keying system
  // (EKS_KEEP_RECENT_AND_PURGE_ETLDP1) we shouldn't go above
  // kDomainMaxCookies for both together.  If we're using the domain
  // keying system (EKS_DISCARD_RECENT_AND_PURGE_DOMAIN), each
  // individual domain shouldn't go above kDomainMaxCookies.
  GURL url_google_specific(kUrlGoogleSpecific);
  {
    scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
    cm->SetExpiryAndKeyScheme(key_scheme);
    for (int i = 0; i < more_than_enough_cookies; ++i) {
      std::string cookie_general = base::StringPrintf("a%03d=b", i);
      EXPECT_TRUE(cm->SetCookie(url_google, cookie_general));
      std::string cookie_specific = base::StringPrintf("c%03d=b", i);
      EXPECT_TRUE(cm->SetCookie(url_google_specific, cookie_specific));
      std::string cookies_general = cm->GetCookies(url_google);
      EXPECT_NE(cookies_general.find(cookie_general), std::string::npos);
      std::string cookies_specific = cm->GetCookies(url_google_specific);
      EXPECT_NE(cookies_specific.find(cookie_specific), std::string::npos);
      if (key_scheme == CookieMonster::EKS_KEEP_RECENT_AND_PURGE_ETLDP1) {
        EXPECT_LE((CountInString(cookies_general, '=') +
                   CountInString(cookies_specific, '=')),
                  domain_max_cookies);
      } else {
        EXPECT_LE(CountInString(cookies_general, '='), domain_max_cookies);
        EXPECT_LE(CountInString(cookies_specific, '='), domain_max_cookies);
      }
    }
    // After all this, there should be at least
    // kDomainMaxCookies - kDomainPurgeCookies for both URLs.
    std::string cookies_general = cm->GetCookies(url_google);
    std::string cookies_specific = cm->GetCookies(url_google_specific);
    if (key_scheme == CookieMonster::EKS_KEEP_RECENT_AND_PURGE_ETLDP1) {
      int total_cookies = (CountInString(cookies_general, '=') +
                           CountInString(cookies_specific, '='));
      EXPECT_GE(total_cookies,
                domain_max_cookies - domain_purge_cookies);
      EXPECT_LE(total_cookies, domain_max_cookies);
    } else {
      int general_cookies = CountInString(cookies_general, '=');
      int specific_cookies = CountInString(cookies_specific, '=');
      EXPECT_GE(general_cookies,
                domain_max_cookies - domain_purge_cookies);
      EXPECT_LE(general_cookies, domain_max_cookies);
      EXPECT_GE(specific_cookies,
                domain_max_cookies - domain_purge_cookies);
      EXPECT_LE(specific_cookies, domain_max_cookies);
    }
  }
}

TEST(CookieMonsterTest, TestHostGarbageCollection) {
  TestHostGarbageCollectHelper(
      CookieMonster::kDomainMaxCookies, CookieMonster::kDomainPurgeCookies,
      CookieMonster::EKS_KEEP_RECENT_AND_PURGE_ETLDP1);
  TestHostGarbageCollectHelper(
      CookieMonster::kDomainMaxCookies, CookieMonster::kDomainPurgeCookies,
      CookieMonster::EKS_DISCARD_RECENT_AND_PURGE_DOMAIN);
}

// Formerly NetUtilTest.CookieTest back when we used wininet's cookie handling.
TEST(CookieMonsterTest, NetUtilCookieTest) {
  const GURL test_url("http://mojo.jojo.google.izzle/");

  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));

  EXPECT_TRUE(cm->SetCookie(test_url, "foo=bar"));
  std::string value = cm->GetCookies(test_url);
  EXPECT_EQ("foo=bar", value);

  // test that we can retrieve all cookies:
  EXPECT_TRUE(cm->SetCookie(test_url, "x=1"));
  EXPECT_TRUE(cm->SetCookie(test_url, "y=2"));

  std::string result = cm->GetCookies(test_url);
  EXPECT_FALSE(result.empty());
  EXPECT_NE(result.find("x=1"), std::string::npos) << result;
  EXPECT_NE(result.find("y=2"), std::string::npos) << result;
}

static bool FindAndDeleteCookie(net::CookieMonster* cm,
                                const std::string& domain,
                                const std::string& name) {
  net::CookieList cookies = cm->GetAllCookies();
  for (net::CookieList::iterator it = cookies.begin();
       it != cookies.end(); ++it)
    if (it->Domain() == domain && it->Name() == name)
      return cm->DeleteCanonicalCookie(*it);
  return false;
}

TEST(CookieMonsterTest, TestDeleteSingleCookie) {
  GURL url_google(kUrlGoogle);

  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));

  EXPECT_TRUE(cm->SetCookie(url_google, "A=B"));
  EXPECT_TRUE(cm->SetCookie(url_google, "C=D"));
  EXPECT_TRUE(cm->SetCookie(url_google, "E=F"));
  EXPECT_EQ("A=B; C=D; E=F", cm->GetCookies(url_google));

  EXPECT_TRUE(FindAndDeleteCookie(cm, url_google.host(), "C"));
  EXPECT_EQ("A=B; E=F", cm->GetCookies(url_google));

  EXPECT_FALSE(FindAndDeleteCookie(cm, "random.host", "E"));
  EXPECT_EQ("A=B; E=F", cm->GetCookies(url_google));
}

TEST(CookieMonsterTest, SetCookieableSchemes) {
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
  scoped_refptr<net::CookieMonster> cm_foo(new net::CookieMonster(NULL, NULL));

  // Only cm_foo should allow foo:// cookies.
  const char* kSchemes[] = {"foo"};
  cm_foo->SetCookieableSchemes(kSchemes, 1);

  GURL foo_url("foo://host/path");
  GURL http_url("http://host/path");

  EXPECT_TRUE(cm->SetCookie(http_url, "x=1"));
  EXPECT_FALSE(cm->SetCookie(foo_url, "x=1"));
  EXPECT_TRUE(cm_foo->SetCookie(foo_url, "x=1"));
  EXPECT_FALSE(cm_foo->SetCookie(http_url, "x=1"));
}

TEST(CookieMonsterTest, GetAllCookiesForURL) {
  GURL url_google(kUrlGoogle);
  GURL url_google_secure(kUrlGoogleSecure);

  scoped_refptr<net::CookieMonster> cm(
      new net::CookieMonster(NULL, NULL, kLastAccessThresholdMilliseconds));

  // Create an httponly cookie.
  net::CookieOptions options;
  options.set_include_httponly();

  EXPECT_TRUE(cm->SetCookieWithOptions(url_google, "A=B; httponly", options));
  EXPECT_TRUE(cm->SetCookieWithOptions(url_google,
                                       "C=D; domain=.google.izzle",
                                       options));
  EXPECT_TRUE(cm->SetCookieWithOptions(url_google_secure,
                                       "E=F; domain=.google.izzle; secure",
                                       options));

  const Time last_access_date(GetFirstCookieAccessDate(cm));

  base::PlatformThread::Sleep(kLastAccessThresholdMilliseconds + 20);

  // Check cookies for url.
  net::CookieList cookies = cm->GetAllCookiesForURL(url_google);
  net::CookieList::iterator it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_EQ("A", it->Name());

  ASSERT_TRUE(++it != cookies.end());
  EXPECT_EQ(".google.izzle", it->Domain());
  EXPECT_EQ("C", it->Name());

  ASSERT_TRUE(++it == cookies.end());

  // Check cookies for url excluding http-only cookies.
  cookies =
      cm->GetAllCookiesForURLWithOptions(url_google, net::CookieOptions());
  it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ(".google.izzle", it->Domain());
  EXPECT_EQ("C", it->Name());

  ASSERT_TRUE(++it == cookies.end());

  // Test secure cookies.
  cookies = cm->GetAllCookiesForURL(url_google_secure);
  it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_EQ("A", it->Name());

  ASSERT_TRUE(++it != cookies.end());
  EXPECT_EQ(".google.izzle", it->Domain());
  EXPECT_EQ("C", it->Name());

  ASSERT_TRUE(++it != cookies.end());
  EXPECT_EQ(".google.izzle", it->Domain());
  EXPECT_EQ("E", it->Name());

  ASSERT_TRUE(++it == cookies.end());

  // Reading after a short wait should not update the access date.
  EXPECT_TRUE(last_access_date == GetFirstCookieAccessDate(cm));
}

TEST(CookieMonsterTest, GetAllCookiesForURLPathMatching) {
  GURL url_google(kUrlGoogle);
  GURL url_google_foo("http://www.google.izzle/foo");
  GURL url_google_bar("http://www.google.izzle/bar");

  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
  net::CookieOptions options;

  EXPECT_TRUE(cm->SetCookieWithOptions(url_google_foo,
                                       "A=B; path=/foo;",
                                       options));
  EXPECT_TRUE(cm->SetCookieWithOptions(url_google_bar,
                                       "C=D; path=/bar;",
                                       options));
  EXPECT_TRUE(cm->SetCookieWithOptions(url_google,
                                       "E=F;",
                                       options));

  net::CookieList cookies = cm->GetAllCookiesForURL(url_google_foo);
  net::CookieList::iterator it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("A", it->Name());
  EXPECT_EQ("/foo", it->Path());

  ASSERT_TRUE(++it != cookies.end());
  EXPECT_EQ("E", it->Name());
  EXPECT_EQ("/", it->Path());

  ASSERT_TRUE(++it == cookies.end());

  cookies = cm->GetAllCookiesForURL(url_google_bar);
  it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("C", it->Name());
  EXPECT_EQ("/bar", it->Path());

  ASSERT_TRUE(++it != cookies.end());
  EXPECT_EQ("E", it->Name());
  EXPECT_EQ("/", it->Path());

  ASSERT_TRUE(++it == cookies.end());
}

TEST(CookieMonsterTest, DeleteCookieByName) {
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
  GURL url_google(kUrlGoogle);

  EXPECT_TRUE(cm->SetCookie(url_google, "A=A1; path=/"));
  EXPECT_TRUE(cm->SetCookie(url_google, "A=A2; path=/foo"));
  EXPECT_TRUE(cm->SetCookie(url_google, "A=A3; path=/bar"));
  EXPECT_TRUE(cm->SetCookie(url_google, "B=B1; path=/"));
  EXPECT_TRUE(cm->SetCookie(url_google, "B=B2; path=/foo"));
  EXPECT_TRUE(cm->SetCookie(url_google, "B=B3; path=/bar"));

  cm->DeleteCookie(GURL(std::string(kUrlGoogle) + "/foo/bar"), "A");

  net::CookieList cookies = cm->GetAllCookies();
  size_t expected_size = 4;
  EXPECT_EQ(expected_size, cookies.size());
  for (net::CookieList::iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    EXPECT_NE("A1", it->Value());
    EXPECT_NE("A2", it->Value());
  }
}

// Test that overwriting persistent cookies deletes the old one from the
// backing store.
TEST(CookieMonsterTest, OverwritePersistentCookie) {
  GURL url_google("http://www.google.com/");
  GURL url_chromium("http://chromium.org");
  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(store, NULL));

  // Insert a cookie "a" for path "/path1"
  EXPECT_TRUE(
      cm->SetCookie(url_google, "a=val1; path=/path1; "
                                "expires=Mon, 18-Apr-22 22:50:13 GMT"));
  ASSERT_EQ(1u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[0].type);

  // Insert a cookie "b" for path "/path1"
  EXPECT_TRUE(
      cm->SetCookie(url_google, "b=val1; path=/path1; "
                                "expires=Mon, 18-Apr-22 22:50:14 GMT"));
  ASSERT_EQ(2u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[1].type);

  // Insert a cookie "b" for path "/path1", that is httponly. This should
  // overwrite the non-http-only version.
  net::CookieOptions allow_httponly;
  allow_httponly.set_include_httponly();
  EXPECT_TRUE(
    cm->SetCookieWithOptions(url_google,
                             "b=val2; path=/path1; httponly; "
                             "expires=Mon, 18-Apr-22 22:50:14 GMT",
                             allow_httponly));
  ASSERT_EQ(4u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[2].type);
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[3].type);

  // Insert a cookie "a" for path "/path1". This should overwrite.
  EXPECT_TRUE(cm->SetCookie(url_google,
                            "a=val33; path=/path1; "
                            "expires=Mon, 18-Apr-22 22:50:14 GMT"));
  ASSERT_EQ(6u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[4].type);
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[5].type);

  // Insert a cookie "a" for path "/path2". This should NOT overwrite
  // cookie "a", since the path is different.
  EXPECT_TRUE(cm->SetCookie(url_google,
                            "a=val9; path=/path2; "
                            "expires=Mon, 18-Apr-22 22:50:14 GMT"));
  ASSERT_EQ(7u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[6].type);

  // Insert a cookie "a" for path "/path1", but this time for "chromium.org".
  // Although the name and path match, the hostnames do not, so shouldn't
  // overwrite.
  EXPECT_TRUE(cm->SetCookie(url_chromium,
                            "a=val99; path=/path1; "
                            "expires=Mon, 18-Apr-22 22:50:14 GMT"));
  ASSERT_EQ(8u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[7].type);

  EXPECT_EQ("a=val33", cm->GetCookies(GURL("http://www.google.com/path1")));
  EXPECT_EQ("a=val9", cm->GetCookies(GURL("http://www.google.com/path2")));
  EXPECT_EQ("a=val99", cm->GetCookies(GURL("http://chromium.org/path1")));
}

// Tests importing from a persistent cookie store that contains duplicate
// equivalent cookies. This situation should be handled by removing the
// duplicate cookie (both from the in-memory cache, and from the backing store).
//
// This is a regression test for: http://crbug.com/17855.
TEST(CookieMonsterTest, DontImportDuplicateCookies) {
  GURL url_google("http://www.google.com/");

  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);

  // We will fill some initial cookies into the PersistentCookieStore,
  // to simulate a database with 4 duplicates.  Note that we need to
  // be careful not to have any duplicate creation times at all (as it's a
  // violation of a CookieMonster invariant) even if Time::Now() doesn't
  // move between calls.
  std::vector<net::CookieMonster::CanonicalCookie*> initial_cookies;

  // Insert 4 cookies with name "X" on path "/", with varying creation
  // dates. We expect only the most recent one to be preserved following
  // the import.

  AddCookieToList("www.google.com",
                  "X=1; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                  Time::Now() + TimeDelta::FromDays(3),
                  &initial_cookies);

  AddCookieToList("www.google.com",
                  "X=2; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                  Time::Now() + TimeDelta::FromDays(1),
                  &initial_cookies);

  // ===> This one is the WINNER (biggest creation time).  <====
  AddCookieToList("www.google.com",
                  "X=3; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                  Time::Now() + TimeDelta::FromDays(4),
                  &initial_cookies);

  AddCookieToList("www.google.com",
                  "X=4; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                  Time::Now(),
                  &initial_cookies);

  // Insert 2 cookies with name "X" on path "/2", with varying creation
  // dates. We expect only the most recent one to be preserved the import.

  // ===> This one is the WINNER (biggest creation time).  <====
  AddCookieToList("www.google.com",
                  "X=a1; path=/2; expires=Mon, 18-Apr-22 22:50:14 GMT",
                  Time::Now() + TimeDelta::FromDays(9),
                  &initial_cookies);

  AddCookieToList("www.google.com",
                  "X=a2; path=/2; expires=Mon, 18-Apr-22 22:50:14 GMT",
                  Time::Now() + TimeDelta::FromDays(2),
                  &initial_cookies);

  // Insert 1 cookie with name "Y" on path "/".
  AddCookieToList("www.google.com",
                  "Y=a; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                  Time::Now() + TimeDelta::FromDays(10),
                  &initial_cookies);

  // Inject our initial cookies into the mock PersistentCookieStore.
  store->SetLoadExpectation(true, initial_cookies);

  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(store, NULL));

  // Verify that duplicates were not imported for path "/".
  // (If this had failed, GetCookies() would have also returned X=1, X=2, X=4).
  EXPECT_EQ("X=3; Y=a", cm->GetCookies(GURL("http://www.google.com/")));

  // Verify that same-named cookie on a different path ("/x2") didn't get
  // messed up.
  EXPECT_EQ("X=a1; X=3; Y=a",
            cm->GetCookies(GURL("http://www.google.com/2/x")));

  // Verify that the PersistentCookieStore was told to kill its 4 duplicates.
  ASSERT_EQ(4u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[0].type);
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[1].type);
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[2].type);
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[3].type);
}

// Tests importing from a persistent cookie store that contains cookies
// with duplicate creation times.  This situation should be handled by
// dropping the cookies before insertion/visibility to user.
//
// This is a regression test for: http://crbug.com/43188.
TEST(CookieMonsterTest, DontImportDuplicateCreationTimes) {
  GURL url_google("http://www.google.com/");

  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);

  Time now(Time::Now());
  Time earlier(now - TimeDelta::FromDays(1));

  // Insert 8 cookies, four with the current time as creation times, and
  // four with the earlier time as creation times.  We should only get
  // two cookies remaining, but which two (other than that there should
  // be one from each set) will be random.
  std::vector<net::CookieMonster::CanonicalCookie*> initial_cookies;
  AddCookieToList("www.google.com", "X=1; path=/", now, &initial_cookies);
  AddCookieToList("www.google.com", "X=2; path=/", now, &initial_cookies);
  AddCookieToList("www.google.com", "X=3; path=/", now, &initial_cookies);
  AddCookieToList("www.google.com", "X=4; path=/", now, &initial_cookies);

  AddCookieToList("www.google.com", "Y=1; path=/", earlier, &initial_cookies);
  AddCookieToList("www.google.com", "Y=2; path=/", earlier, &initial_cookies);
  AddCookieToList("www.google.com", "Y=3; path=/", earlier, &initial_cookies);
  AddCookieToList("www.google.com", "Y=4; path=/", earlier, &initial_cookies);

  // Inject our initial cookies into the mock PersistentCookieStore.
  store->SetLoadExpectation(true, initial_cookies);

  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(store, NULL));

  net::CookieList list(cm->GetAllCookies());
  EXPECT_EQ(2U, list.size());
  // Confirm that we have one of each.
  std::string name1(list[0].Name());
  std::string name2(list[1].Name());
  EXPECT_TRUE(name1 == "X" || name2 == "X");
  EXPECT_TRUE(name1 == "Y" || name2 == "Y");
  EXPECT_NE(name1, name2);
}

TEST(CookieMonsterTest, Delegate) {
  GURL url_google(kUrlGoogle);

  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);
  scoped_refptr<MockCookieMonsterDelegate> delegate(
      new MockCookieMonsterDelegate);
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(store, delegate));

  EXPECT_TRUE(cm->SetCookie(url_google, "A=B"));
  EXPECT_TRUE(cm->SetCookie(url_google, "C=D"));
  EXPECT_TRUE(cm->SetCookie(url_google, "E=F"));
  EXPECT_EQ("A=B; C=D; E=F", cm->GetCookies(url_google));
  ASSERT_EQ(3u, delegate->changes().size());
  EXPECT_FALSE(delegate->changes()[0].second);
  EXPECT_EQ(url_google.host(), delegate->changes()[0].first.Domain());
  EXPECT_EQ("A", delegate->changes()[0].first.Name());
  EXPECT_EQ("B", delegate->changes()[0].first.Value());
  EXPECT_EQ(url_google.host(), delegate->changes()[1].first.Domain());
  EXPECT_FALSE(delegate->changes()[1].second);
  EXPECT_EQ("C", delegate->changes()[1].first.Name());
  EXPECT_EQ("D", delegate->changes()[1].first.Value());
  EXPECT_EQ(url_google.host(), delegate->changes()[2].first.Domain());
  EXPECT_FALSE(delegate->changes()[2].second);
  EXPECT_EQ("E", delegate->changes()[2].first.Name());
  EXPECT_EQ("F", delegate->changes()[2].first.Value());
  delegate->reset();

  EXPECT_TRUE(FindAndDeleteCookie(cm, url_google.host(), "C"));
  EXPECT_EQ("A=B; E=F", cm->GetCookies(url_google));
  ASSERT_EQ(1u, delegate->changes().size());
  EXPECT_EQ(url_google.host(), delegate->changes()[0].first.Domain());
  EXPECT_TRUE(delegate->changes()[0].second);
  EXPECT_EQ("C", delegate->changes()[0].first.Name());
  EXPECT_EQ("D", delegate->changes()[0].first.Value());
  delegate->reset();

  EXPECT_FALSE(FindAndDeleteCookie(cm, "random.host", "E"));
  EXPECT_EQ("A=B; E=F", cm->GetCookies(url_google));
  EXPECT_EQ(0u, delegate->changes().size());

  // Insert a cookie "a" for path "/path1"
  EXPECT_TRUE(
      cm->SetCookie(url_google, "a=val1; path=/path1; "
                                "expires=Mon, 18-Apr-22 22:50:13 GMT"));
  ASSERT_EQ(1u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[0].type);
  ASSERT_EQ(1u, delegate->changes().size());
  EXPECT_FALSE(delegate->changes()[0].second);
  EXPECT_EQ(url_google.host(), delegate->changes()[0].first.Domain());
  EXPECT_EQ("a", delegate->changes()[0].first.Name());
  EXPECT_EQ("val1", delegate->changes()[0].first.Value());
  delegate->reset();

  // Insert a cookie "a" for path "/path1", that is httponly. This should
  // overwrite the non-http-only version.
  net::CookieOptions allow_httponly;
  allow_httponly.set_include_httponly();
  EXPECT_TRUE(
    cm->SetCookieWithOptions(url_google,
                             "a=val2; path=/path1; httponly; "
                             "expires=Mon, 18-Apr-22 22:50:14 GMT",
                             allow_httponly));
  ASSERT_EQ(3u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[1].type);
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[2].type);
  ASSERT_EQ(2u, delegate->changes().size());
  EXPECT_EQ(url_google.host(), delegate->changes()[0].first.Domain());
  EXPECT_TRUE(delegate->changes()[0].second);
  EXPECT_EQ("a", delegate->changes()[0].first.Name());
  EXPECT_EQ("val1", delegate->changes()[0].first.Value());
  EXPECT_EQ(url_google.host(), delegate->changes()[1].first.Domain());
  EXPECT_FALSE(delegate->changes()[1].second);
  EXPECT_EQ("a", delegate->changes()[1].first.Name());
  EXPECT_EQ("val2", delegate->changes()[1].first.Value());
  delegate->reset();
}

TEST(CookieMonsterTest, SetCookieWithDetails) {
  GURL url_google(kUrlGoogle);
  GURL url_google_foo("http://www.google.izzle/foo");
  GURL url_google_bar("http://www.google.izzle/bar");
  GURL url_google_secure(kUrlGoogleSecure);

  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));

  EXPECT_TRUE(cm->SetCookieWithDetails(
      url_google_foo, "A", "B", std::string(), "/foo", base::Time(),
      false, false));
  EXPECT_TRUE(cm->SetCookieWithDetails(
      url_google_bar, "C", "D", "google.izzle", "/bar", base::Time(),
      false, true));
  EXPECT_TRUE(cm->SetCookieWithDetails(
      url_google, "E", "F", std::string(), std::string(), base::Time(),
      true, false));

  // Test that malformed attributes fail to set the cookie.
  EXPECT_FALSE(cm->SetCookieWithDetails(
      url_google_foo, " A", "B", std::string(), "/foo", base::Time(),
      false, false));
  EXPECT_FALSE(cm->SetCookieWithDetails(
      url_google_foo, "A;", "B", std::string(), "/foo", base::Time(),
      false, false));
  EXPECT_FALSE(cm->SetCookieWithDetails(
      url_google_foo, "A=", "B", std::string(), "/foo", base::Time(),
      false, false));
  EXPECT_FALSE(cm->SetCookieWithDetails(
      url_google_foo, "A", "B", "google.ozzzzzzle", "foo", base::Time(),
      false, false));
  EXPECT_FALSE(cm->SetCookieWithDetails(
      url_google_foo, "A=", "B", std::string(), "foo", base::Time(),
      false, false));

  net::CookieList cookies = cm->GetAllCookiesForURL(url_google_foo);
  net::CookieList::iterator it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("A", it->Name());
  EXPECT_EQ("B", it->Value());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_EQ("/foo", it->Path());
  EXPECT_FALSE(it->DoesExpire());
  EXPECT_FALSE(it->IsSecure());
  EXPECT_FALSE(it->IsHttpOnly());

  ASSERT_TRUE(++it == cookies.end());

  cookies = cm->GetAllCookiesForURL(url_google_bar);
  it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("C", it->Name());
  EXPECT_EQ("D", it->Value());
  EXPECT_EQ(".google.izzle", it->Domain());
  EXPECT_EQ("/bar", it->Path());
  EXPECT_FALSE(it->IsSecure());
  EXPECT_TRUE(it->IsHttpOnly());

  ASSERT_TRUE(++it == cookies.end());

  cookies = cm->GetAllCookiesForURL(url_google_secure);
  it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("E", it->Name());
  EXPECT_EQ("F", it->Value());
  EXPECT_EQ("/", it->Path());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_TRUE(it->IsSecure());
  EXPECT_FALSE(it->IsHttpOnly());

  ASSERT_TRUE(++it == cookies.end());
}

TEST(CookieMonsterTest, DeleteAllForHost) {
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));

  // Test probes:
  //    * Non-secure URL, mid-level (http://w.c.b.a)
  //    * Secure URL, mid-level (https://w.c.b.a)
  //    * URL with path, mid-level (https:/w.c.b.a/dir1/xx)
  // All three tests should nuke only the midlevel host cookie,
  // the http_only cookie, the host secure cookie, and the two host
  // path cookies.  http_only, secure, and paths are ignored by
  // this call, and domain cookies arent touched.
  PopulateCmForDeleteAllForHost(cm);
  EXPECT_EQ("dom_1=X; dom_2=X; dom_3=X; host_3=X",
            cm->GetCookies(GURL(kTopLevelDomainPlus3)));
  EXPECT_EQ("dom_1=X; dom_2=X; host_2=X; sec_dom=X; sec_host=X",
            cm->GetCookies(GURL(kTopLevelDomainPlus2Secure)));
  EXPECT_EQ("dom_1=X; host_1=X", cm->GetCookies(GURL(kTopLevelDomainPlus1)));
  EXPECT_EQ("dom_path_2=X; host_path_2=X; dom_path_1=X; host_path_1=X; "
            "dom_1=X; dom_2=X; host_2=X; sec_dom=X; sec_host=X",
            cm->GetCookies(GURL(kTopLevelDomainPlus2Secure +
                                std::string("/dir1/dir2/xxx"))));

  EXPECT_EQ(5, cm->DeleteAllForHost(GURL(kTopLevelDomainPlus2)));
  EXPECT_EQ(8U, cm->GetAllCookies().size());

  EXPECT_EQ("dom_1=X; dom_2=X; dom_3=X; host_3=X",
            cm->GetCookies(GURL(kTopLevelDomainPlus3)));
  EXPECT_EQ("dom_1=X; dom_2=X; sec_dom=X",
            cm->GetCookies(GURL(kTopLevelDomainPlus2Secure)));
  EXPECT_EQ("dom_1=X; host_1=X", cm->GetCookies(GURL(kTopLevelDomainPlus1)));
  EXPECT_EQ("dom_path_2=X; dom_path_1=X; dom_1=X; dom_2=X; sec_dom=X",
            cm->GetCookies(GURL(kTopLevelDomainPlus2Secure +
                                std::string("/dir1/dir2/xxx"))));

  PopulateCmForDeleteAllForHost(cm);
  EXPECT_EQ(5, cm->DeleteAllForHost(GURL(kTopLevelDomainPlus2Secure)));
  EXPECT_EQ(8U, cm->GetAllCookies().size());

  EXPECT_EQ("dom_1=X; dom_2=X; dom_3=X; host_3=X",
            cm->GetCookies(GURL(kTopLevelDomainPlus3)));
  EXPECT_EQ("dom_1=X; dom_2=X; sec_dom=X",
            cm->GetCookies(GURL(kTopLevelDomainPlus2Secure)));
  EXPECT_EQ("dom_1=X; host_1=X", cm->GetCookies(GURL(kTopLevelDomainPlus1)));
  EXPECT_EQ("dom_path_2=X; dom_path_1=X; dom_1=X; dom_2=X; sec_dom=X",
            cm->GetCookies(GURL(kTopLevelDomainPlus2Secure +
                                std::string("/dir1/dir2/xxx"))));

  PopulateCmForDeleteAllForHost(cm);
  EXPECT_EQ(5, cm->DeleteAllForHost(GURL(kTopLevelDomainPlus2Secure +
                                         std::string("/dir1/xxx"))));
  EXPECT_EQ(8U, cm->GetAllCookies().size());

  EXPECT_EQ("dom_1=X; dom_2=X; dom_3=X; host_3=X",
            cm->GetCookies(GURL(kTopLevelDomainPlus3)));
  EXPECT_EQ("dom_1=X; dom_2=X; sec_dom=X",
            cm->GetCookies(GURL(kTopLevelDomainPlus2Secure)));
  EXPECT_EQ("dom_1=X; host_1=X", cm->GetCookies(GURL(kTopLevelDomainPlus1)));
  EXPECT_EQ("dom_path_2=X; dom_path_1=X; dom_1=X; dom_2=X; sec_dom=X",
            cm->GetCookies(GURL(kTopLevelDomainPlus2Secure +
                                std::string("/dir1/dir2/xxx"))));

}

TEST(CookieMonsterTest, UniqueCreationTime) {
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
  GURL url_google(kUrlGoogle);
  net::CookieOptions options;

  // Add in three cookies through every public interface to the
  // CookieMonster and confirm that none of them have duplicate
  // creation times.

  // SetCookieWithCreationTime and SetCookieWithCreationTimeAndOptions
  // are not included as they aren't going to be public for very much
  // longer.

  // SetCookie, SetCookies, SetCookiesWithOptions,
  // SetCookieWithOptions, SetCookieWithDetails

  cm->SetCookie(url_google, "SetCookie1=A");
  cm->SetCookie(url_google, "SetCookie2=A");
  cm->SetCookie(url_google, "SetCookie3=A");

  {
    std::vector<std::string> cookie_lines;
    cookie_lines.push_back("setCookies1=A");
    cookie_lines.push_back("setCookies2=A");
    cookie_lines.push_back("setCookies4=A");
    cm->SetCookies(url_google, cookie_lines);
  }

  {
    std::vector<std::string> cookie_lines;
    cookie_lines.push_back("setCookiesWithOptions1=A");
    cookie_lines.push_back("setCookiesWithOptions2=A");
    cookie_lines.push_back("setCookiesWithOptions3=A");

    cm->SetCookiesWithOptions(url_google, cookie_lines, options);
  }

  cm->SetCookieWithOptions(url_google, "setCookieWithOptions1=A", options);
  cm->SetCookieWithOptions(url_google, "setCookieWithOptions2=A", options);
  cm->SetCookieWithOptions(url_google, "setCookieWithOptions3=A", options);

  cm->SetCookieWithDetails(url_google, "setCookieWithDetails1", "A",
                           ".google.com", "/", Time(), false, false);
  cm->SetCookieWithDetails(url_google, "setCookieWithDetails2", "A",
                           ".google.com", "/", Time(), false, false);
  cm->SetCookieWithDetails(url_google, "setCookieWithDetails3", "A",
                           ".google.com", "/", Time(), false, false);

  // Now we check
  net::CookieList cookie_list(cm->GetAllCookies());
  typedef std::map<int64, net::CookieMonster::CanonicalCookie> TimeCookieMap;
  TimeCookieMap check_map;
  for (net::CookieList::const_iterator it = cookie_list.begin();
       it != cookie_list.end(); it++) {
    const int64 creation_date = it->CreationDate().ToInternalValue();
    TimeCookieMap::const_iterator
        existing_cookie_it(check_map.find(creation_date));
    EXPECT_TRUE(existing_cookie_it == check_map.end())
        << "Cookie " << it->Name() << " has same creation date ("
        << it->CreationDate().ToInternalValue()
        << ") as previously entered cookie "
        << existing_cookie_it->second.Name();

    if (existing_cookie_it == check_map.end()) {
      check_map.insert(TimeCookieMap::value_type(
          it->CreationDate().ToInternalValue(), *it));
    }
  }
}

// Mainly a test of GetEffectiveDomain, or more specifically, of the
// expected behavior of GetEffectiveDomain within the CookieMonster.
TEST(CookieMonsterTest, GetKey) {
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));

  // This test is really only interesting if GetKey() actually does something.
  if (cm->expiry_and_key_scheme_ ==
      CookieMonster::EKS_KEEP_RECENT_AND_PURGE_ETLDP1) {
    EXPECT_EQ("google.com", cm->GetKey("www.google.com"));
    EXPECT_EQ("google.izzie", cm->GetKey("www.google.izzie"));
    EXPECT_EQ("google.izzie", cm->GetKey(".google.izzie"));
    EXPECT_EQ("bbc.co.uk", cm->GetKey("bbc.co.uk"));
    EXPECT_EQ("bbc.co.uk", cm->GetKey("a.b.c.d.bbc.co.uk"));
    EXPECT_EQ("apple.com", cm->GetKey("a.b.c.d.apple.com"));
    EXPECT_EQ("apple.izzie", cm->GetKey("a.b.c.d.apple.izzie"));

    // Cases where the effective domain is null, so we use the host
    // as the key.
    EXPECT_EQ("co.uk", cm->GetKey("co.uk"));
    const std::string extension_name("iehocdgbbocmkdidlbnnfbmbinnahbae");
    EXPECT_EQ(extension_name, cm->GetKey(extension_name));
    EXPECT_EQ("com", cm->GetKey("com"));
    EXPECT_EQ("hostalias", cm->GetKey("hostalias"));
    EXPECT_EQ("localhost", cm->GetKey("localhost"));
  }
}

// Test that cookies transfer from/to the backing store correctly.
TEST(CookieMonsterTest, BackingStoreCommunication) {
  // Store details for cookies transforming through the backing store interface.

  base::Time current(base::Time::Now());
  scoped_refptr<MockSimplePersistentCookieStore> store(
      new MockSimplePersistentCookieStore);
  base::Time new_access_time;
  base::Time expires(base::Time::Now() + base::TimeDelta::FromSeconds(100));

  struct CookiesInputInfo {
    std::string gurl;
    std::string name;
    std::string value;
    std::string domain;
    std::string path;
    base::Time expires;
    bool secure;
    bool http_only;
  };
  const CookiesInputInfo input_info[] = {
    {"http://a.b.google.com", "a", "1", "", "/path/to/cookie", expires,
     false, false},
    {"https://www.google.com", "b", "2", ".google.com", "/path/from/cookie",
     expires + TimeDelta::FromSeconds(10), true, true},
    {"https://google.com", "c", "3", "", "/another/path/to/cookie",
     base::Time::Now() + base::TimeDelta::FromSeconds(100),
     true, false}
  };
  const int INPUT_DELETE = 1;

  // Create new cookies and flush them to the store.
  {
    scoped_refptr<net::CookieMonster> cmout(new CookieMonster(store, NULL));
    for (const CookiesInputInfo* p = input_info;
         p < &input_info[ARRAYSIZE_UNSAFE(input_info)]; p++) {
      EXPECT_TRUE(cmout->SetCookieWithDetails(GURL(p->gurl), p->name, p->value,
                                              p->domain, p->path, p->expires,
                                              p->secure, p->http_only));
    }
    cmout->DeleteCookie(GURL(std::string(input_info[INPUT_DELETE].gurl) +
                             input_info[INPUT_DELETE].path),
                        input_info[INPUT_DELETE].name);
  }

  // Create a new cookie monster and make sure that everything is correct
  {
    scoped_refptr<net::CookieMonster> cmin(new CookieMonster(store, NULL));
    CookieList cookies(cmin->GetAllCookies());
    ASSERT_EQ(2u, cookies.size());
    // Ordering is path length, then creation time.  So second cookie
    // will come first, and we need to swap them.
    std::swap(cookies[0], cookies[1]);
    for (int output_index = 0; output_index < 2; output_index++) {
      int input_index = output_index * 2;
      const CookiesInputInfo* input = &input_info[input_index];
      const CookieMonster::CanonicalCookie* output = &cookies[output_index];

      EXPECT_EQ(input->name, output->Name());
      EXPECT_EQ(input->value, output->Value());
      EXPECT_EQ(GURL(input->gurl).host(), output->Domain());
      EXPECT_EQ(input->path, output->Path());
      EXPECT_LE(current.ToInternalValue(),
                output->CreationDate().ToInternalValue());
      EXPECT_EQ(input->secure, output->IsSecure());
      EXPECT_EQ(input->http_only, output->IsHttpOnly());
      EXPECT_TRUE(output->IsPersistent());
      EXPECT_EQ(input->expires.ToInternalValue(),
                output->ExpiryDate().ToInternalValue());
    }
  }
}

TEST(CookieMonsterTest, CookieOrdering) {
  // Put a random set of cookies into a monster and make sure
  // they're returned in the right order.
  scoped_refptr<net::CookieMonster> cm(new CookieMonster(NULL, NULL));
  EXPECT_TRUE(cm->SetCookie(GURL("http://d.c.b.a.google.com/aa/x.html"),
                            "c=1"));
  EXPECT_TRUE(cm->SetCookie(GURL("http://b.a.google.com/aa/bb/cc/x.html"),
                            "d=1; domain=b.a.google.com"));
  EXPECT_TRUE(cm->SetCookie(GURL("http://b.a.google.com/aa/bb/cc/x.html"),
                            "a=4; domain=b.a.google.com"));
  EXPECT_TRUE(cm->SetCookie(GURL("http://c.b.a.google.com/aa/bb/cc/x.html"),
                            "e=1; domain=c.b.a.google.com"));
  EXPECT_TRUE(cm->SetCookie(GURL("http://d.c.b.a.google.com/aa/bb/x.html"),
                            "b=1"));
  EXPECT_TRUE(cm->SetCookie(GURL("http://news.bbc.co.uk/midpath/x.html"),
                            "g=10"));
  EXPECT_EQ("d=1; a=4; e=1; b=1; c=1",
            cm->GetCookiesWithOptions(
                GURL("http://d.c.b.a.google.com/aa/bb/cc/dd"),
                CookieOptions()));
  {
    unsigned int i = 0;
    CookieList cookies(cm->GetAllCookiesForURL(
        GURL("http://d.c.b.a.google.com/aa/bb/cc/dd")));
    ASSERT_EQ(5u, cookies.size());
    EXPECT_EQ("d", cookies[i++].Name());
    EXPECT_EQ("a", cookies[i++].Name());
    EXPECT_EQ("e", cookies[i++].Name());
    EXPECT_EQ("b", cookies[i++].Name());
    EXPECT_EQ("c", cookies[i++].Name());
  }

  {
    unsigned int i = 0;
    CookieList cookies(cm->GetAllCookies());
    ASSERT_EQ(6u, cookies.size());
    EXPECT_EQ("d", cookies[i++].Name());
    EXPECT_EQ("a", cookies[i++].Name());
    EXPECT_EQ("e", cookies[i++].Name());
    EXPECT_EQ("g", cookies[i++].Name());
    EXPECT_EQ("b", cookies[i++].Name());
    EXPECT_EQ("c", cookies[i++].Name());
  }
  cm->ValidateMap(0);  // Quick check that ValidateMap() doesn't fail.
}


// Function for creating a CM with a number of cookies in it,
// no store (and hence no ability to affect access time).
static net::CookieMonster* CreateMonsterForGC(int num_cookies) {
  net::CookieMonster* cm(new net::CookieMonster(NULL, NULL));
  for (int i = 0; i < num_cookies; i++)
    cm->SetCookie(GURL(StringPrintf("http://h%05d.izzle", i)), "a=1");
  return cm;
}

// This test and CookieMonstertest.TestGCTimes (in cookie_monster_perftest.cc)
// are somewhat complementary twins.  This test is probing for whether
// garbage collection always happens when it should (i.e. that we actually
// get rid of cookies when we should).  The perftest is probing for
// whether garbage collection happens when it shouldn't.  See comments
// before that test for more details.

// TODO(eroman): Re-enable this test! I disabled this test because after
// committing r77790 (which adds some validation checks), the test started
// timing out on tsan bots. This test was already running pretty slowly
// (3 minutes), but my change must have put it over the edge. r77790 is
// only temporary, so once it is reverted this can be re-enabled.
TEST(CookieMonsterTest, DISABLED_GarbageCollectionTriggers) {
  // First we check to make sure that a whole lot of recent cookies
  // doesn't get rid of anything after garbage collection is checked for.
  {
    scoped_refptr<net::CookieMonster> cm(
        CreateMonsterForGC(CookieMonster::kMaxCookies * 2));
    EXPECT_EQ(CookieMonster::kMaxCookies * 2, cm->GetAllCookies().size());
    cm->SetCookie(GURL("http://newdomain.com"), "b=2");
    EXPECT_EQ(CookieMonster::kMaxCookies * 2 + 1, cm->GetAllCookies().size());
  }

  // Now we explore a series of relationships between cookie last access
  // time and size of store to make sure we only get rid of cookies when
  // we really should.
  const struct TestCase {
    int num_cookies;
    int num_old_cookies;
    int expected_initial_cookies;
    // Indexed by ExpiryAndKeyScheme
    int expected_cookies_after_set[CookieMonster::EKS_LAST_ENTRY];
  } test_cases[] = {
    {
      // A whole lot of recent cookies; gc shouldn't happen.
      CookieMonster::kMaxCookies * 2,
      0,
      CookieMonster::kMaxCookies * 2,
      { CookieMonster::kMaxCookies * 2 + 1,
        CookieMonster::kMaxCookies - CookieMonster::kPurgeCookies }
    }, {
      // Some old cookies, but still overflowing max.
      CookieMonster::kMaxCookies * 2,
      CookieMonster::kMaxCookies / 2,
      CookieMonster::kMaxCookies * 2,
      {CookieMonster::kMaxCookies * 2 - CookieMonster::kMaxCookies / 2 + 1,
       CookieMonster::kMaxCookies - CookieMonster::kPurgeCookies}
    }, {
      // Old cookies enough to bring us right down to our purge line.
      CookieMonster::kMaxCookies * 2,
      CookieMonster::kMaxCookies + CookieMonster::kPurgeCookies + 1,
      CookieMonster::kMaxCookies * 2,
      {CookieMonster::kMaxCookies - CookieMonster::kPurgeCookies,
       CookieMonster::kMaxCookies - CookieMonster::kPurgeCookies}
    }, {
      // Old cookies enough to bring below our purge line (which we
      // shouldn't do).
      CookieMonster::kMaxCookies * 2,
      CookieMonster::kMaxCookies * 3 / 2,
      CookieMonster::kMaxCookies * 2,
      {CookieMonster::kMaxCookies - CookieMonster::kPurgeCookies,
       CookieMonster::kMaxCookies - CookieMonster::kPurgeCookies}
    }
  };
  const CookieMonster::ExpiryAndKeyScheme schemes[] = {
    CookieMonster::EKS_KEEP_RECENT_AND_PURGE_ETLDP1,
    CookieMonster::EKS_DISCARD_RECENT_AND_PURGE_DOMAIN,
  };

  for (int ci = 0; ci < static_cast<int>(ARRAYSIZE_UNSAFE(test_cases)); ++ci) {
    // Test both old and new key and expiry schemes.
    for (int recent_scheme = 0;
         recent_scheme < static_cast<int>(ARRAYSIZE_UNSAFE(schemes));
         recent_scheme++) {
      const TestCase *test_case = &test_cases[ci];
      scoped_refptr<net::CookieMonster> cm(
          CreateMonsterFromStoreForGC(
              test_case->num_cookies, test_case->num_old_cookies,
              CookieMonster::kSafeFromGlobalPurgeDays * 2));
      cm->SetExpiryAndKeyScheme(schemes[recent_scheme]);
      cm->ValidateMap(0);
      EXPECT_EQ(test_case->expected_initial_cookies,
                static_cast<int>(cm->GetAllCookies().size()))
          << "For test case " << ci;
      // Will trigger GC
      cm->ValidateMap(0);
      cm->SetCookie(GURL("http://newdomain.com"), "b=2");
      cm->ValidateMap(0);
      EXPECT_EQ(test_case->expected_cookies_after_set[recent_scheme],
                static_cast<int>((cm->GetAllCookies().size())))
          << "For test case (" << ci << ", " << recent_scheme << ")";
    }
  }
}

// This test checks that setting a cookie forcing it to be a session only
// cookie works as expected.
TEST(CookieMonsterTest, ForceSessionOnly) {
  GURL url_google(kUrlGoogle);
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
  net::CookieOptions options;

  // Set a persistent cookie, but force it to be a session cookie.
  options.set_force_session();
  ASSERT_TRUE(cm->SetCookieWithOptions(url_google,
      std::string(kValidCookieLine) + "; expires=Mon, 18-Apr-22 22:50:13 GMT",
      options));

  // Get the canonical cookie.
  CookieList cookie_list = cm->GetAllCookies();
  ASSERT_EQ(1U, cookie_list.size());
  ASSERT_FALSE(cookie_list[0].IsPersistent());

  // Use a past expiry date to delete the cookie, but force it to session only.
  ASSERT_TRUE(cm->SetCookieWithOptions(url_google,
      std::string(kValidCookieLine) + "; expires=Mon, 18-Apr-1977 22:50:13 GMT",
      options));

  // Check that the cookie was deleted.
  cookie_list = cm->GetAllCookies();
  ASSERT_EQ(0U, cookie_list.size());
}

namespace {

// Mock PersistentCookieStore that keeps track of the number of Flush() calls.
class FlushablePersistentStore : public CookieMonster::PersistentCookieStore {
 public:
  FlushablePersistentStore() : flush_count_(0) {}

  bool Load(std::vector<CookieMonster::CanonicalCookie*>*) {
    return false;
  }

  void AddCookie(const CookieMonster::CanonicalCookie&) {}
  void UpdateCookieAccessTime(const CookieMonster::CanonicalCookie&) {}
  void DeleteCookie(const CookieMonster::CanonicalCookie&) {}
  void SetClearLocalStateOnExit(bool clear_local_state) {}

  void Flush(Task* completion_callback) {
    ++flush_count_;
    if (completion_callback) {
      completion_callback->Run();
      delete completion_callback;
    }
  }

  int flush_count() {
    return flush_count_;
  }

 private:
  volatile int flush_count_;
};

// Counts the number of times Callback() has been run.
class CallbackCounter : public base::RefCountedThreadSafe<CallbackCounter> {
 public:
  CallbackCounter() : callback_count_(0) {}

  void Callback() {
    ++callback_count_;
  }

  int callback_count() {
    return callback_count_;
  }

 private:
  friend class base::RefCountedThreadSafe<CallbackCounter>;
  volatile int callback_count_;
};

}  // namespace

// Test that FlushStore() is forwarded to the store and callbacks are posted.
TEST(CookieMonsterTest, FlushStore) {
  scoped_refptr<CallbackCounter> counter(new CallbackCounter());
  scoped_refptr<FlushablePersistentStore> store(new FlushablePersistentStore());
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(store, NULL));

  ASSERT_EQ(0, store->flush_count());
  ASSERT_EQ(0, counter->callback_count());

  // Before initialization, FlushStore() should just run the callback.
  cm->FlushStore(NewRunnableMethod(counter.get(), &CallbackCounter::Callback));
  MessageLoop::current()->RunAllPending();

  ASSERT_EQ(0, store->flush_count());
  ASSERT_EQ(1, counter->callback_count());

  // NULL callback is safe.
  cm->FlushStore(NULL);
  MessageLoop::current()->RunAllPending();

  ASSERT_EQ(0, store->flush_count());
  ASSERT_EQ(1, counter->callback_count());

  // After initialization, FlushStore() should delegate to the store.
  cm->GetAllCookies();  // Force init.
  cm->FlushStore(NewRunnableMethod(counter.get(), &CallbackCounter::Callback));
  MessageLoop::current()->RunAllPending();

  ASSERT_EQ(1, store->flush_count());
  ASSERT_EQ(2, counter->callback_count());

  // NULL callback is still safe.
  cm->FlushStore(NULL);
  MessageLoop::current()->RunAllPending();

  ASSERT_EQ(2, store->flush_count());
  ASSERT_EQ(2, counter->callback_count());

  // If there's no backing store, FlushStore() is always a safe no-op.
  cm = new net::CookieMonster(NULL, NULL);
  cm->GetAllCookies();  // Force init.
  cm->FlushStore(NULL);
  MessageLoop::current()->RunAllPending();

  ASSERT_EQ(2, counter->callback_count());

  cm->FlushStore(NewRunnableMethod(counter.get(), &CallbackCounter::Callback));
  MessageLoop::current()->RunAllPending();

  ASSERT_EQ(3, counter->callback_count());
}

TEST(CookieMonsterTest, GetCookieSourceFromURL) {
  EXPECT_EQ("http://example.com/",
            CookieMonster::CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com")));
  EXPECT_EQ("http://example.com/",
            CookieMonster::CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com/")));
  EXPECT_EQ("http://example.com/",
            CookieMonster::CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com/test")));
  EXPECT_EQ("file:///tmp/test.html",
            CookieMonster::CanonicalCookie::GetCookieSourceFromURL(
                GURL("file:///tmp/test.html")));
  EXPECT_EQ("http://example.com/",
            CookieMonster::CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com:1234/")));
  EXPECT_EQ("http://example.com/",
            CookieMonster::CanonicalCookie::GetCookieSourceFromURL(
                GURL("https://example.com/")));
  EXPECT_EQ("http://example.com/",
            CookieMonster::CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://user:pwd@example.com/")));
  EXPECT_EQ("http://example.com/",
            CookieMonster::CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com/test?foo")));
  EXPECT_EQ("http://example.com/",
            CookieMonster::CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com/test#foo")));
}

}  // namespace
