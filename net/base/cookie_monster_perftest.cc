// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/perftimer.h"
#include "base/string_util.h"
#include "net/base/cookie_monster.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "googleurl/src/gurl.h"

namespace {
  class ParsedCookieTest : public testing::Test { };
  class CookieMonsterTest : public testing::Test { };
}

static const int kNumCookies = 20000;
static const char kCookieLine[] = "A  = \"b=;\\\"\"  ;secure;;;";

TEST(ParsedCookieTest, TestParseCookies) {
  std::string cookie(kCookieLine);
  PerfTimeLogger timer("Parsed_cookie_parse_cookies");
  for (int i = 0; i < kNumCookies; ++i) {
    net::CookieMonster::ParsedCookie pc(cookie);
    EXPECT_TRUE(pc.IsValid());
  }
  timer.Done();
}

TEST(ParsedCookieTest, TestParseBigCookies) {
  std::string cookie(3800, 'z');
  cookie += kCookieLine;
  PerfTimeLogger timer("Parsed_cookie_parse_big_cookies");
  for (int i = 0; i < kNumCookies; ++i) {
    net::CookieMonster::ParsedCookie pc(cookie);
    EXPECT_TRUE(pc.IsValid());
  }
  timer.Done();
}

static const GURL kUrlGoogle("http://www.google.izzle");

TEST(CookieMonsterTest, TestAddCookiesOnSingleHost) {
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
  std::vector<std::string> cookies;
  for (int i = 0; i < kNumCookies; i++) {
    cookies.push_back(StringPrintf("a%03d=b", i));
  }

  // Add a bunch of cookies on a single host
  PerfTimeLogger timer("Cookie_monster_add_single_host");
  for (std::vector<std::string>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    EXPECT_TRUE(cm->SetCookie(kUrlGoogle, *it));
  }
  timer.Done();

  PerfTimeLogger timer2("Cookie_monster_query_single_host");
  for (std::vector<std::string>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    cm->GetCookies(kUrlGoogle);
  }
  timer2.Done();

  PerfTimeLogger timer3("Cookie_monster_deleteall_single_host");
  cm->DeleteAll(false);
  timer3.Done();
}

TEST(CookieMonsterTest, TestAddCookieOnManyHosts) {
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
  std::string cookie(kCookieLine);
  std::vector<GURL> gurls;  // just wanna have ffffuunnn
  for (int i = 0; i < kNumCookies; ++i) {
    gurls.push_back(GURL(StringPrintf("http://a%04d.izzle", i)));
  }

  // Add a cookie on a bunch of host
  PerfTimeLogger timer("Cookie_monster_add_many_hosts");
  for (std::vector<GURL>::const_iterator it = gurls.begin();
       it != gurls.end(); ++it) {
    EXPECT_TRUE(cm->SetCookie(*it, cookie));
  }
  timer.Done();

  PerfTimeLogger timer2("Cookie_monster_query_many_hosts");
  for (std::vector<GURL>::const_iterator it = gurls.begin();
       it != gurls.end(); ++it) {
    cm->GetCookies(*it);
  }
  timer2.Done();

  PerfTimeLogger timer3("Cookie_monster_deleteall_many_hosts");
  cm->DeleteAll(false);
  timer3.Done();
}

TEST(CookieMonsterTest, TestDomainTree) {
  scoped_refptr<net::CookieMonster> cm(new net::CookieMonster(NULL, NULL));
  std::string cookie(kCookieLine);
  std::string domain_base("top.com");

  std::vector<GURL> gurl_list;

  // Create a balanced binary tree of domains on which the cookie is set.
  for (int i1=0; i1 < 2; i1++) {
    std::string domain_base_1((i1 ? "a." : "b.") + domain_base);
    gurl_list.push_back(GURL(StringPrintf("http://%s",
                                          domain_base_1.c_str())));
    for (int i2=0; i2 < 2; i2++) {
      std::string domain_base_2((i1 ? "a." : "b.") + domain_base_1);
      gurl_list.push_back(GURL(StringPrintf("http://%s",
                                            domain_base_2.c_str())));
      for (int i3=0; i3 < 2; i3++) {
        std::string domain_base_3((i1 ? "a." : "b.") + domain_base_2);
        gurl_list.push_back(GURL(StringPrintf("http://%s",
                                              domain_base_3.c_str())));
        for (int i4=0; i4 < 2; i4++) {
          gurl_list.push_back(GURL(StringPrintf("http://%s.%s",
                                                i4 ? "a" : "b",
                                                domain_base_3.c_str())));
        }
      }
    }
  }

  for (std::vector<GURL>::const_iterator it = gurl_list.begin();
       it != gurl_list.end(); it++) {
    EXPECT_TRUE(cm->SetCookie(*it, cookie));
  }

  GURL probe_gurl("http://b.a.b.a.top.com/");
  PerfTimeLogger timer("Cookie_monster_query_domain_tree");
  for (int i = 0; i < kNumCookies; i++) {
    cm->GetCookies(probe_gurl);
  }
  timer.Done();

  cm->DeleteAll(false);
  gurl_list.clear();

  // Create a line of 32 domain cookies such that all cookies stored
  // by effective TLD+1 will apply to probe GURL.
  // (TLD + 1 is the level above .com/org/net/etc, e.g. "top.com"
  // or "google.com".  "Effective" is added to include sites like
  // bbc.co.uk, where the effetive TLD+1 is more than one level
  // below the top level.)
  gurl_list.push_back(GURL("http://a.top.com"));
  gurl_list.push_back(GURL("http://b.a.top.com"));
  gurl_list.push_back(GURL("http://a.b.a.top.com"));
  gurl_list.push_back(GURL("http://b.a.b.a.top.com"));

  for (int i = 0; i < 8; i++) {
    for (std::vector<GURL>::const_iterator it = gurl_list.begin();
         it != gurl_list.end(); it++) {
      EXPECT_TRUE(cm->SetCookie(*it, StringPrintf("a%03d=b", i)));
    }
  }
  PerfTimeLogger timer2("Cookie_monster_query_domain_line");
  for (int i = 0; i < kNumCookies; i++) {
    cm->GetCookies(probe_gurl);
  }
  timer2.Done();
}
