// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/geolocation/chrome_access_token_store.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/access_token_store.h"
#include "content/public/test/test_browser_thread.h"

using content::AccessTokenStore;
using content::BrowserThread;

namespace {

// The token store factory implementation expects to be used from any well-known
// chrome thread other than UI. We could use any arbitrary thread; IO is
// a good choice as this is the expected usage.
const BrowserThread::ID kExpectedClientThreadId = BrowserThread::IO;
const char* kRefServerUrl1 = "https://test.domain.example/foo?id=bar.bar";
const char* kRefServerUrl2 = "http://another.domain.example/foo?id=bar.bar#2";
const char* kOldDefaultNetworkProviderUrl = "https://www.google.com/loc/json";

class GeolocationAccessTokenStoreTest
    : public InProcessBrowserTest {
 public:
  GeolocationAccessTokenStoreTest()
      : token_to_expect_(NULL), token_to_set_(NULL) {}

  void DoTestStepAndWaitForResults(
      const char* ref_url, const base::string16* token_to_expect,
      const base::string16* token_to_set);

  void OnAccessTokenStoresLoaded(
      AccessTokenStore::AccessTokenSet access_token_set,
      net::URLRequestContextGetter* context_getter);

  scoped_refptr<AccessTokenStore> token_store_;
  GURL ref_url_;
  const base::string16* token_to_expect_;
  const base::string16* token_to_set_;
};

void StartTestStepFromClientThread(
    scoped_refptr<AccessTokenStore>* store,
    const AccessTokenStore::LoadAccessTokensCallbackType& callback) {
  ASSERT_TRUE(BrowserThread::CurrentlyOn(kExpectedClientThreadId));
  if (store->get() == NULL)
    (*store) = new ChromeAccessTokenStore();
  (*store)->LoadAccessTokens(callback);
}

struct TokenLoadClientForTest {
  void NotReachedCallback(AccessTokenStore::AccessTokenSet /*tokens*/,
                          net::URLRequestContextGetter* /*context_getter*/) {
    NOTREACHED() << "This request should have been canceled before callback";
  }
};

void GeolocationAccessTokenStoreTest::DoTestStepAndWaitForResults(
    const char* ref_url, const base::string16* token_to_expect,
    const base::string16* token_to_set) {
  ref_url_ = GURL(ref_url);
  token_to_expect_ = token_to_expect;
  token_to_set_ = token_to_set;

  BrowserThread::PostTask(
      kExpectedClientThreadId, FROM_HERE,
      base::Bind(
          &StartTestStepFromClientThread,
          &token_store_,
          base::Bind(
              &GeolocationAccessTokenStoreTest::OnAccessTokenStoresLoaded,
              base::Unretained(this))));
  content::RunMessageLoop();
}

void GeolocationAccessTokenStoreTest::OnAccessTokenStoresLoaded(
    AccessTokenStore::AccessTokenSet access_token_set,
    net::URLRequestContextGetter* context_getter) {
  ASSERT_TRUE(BrowserThread::CurrentlyOn(kExpectedClientThreadId))
      << "Callback from token factory should be from the same thread as the "
         "LoadAccessTokenStores request was made on";
  DCHECK(context_getter);
  AccessTokenStore::AccessTokenSet::const_iterator item =
      access_token_set.find(ref_url_);
  if (!token_to_expect_) {
    EXPECT_TRUE(item == access_token_set.end());
  } else {
    EXPECT_FALSE(item == access_token_set.end());
    EXPECT_EQ(*token_to_expect_, item->second);
  }

  if (token_to_set_) {
    scoped_refptr<AccessTokenStore> store(new ChromeAccessTokenStore());
    store->SaveAccessToken(ref_url_, *token_to_set_);
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::MessageLoop::QuitClosure());
}

IN_PROC_BROWSER_TEST_F(GeolocationAccessTokenStoreTest, SetAcrossInstances) {
  const base::string16 ref_token1 = base::ASCIIToUTF16("jksdfo90,'s#\"#1*(");
  const base::string16 ref_token2 =
      base::ASCIIToUTF16("\1\2\3\4\5\6\7\10\11\12=023");
  ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DoTestStepAndWaitForResults(kRefServerUrl1, NULL, &ref_token1);
  // Check it was set, and change to new value.
  DoTestStepAndWaitForResults(kRefServerUrl1, &ref_token1, &ref_token2);
  // And change back.
  DoTestStepAndWaitForResults(kRefServerUrl1, &ref_token2, &ref_token1);
  DoTestStepAndWaitForResults(kRefServerUrl1, &ref_token1, NULL);

  // Set a second server URL
  DoTestStepAndWaitForResults(kRefServerUrl2, NULL, &ref_token2);
  DoTestStepAndWaitForResults(kRefServerUrl2, &ref_token2, NULL);
  DoTestStepAndWaitForResults(kRefServerUrl1, &ref_token1, NULL);
}

IN_PROC_BROWSER_TEST_F(GeolocationAccessTokenStoreTest, OldUrlRemoval) {
  const base::string16 ref_token1 = base::ASCIIToUTF16("jksdfo90,'s#\"#1*(");
  ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Set a token for the old default network provider url.
  DoTestStepAndWaitForResults(kOldDefaultNetworkProviderUrl,
                              NULL, &ref_token1);
  // Check that the token related to the old default network provider url
  // was deleted.
  DoTestStepAndWaitForResults(kOldDefaultNetworkProviderUrl,
                              NULL, NULL);
}

}  // namespace
