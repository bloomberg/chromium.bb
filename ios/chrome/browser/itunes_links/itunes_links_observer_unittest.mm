// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/itunes_links/itunes_links_observer.h"

#import "base/mac/scoped_nsobject.h"
#import "ios/web/public/test/test_web_state.h"
#import "ios/chrome/browser/storekit_launcher.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "url/gurl.h"

namespace {

class ITunesLinksObserverTest : public PlatformTest {
 protected:
  void SetUp() override {
    mocked_store_kit_launcher_.reset(
        [[OCMockObject mockForProtocol:@protocol(StoreKitLauncher)] retain]);
    link_observer_.reset(
        [[ITunesLinksObserver alloc] initWithWebState:&web_state_]);
    [link_observer_ setStoreKitLauncher:mocked_store_kit_launcher_];
  }
  void VerifyOpeningOfAppStore(NSString* expected_product_id,
                               std::string const& url_string) {
    if (expected_product_id)
      [[mocked_store_kit_launcher_.get() expect]
          openAppStore:expected_product_id];
    web_state_.SetCurrentURL(GURL(url_string));
    [link_observer_ webStateDidLoadPage:&web_state_];
    EXPECT_OCMOCK_VERIFY(mocked_store_kit_launcher_.get());
  }
  web::TestWebState web_state_;
  base::scoped_nsobject<id> mocked_store_kit_launcher_;
  // |link_observer_| must be destroyed before web_state_.
  base::scoped_nsobject<ITunesLinksObserver> link_observer_;
};

// Verifies that navigating to URLs not concerning a product hosted on the
// iTunes AppStore does not trigger the opening of the AppStore.
TEST_F(ITunesLinksObserverTest, NonMatchingUrls) {
  VerifyOpeningOfAppStore(nil, "");
  VerifyOpeningOfAppStore(nil, "foobar");
  VerifyOpeningOfAppStore(nil, "foo://bar");
  VerifyOpeningOfAppStore(nil, "http://foo");
  VerifyOpeningOfAppStore(nil, "http://foo?bar#qux");
  VerifyOpeningOfAppStore(nil, "http://foo.bar/qux");
  VerifyOpeningOfAppStore(nil, "http://itunes.apple.com");
  VerifyOpeningOfAppStore(nil, "http://itunes.apple.com/id");
  VerifyOpeningOfAppStore(nil, "http://itunes.apple.com/12345");
  VerifyOpeningOfAppStore(nil, "ftp://itunes.apple.com/id123");
}

// Verifies that navigating to URLs concerning a product hosted on iTunes
// AppStore triggers the opening of the AppStore.
TEST_F(ITunesLinksObserverTest, MatchingUrls) {
  VerifyOpeningOfAppStore(@"123", "http://itunes.apple.com/id123");
  VerifyOpeningOfAppStore(@"123", "https://itunes.apple.com/id123");
  VerifyOpeningOfAppStore(@"123", "http://itunes.apple.com/bar/id123");
  VerifyOpeningOfAppStore(@"123", "http://itunes.apple.com/bar/id123?qux");
  VerifyOpeningOfAppStore(@"123", "http://itunes.apple.com/bar/id123?qux&baz");
  VerifyOpeningOfAppStore(@"123",
                          "http://itunes.apple.com/bar/id123?qux&baz#foo");
  VerifyOpeningOfAppStore(@"123",
                          "http://foo.itunes.apple.com/bar/id123?qux&baz#foo");
}

}  // namespace
