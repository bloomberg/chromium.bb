// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/cocoa/cookie_details.h"
#include "googleurl/src/gurl.h"

namespace {

class CookiesDetailsTest : public CocoaTest {
};

TEST_F(CookiesDetailsTest, CreateForFolder) {
  scoped_nsobject<CocoaCookieDetails> details;
  details.reset([[CocoaCookieDetails alloc] initAsFolder]);

  EXPECT_EQ([details.get() type], kCocoaCookieDetailsTypeFolder);
}

TEST_F(CookiesDetailsTest, CreateForCookie) {
  scoped_nsobject<CocoaCookieDetails> details;
  GURL url("http://chromium.org");
  std::string cookieLine(
      "PHPSESSID=0123456789abcdef0123456789abcdef; path=/");
  net::CookieMonster::ParsedCookie pc(cookieLine);
  net::CookieMonster::CanonicalCookie cookie(url, pc);
  NSString* origin = base::SysUTF8ToNSString("http://chromium.org");
  details.reset([[CocoaCookieDetails alloc] initWithCookie:&cookie
                                                    origin:origin
                                         canEditExpiration:NO]);

  EXPECT_EQ([details.get() type], kCocoaCookieDetailsTypeCookie);
  EXPECT_TRUE([@"PHPSESSID" isEqualToString:[details.get() name]]);
  EXPECT_TRUE([@"0123456789abcdef0123456789abcdef"
      isEqualToString:[details.get() content]]);
  EXPECT_TRUE([@"http://chromium.org" isEqualToString:[details.get() domain]]);
  EXPECT_TRUE([@"/" isEqualToString:[details.get() path]]);
  EXPECT_FALSE([@"" isEqualToString:[details.get() lastModified]]);
  EXPECT_FALSE([@"" isEqualToString:[details.get() created]]);
  EXPECT_FALSE([@"" isEqualToString:[details.get() sendFor]]);

  EXPECT_FALSE([details.get() shouldHideCookieDetailsView]);
  EXPECT_FALSE([details.get() shouldShowLocalStorageTreeDetailsView]);
  EXPECT_FALSE([details.get() shouldShowDatabaseTreeDetailsView]);
  EXPECT_FALSE([details.get() shouldShowAppCacheTreeDetailsView]);
  EXPECT_FALSE([details.get() shouldShowLocalStoragePromptDetailsView]);
  EXPECT_FALSE([details.get() shouldShowDatabasePromptDetailsView]);
  EXPECT_FALSE([details.get() shouldShowAppCachePromptDetailsView]);
}

TEST_F(CookiesDetailsTest, CreateForTreeDatabase) {
  scoped_nsobject<CocoaCookieDetails> details;
  std::string host("http://chromium.org");
  std::string database_name("sassolungo");
  std::string origin_identifier("dolomites");
  std::string description("a great place to climb");
  int64 size = 1234;
  base::Time last_modified = base::Time::Now();
  BrowsingDataDatabaseHelper::DatabaseInfo info(host, database_name,
      origin_identifier, description, host, size, last_modified);
  details.reset([[CocoaCookieDetails alloc] initWithDatabase:&info]);

  EXPECT_EQ([details.get() type], kCocoaCookieDetailsTypeTreeDatabase);
  EXPECT_TRUE([@"a great place to climb" isEqualToString:[details.get()
      databaseDescription]]);
  EXPECT_TRUE([@"1234 B" isEqualToString:[details.get() fileSize]]);
  EXPECT_FALSE([@"" isEqualToString:[details.get() lastModified]]);

  EXPECT_TRUE([details.get() shouldHideCookieDetailsView]);
  EXPECT_FALSE([details.get() shouldShowLocalStorageTreeDetailsView]);
  EXPECT_TRUE([details.get() shouldShowDatabaseTreeDetailsView]);
  EXPECT_FALSE([details.get() shouldShowAppCacheTreeDetailsView]);
  EXPECT_FALSE([details.get() shouldShowLocalStoragePromptDetailsView]);
  EXPECT_FALSE([details.get() shouldShowDatabasePromptDetailsView]);
  EXPECT_FALSE([details.get() shouldShowAppCachePromptDetailsView]);
}

TEST_F(CookiesDetailsTest, CreateForTreeLocalStorage) {
  scoped_nsobject<CocoaCookieDetails> details;
  std::string protocol("http");
  std::string host("chromium.org");
  unsigned short port = 80;
  std::string database_identifier("id");
  std::string origin("chromium.org");
  FilePath file_path(FilePath::FromWStringHack(std::wstring(L"/")));
  int64 size = 1234;
  base::Time last_modified = base::Time::Now();
  BrowsingDataLocalStorageHelper::LocalStorageInfo info(protocol, host, port,
      database_identifier, origin, file_path, size, last_modified);
  details.reset([[CocoaCookieDetails alloc] initWithLocalStorage:&info]);

  EXPECT_EQ([details.get() type], kCocoaCookieDetailsTypeTreeLocalStorage);
  EXPECT_TRUE([@"chromium.org" isEqualToString:[details.get() domain]]);
  EXPECT_TRUE([@"1234 B" isEqualToString:[details.get() fileSize]]);
  EXPECT_FALSE([@"" isEqualToString:[details.get() lastModified]]);

  EXPECT_TRUE([details.get() shouldHideCookieDetailsView]);
  EXPECT_TRUE([details.get() shouldShowLocalStorageTreeDetailsView]);
  EXPECT_FALSE([details.get() shouldShowDatabaseTreeDetailsView]);
  EXPECT_FALSE([details.get() shouldShowAppCacheTreeDetailsView]);
  EXPECT_FALSE([details.get() shouldShowLocalStoragePromptDetailsView]);
  EXPECT_FALSE([details.get() shouldShowDatabasePromptDetailsView]);
  EXPECT_FALSE([details.get() shouldShowAppCachePromptDetailsView]);
}

TEST_F(CookiesDetailsTest, CreateForTreeAppCache) {
  scoped_nsobject<CocoaCookieDetails> details;

  GURL url("http://chromium.org/stuff.manifest");
  appcache::AppCacheInfo info;
  info.creation_time = base::Time::Now();
  info.last_update_time = base::Time::Now();
  info.last_access_time = base::Time::Now();
  info.size=2678;
  info.manifest_url = url;
  details.reset([[CocoaCookieDetails alloc] initWithAppCacheInfo:&info]);

  EXPECT_EQ([details.get() type], kCocoaCookieDetailsTypeTreeAppCache);
  EXPECT_TRUE([@"http://chromium.org/stuff.manifest"
      isEqualToString:[details.get() manifestURL]]);
  EXPECT_TRUE([@"2678 B" isEqualToString:[details.get() fileSize]]);
  EXPECT_FALSE([@"" isEqualToString:[details.get() lastAccessed]]);
  EXPECT_FALSE([@"" isEqualToString:[details.get() created]]);

  EXPECT_TRUE([details.get() shouldHideCookieDetailsView]);
  EXPECT_FALSE([details.get() shouldShowLocalStorageTreeDetailsView]);
  EXPECT_FALSE([details.get() shouldShowDatabaseTreeDetailsView]);
  EXPECT_TRUE([details.get() shouldShowAppCacheTreeDetailsView]);
  EXPECT_FALSE([details.get() shouldShowLocalStoragePromptDetailsView]);
  EXPECT_FALSE([details.get() shouldShowDatabasePromptDetailsView]);
  EXPECT_FALSE([details.get() shouldShowAppCachePromptDetailsView]);
}

TEST_F(CookiesDetailsTest, CreateForPromptDatabase) {
  scoped_nsobject<CocoaCookieDetails> details;
  std::string domain("chromium.org");
  string16 name(base::SysNSStringToUTF16(@"wicked_name"));
  string16 desc(base::SysNSStringToUTF16(@"desc"));
  details.reset([[CocoaCookieDetails alloc] initWithDatabase:domain
                                                databaseName:name
                                         databaseDescription:desc
                                                    fileSize:94]);

  EXPECT_EQ([details.get() type], kCocoaCookieDetailsTypePromptDatabase);
  EXPECT_TRUE([@"chromium.org" isEqualToString:[details.get() domain]]);
  EXPECT_TRUE([@"wicked_name" isEqualToString:[details.get() name]]);
  EXPECT_TRUE([@"desc" isEqualToString:[details.get() databaseDescription]]);
  EXPECT_TRUE([@"94 B" isEqualToString:[details.get() fileSize]]);

  EXPECT_TRUE([details.get() shouldHideCookieDetailsView]);
  EXPECT_FALSE([details.get() shouldShowLocalStorageTreeDetailsView]);
  EXPECT_FALSE([details.get() shouldShowDatabaseTreeDetailsView]);
  EXPECT_FALSE([details.get() shouldShowAppCacheTreeDetailsView]);
  EXPECT_FALSE([details.get() shouldShowLocalStoragePromptDetailsView]);
  EXPECT_TRUE([details.get() shouldShowDatabasePromptDetailsView]);
  EXPECT_FALSE([details.get() shouldShowAppCachePromptDetailsView]);
}

TEST_F(CookiesDetailsTest, CreateForPromptLocalStorage) {
  scoped_nsobject<CocoaCookieDetails> details;
  std::string domain("chromium.org");
  string16 key(base::SysNSStringToUTF16(@"testKey"));
  string16 value(base::SysNSStringToUTF16(@"testValue"));
  details.reset([[CocoaCookieDetails alloc] initWithLocalStorage:domain
                                                             key:key
                                                           value:value]);

  EXPECT_EQ([details.get() type], kCocoaCookieDetailsTypePromptLocalStorage);
  EXPECT_TRUE([@"chromium.org" isEqualToString:[details.get() domain]]);
  EXPECT_TRUE([@"testKey" isEqualToString:[details.get() localStorageKey]]);
  EXPECT_TRUE([@"testValue" isEqualToString:[details.get() localStorageValue]]);

  EXPECT_TRUE([details.get() shouldHideCookieDetailsView]);
  EXPECT_FALSE([details.get() shouldShowLocalStorageTreeDetailsView]);
  EXPECT_FALSE([details.get() shouldShowDatabaseTreeDetailsView]);
  EXPECT_FALSE([details.get() shouldShowAppCacheTreeDetailsView]);
  EXPECT_TRUE([details.get() shouldShowLocalStoragePromptDetailsView]);
  EXPECT_FALSE([details.get() shouldShowDatabasePromptDetailsView]);
  EXPECT_FALSE([details.get() shouldShowAppCachePromptDetailsView]);
}

TEST_F(CookiesDetailsTest, CreateForPromptAppCache) {
  scoped_nsobject<CocoaCookieDetails> details;
  std::string manifestURL("http://html5demos.com/html5demo.manifest");
  details.reset([[CocoaCookieDetails alloc]
      initWithAppCacheManifestURL:manifestURL.c_str()]);

  EXPECT_EQ([details.get() type], kCocoaCookieDetailsTypePromptAppCache);
  EXPECT_TRUE([@"http://html5demos.com/html5demo.manifest"
      isEqualToString:[details.get() manifestURL]]);

  EXPECT_TRUE([details.get() shouldHideCookieDetailsView]);
  EXPECT_FALSE([details.get() shouldShowLocalStorageTreeDetailsView]);
  EXPECT_FALSE([details.get() shouldShowDatabaseTreeDetailsView]);
  EXPECT_FALSE([details.get() shouldShowAppCacheTreeDetailsView]);
  EXPECT_FALSE([details.get() shouldShowLocalStoragePromptDetailsView]);
  EXPECT_FALSE([details.get() shouldShowDatabasePromptDetailsView]);
  EXPECT_TRUE([details.get() shouldShowAppCachePromptDetailsView]);
}

}
