// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/access_token_store.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

namespace {

// The token store factory implementation expects to be used from any well-known
// chrome thread other than UI. We could use any arbitrary thread; IO is
// a good choice as this is the expected usage.
const BrowserThread::ID kExpectedClientThreadId = BrowserThread::IO;
const char* kRefServerUrl1 = "https://test.domain.example/foo?id=bar.bar";
const char* kRefServerUrl2 = "http://another.domain.example/foo?id=bar.bar#2";

class GeolocationAccessTokenStoreTest
    : public InProcessBrowserTest {
 public:
  GeolocationAccessTokenStoreTest()
      : token_to_expect_(NULL), token_to_set_(NULL) {}

  void DoTestStepAndWaitForResults(
      const char* ref_url, const string16* token_to_expect,
      const string16* token_to_set);

  void OnAccessTokenStoresLoaded(
        AccessTokenStore::AccessTokenSet access_token_set);

  scoped_refptr<AccessTokenStore> token_store_;
  CancelableRequestConsumer request_consumer_;
  GURL ref_url_;
  const string16* token_to_expect_;
  const string16* token_to_set_;
};

void StartTestStepFromClientThread(
    scoped_refptr<AccessTokenStore>* store,
    CancelableRequestConsumerBase* consumer,
    AccessTokenStore::LoadAccessTokensCallbackType* callback) {
  ASSERT_TRUE(BrowserThread::CurrentlyOn(kExpectedClientThreadId));
  if (*store == NULL)
    (*store) = NewChromePrefsAccessTokenStore();
  (*store)->LoadAccessTokens(consumer, callback);
}

struct TokenLoadClientForTest {
  void NotReachedCallback(AccessTokenStore::AccessTokenSet /*tokens*/) {
    NOTREACHED() << "This request should have been canceled before callback";
  }
};

void RunCancelTestInClientTread() {
  ASSERT_TRUE(BrowserThread::CurrentlyOn(kExpectedClientThreadId));
  scoped_refptr<AccessTokenStore> store(NewChromePrefsAccessTokenStore());
  CancelableRequestConsumer consumer;
  TokenLoadClientForTest load_client;

  // Single request, canceled explicitly
  CancelableRequestProvider::Handle first_handle =
      store->LoadAccessTokens(&consumer, NewCallback(
          &load_client, &TokenLoadClientForTest::NotReachedCallback));
  EXPECT_TRUE(consumer.HasPendingRequests());
  // Test this handle is valid.
  consumer.GetClientData(store.get(), first_handle);
  store->CancelRequest(first_handle);
  EXPECT_FALSE(consumer.HasPendingRequests());

  // 2 requests, canceled globally.
  store->LoadAccessTokens(&consumer, NewCallback(
      &load_client, &TokenLoadClientForTest::NotReachedCallback));
  store->LoadAccessTokens(&consumer, NewCallback(
      &load_client, &TokenLoadClientForTest::NotReachedCallback));
  EXPECT_TRUE(consumer.HasPendingRequests());
  EXPECT_EQ(2u, consumer.PendingRequestCount());
  consumer.CancelAllRequests();
  EXPECT_FALSE(consumer.HasPendingRequests());
  EXPECT_EQ(0u, consumer.PendingRequestCount());

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, new MessageLoop::QuitTask);
}

void GeolocationAccessTokenStoreTest::DoTestStepAndWaitForResults(
    const char* ref_url, const string16* token_to_expect,
    const string16* token_to_set) {
  ref_url_ = GURL(ref_url);
  token_to_expect_ = token_to_expect;
  token_to_set_ = token_to_set;

  BrowserThread::PostTask(
      kExpectedClientThreadId, FROM_HERE, NewRunnableFunction(
          &StartTestStepFromClientThread, &token_store_, &request_consumer_,
          NewCallback(this,
              &GeolocationAccessTokenStoreTest::OnAccessTokenStoresLoaded)));
  ui_test_utils::RunMessageLoop();
}

void GeolocationAccessTokenStoreTest::OnAccessTokenStoresLoaded(
    AccessTokenStore::AccessTokenSet access_token_set) {
  ASSERT_TRUE(BrowserThread::CurrentlyOn(kExpectedClientThreadId))
      << "Callback from token factory should be from the same thread as the "
         "LoadAccessTokenStores request was made on";
  EXPECT_TRUE(token_to_set_ || token_to_expect_) << "No work to do?";
  AccessTokenStore::AccessTokenSet::const_iterator item =
      access_token_set.find(ref_url_);
  if (!token_to_expect_) {
    EXPECT_TRUE(item == access_token_set.end());
  } else {
    EXPECT_FALSE(item == access_token_set.end());
    EXPECT_EQ(*token_to_expect_, item->second);
  }

  if (token_to_set_) {
    scoped_refptr<AccessTokenStore> store(
        NewChromePrefsAccessTokenStore());
    store->SaveAccessToken(ref_url_, *token_to_set_);
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, new MessageLoop::QuitTask);
}

IN_PROC_BROWSER_TEST_F(GeolocationAccessTokenStoreTest, SetAcrossInstances) {
  const string16 ref_token1 = ASCIIToUTF16("jksdfo90,'s#\"#1*(");
  const string16 ref_token2 = ASCIIToUTF16("\1\2\3\4\5\6\7\10\11\12=023");
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

IN_PROC_BROWSER_TEST_F(GeolocationAccessTokenStoreTest, CancelRequest) {
  BrowserThread::PostTask(
      kExpectedClientThreadId, FROM_HERE, NewRunnableFunction(
          RunCancelTestInClientTread));
  ui_test_utils::RunMessageLoop();
}

}  // namespace
