// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/web/public/test/http_server.h"
#include "ios/web/public/test/http_server_util.h"
#include "ios/web/public/test/response_providers/html_response_provider.h"
#include "ios/web/public/test/response_providers/response_provider.h"
#include "url/gurl.h"

namespace {

const char kTestUrlNormalBrowsing[] = "http://choux/normal/browsing";
const char kTestUrlNormalSetCookie[] = "http://choux/normal/set_cookie";
const char kTestUrlIncognitoBrowsing[] = "http://choux/incognito/browsing";
const char kTestUrlIncognitoSetCookie[] = "http://choux/incognito/set_cookie";

const char kTestResponse[] = "fleur";
NSString* const kNoCookieText = @"No cookies!";
NSString* const kNormalCookieKey = @"request";
NSString* const kNormalCookieValue = @"pony";
NSString* const kIncognitoCookieKey = @"secret";
NSString* const kIncognitoCookieValue = @"rainbow";

NSString* const kGetCookieScript =
    @"var c = document.cookie ? document.cookie.split(/;\\s*/) : [];"
     "if (!c.length) {"
     "  document.documentElement.innerHTML = 'No cookies!';"
     "} else {"
     "  document.documentElement.innerHTML = 'OK: ' + c.join(',');"
     "}";

// Checks that a cookie of key=value is the only cookie exists in current domain
// in current web state.
// Returns true if cookie |key| of value |value| is the only cookie that exists.
bool CheckCookieExists(NSString* key, NSString* value) {
  NSError* error = nil;
  id result = chrome_test_util::ExecuteJavaScript(kGetCookieScript, &error);
  if (error)
    return false;
  NSString* stringResult = base::mac::ObjCCastStrict<NSString>(result);
  NSString* expectedText = [NSString stringWithFormat:@"OK: %@=%@", key, value];
  return [stringResult isEqualToString:expectedText];
}

// Checks that there is no cookie in current domain in current web state.
bool CheckNoCookieExists() {
  NSError* error = nil;
  id result = chrome_test_util::ExecuteJavaScript(kGetCookieScript, &error);
  if (error)
    return false;
  NSString* stringResult = base::mac::ObjCCastStrict<NSString>(result);
  return [stringResult isEqualToString:kNoCookieText];
}

}  // namespace

@interface CookiesTestCase : ChromeTestCase
@end

@implementation CookiesTestCase

#pragma mark - Overrides superclass

+ (void)setUp {
  [super setUp];
  // Creates a map of canned responses and set up the test HTML server.
  // |kTestUrlNormalSetCookie| and |kTestUrlIncognitoSetCookie| always sets
  // cookie in response header while |kTestUrlNormalBrowsing| and
  // |kTestUrlIncognitoBrowsing| doesn't.
  std::map<GURL, std::pair<std::string, std::string>> responses;

  NSString* normalCookie = [NSString
      stringWithFormat:@"%@=%@", kNormalCookieKey, kNormalCookieValue];
  NSString* incognitoCookie = [NSString
      stringWithFormat:@"%@=%@", kIncognitoCookieKey, kIncognitoCookieValue];

  responses[web::test::HttpServer::MakeUrl(kTestUrlNormalBrowsing)] =
      std::pair<std::string, std::string>("", kTestResponse);
  responses[web::test::HttpServer::MakeUrl(kTestUrlNormalSetCookie)] =
      std::pair<std::string, std::string>(base::SysNSStringToUTF8(normalCookie),
                                          kTestResponse);
  responses[web::test::HttpServer::MakeUrl(kTestUrlIncognitoBrowsing)] =
      std::pair<std::string, std::string>("", kTestResponse);
  responses[web::test::HttpServer::MakeUrl(kTestUrlIncognitoSetCookie)] =
      std::pair<std::string, std::string>(
          base::SysNSStringToUTF8(incognitoCookie), kTestResponse);

  web::test::SetUpSimpleHttpServerWithSetCookies(responses);
}

// Clear cookies to make sure that tests do not interfere each other.
// TODO(crbug.com/638674): Evaluate if this can move to shared code.
- (void)tearDown {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalBrowsing)];
  NSString* const clearCookieScript =
      @"var cookies = document.cookie.split(';');"
       "for (var i = 0; i < cookies.length; i++) {"
       "  var cookie = cookies[i];"
       "  var eqPos = cookie.indexOf('=');"
       "  var name = eqPos > -1 ? cookie.substr(0, eqPos) : cookie;"
       "  document.cookie = name + '=;expires=Thu, 01 Jan 1970 00:00:00 GMT';"
       "}";
  NSError* error = nil;
  chrome_test_util::ExecuteJavaScript(clearCookieScript, &error);
  [super tearDown];
}

#pragma mark - Tests

// Tests toggling between Normal tabs and Incognito tabs. Different cookies
// (request=pony for normal tabs, secret=rainbow for incognito tabs) are set.
// The goal is to verify that cookies set in incognito tabs are available in
// incognito tabs but not available in normal tabs. Cookies set in incognito
// tabs are also deleted when all incognito tabs are closed.
- (void)testClearIncognitoFromMain {
  // Loads a dummy page in normal tab. Sets a normal test cookie. Verifies that
  // the incognito test cookie is not found.
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalSetCookie)];
  GREYAssertTrue(CheckCookieExists(kNormalCookieKey, kNormalCookieValue),
                 @"Failed to set normal cookie in normal mode. (1)");
  GREYAssertFalse(CheckCookieExists(kIncognitoCookieKey, kIncognitoCookieValue),
                  @"Incognito cookie should not be found in normal mode. (1)");

  // Opens an incognito tab, loads the dummy page, and sets incognito test
  // cookie.
  chrome_test_util::OpenNewIncognitoTab();
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlIncognitoSetCookie)];
  GREYAssertTrue(CheckCookieExists(kIncognitoCookieKey, kIncognitoCookieValue),
                 @"Failed to set incognito cookie in incognito mode. (2)");

  // Switches back to normal profile by opening up a new tab. Test cookie
  // should not be found.
  chrome_test_util::OpenNewTab();
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalBrowsing)];
  GREYAssertTrue(CheckCookieExists(kNormalCookieKey, kNormalCookieValue),
                 @"Normal cookie should be found in normal mode. (3)");
  GREYAssertFalse(CheckCookieExists(kIncognitoCookieKey, kIncognitoCookieValue),
                  @"Incognito cookie should not be found in normal mode. (3)");

  // Finally, closes all incognito tabs while still in normal tab.
  // Checks that incognito cookie is gone.
  chrome_test_util::CloseAllIncognitoTabs();
  chrome_test_util::OpenNewTab();
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalBrowsing)];
  GREYAssertTrue(CheckCookieExists(kNormalCookieKey, kNormalCookieValue),
                 @"Normal cookie should be found in normal mode. (4)");
  GREYAssertFalse(CheckCookieExists(kIncognitoCookieKey, kIncognitoCookieValue),
                  @"Incognito cookie should be gone from normal mode. (4)");
}

// Tests that a cookie set in incognito tab is removed after closing all
// incognito tabs and then when new incognito tab is created the cookie will
// not reappear.
- (void)testClearIncognitoFromIncognito {
  // Loads a page in normal tab.
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalBrowsing)];

  // Opens an incognito tab, loads a page, and sets an incognito cookie.
  chrome_test_util::OpenNewIncognitoTab();
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlIncognitoSetCookie)];
  GREYAssertTrue(CheckCookieExists(kIncognitoCookieKey, kIncognitoCookieValue),
                 @"Failed to set incognito cookie in incognito mode. (1)");

  // Closes all incognito tabs and switch back to a normal tab.
  chrome_test_util::CloseAllIncognitoTabs();
  chrome_test_util::OpenNewTab();
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalBrowsing)];

  // Opens a new incognito tab and verify that the previously set cookie
  // is no longer there.
  chrome_test_util::OpenNewIncognitoTab();
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlIncognitoBrowsing)];
  GREYAssertTrue(CheckNoCookieExists(),
                 @"Incognito cookie should be gone from incognito mode. (2)");

  // Verifies that new incognito cookies can be set.
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlIncognitoSetCookie)];
  GREYAssertTrue(CheckCookieExists(kIncognitoCookieKey, kIncognitoCookieValue),
                 @"Failed to set incognito cookie in incognito mode. (3)");
}

// Tests that a cookie set in normal tab is not available in an incognito tab.
- (void)testSwitchToIncognito {
  // Sets cookie in normal tab.
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalSetCookie)];
  GREYAssertTrue(CheckCookieExists(kNormalCookieKey, kNormalCookieValue),
                 @"Failed to set normal cookie in normal mode. (1)");

  // Switches to a new incognito tab and verifies that cookie is not there.
  chrome_test_util::OpenNewIncognitoTab();
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlIncognitoBrowsing)];
  GREYAssertTrue(CheckNoCookieExists(),
                 @"Normal cookie should not be found in incognito mode. (2)");

  // Closes all incognito tabs and then switching back to a normal tab. Verifies
  // that the cookie set earlier is still there.
  chrome_test_util::CloseAllIncognitoTabs();
  chrome_test_util::OpenNewTab();
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalBrowsing)];
  GREYAssertTrue(CheckCookieExists(kNormalCookieKey, kNormalCookieValue),
                 @"Normal cookie should still be found in normal mode. (3)");
}

// Tests that a cookie set in incognito tab is only available in another
// incognito tab. They are not available in a normal tab.
- (void)testSwitchToMain {
  // Loads a page in normal tab and then switches to a new incognito tab. Sets
  // cookie in incognito tab.
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalBrowsing)];
  chrome_test_util::OpenNewIncognitoTab();
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlIncognitoSetCookie)];
  GREYAssertTrue(CheckCookieExists(kIncognitoCookieKey, kIncognitoCookieValue),
                 @"Failed to set incognito cookie in incognito mode. (1)");
  // Switches back to a normal tab and verifies that cookie set in incognito tab
  // is not available.
  chrome_test_util::OpenNewTab();
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalBrowsing)];
  GREYAssertTrue(CheckNoCookieExists(),
                 @"Incognito cookie should not be found in normal mode. (2)");
  // Returns back to Incognito tab and cookie is still there.
  chrome_test_util::OpenNewIncognitoTab();
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlIncognitoBrowsing)];
  GREYAssertTrue(CheckCookieExists(kIncognitoCookieKey, kIncognitoCookieValue),
                 @"Incognito cookie should be found in incognito mode. (3)");
}

// Tests that a cookie set in a normal tab can be found in another normal tab.
- (void)testShareCookiesBetweenTabs {
  // Loads page and sets cookie in first normal tab.
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalSetCookie)];
  GREYAssertTrue(CheckCookieExists(kNormalCookieKey, kNormalCookieValue),
                 @"Failed to set normal cookie in normal mode. (1)");

  // Creates another normal tab and verifies that the cookie is also there.
  chrome_test_util::OpenNewTab();
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalBrowsing)];
  GREYAssertTrue(CheckCookieExists(kNormalCookieKey, kNormalCookieValue),
                 @"Normal cookie should still be found in normal mode. (2)");
}

@end
