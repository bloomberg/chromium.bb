// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/access_token_store.h"

#include "chrome/browser/chrome_thread.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

namespace {
// The token store factory implementation expects to be used from any well-known
// chrome thread other than UI. We could use any arbitrary thread; IO is
// a good choice as this is the expected usage.
const ChromeThread::ID kExpectedClientThreadId = ChromeThread::IO;
const char* kRefServerUrl1 = "https://test.domain.example/foo?id=bar.bar";
const char* kRefServerUrl2 = "http://another.domain.example/foo?id=bar.bar#2";
}  // namespace

class GeolocationAccessTokenStoreTest
    : public InProcessBrowserTest,
      public AccessTokenStoreFactory::Delegate,
      public base::SupportsWeakPtr<GeolocationAccessTokenStoreTest> {
 protected:
  GeolocationAccessTokenStoreTest()
      : token_to_expect_(NULL), token_to_set_(NULL) {}

  void StartThreadAndWaitForResults(
      const char* ref_url, const string16* token_to_expect,
      const string16* token_to_set);

  // AccessTokenStoreFactory::Delegate
  virtual void OnAccessTokenStoresCreated(
        const AccessTokenStoreFactory::TokenStoreSet& access_token_store);
  GURL ref_url_;
  const string16* token_to_expect_;
  const string16* token_to_set_;
};

namespace {
// A WeakPtr may only be used on the thread in which it is created, hence we
// defer the call to delegate->AsWeakPtr() into this function rather than pass
// WeakPtr& in.
void StartTestFromClientThread(
    GeolocationAccessTokenStoreTest* delegate,
    const GURL& ref_url) {
  ASSERT_TRUE(ChromeThread::CurrentlyOn(kExpectedClientThreadId));

  scoped_refptr<AccessTokenStoreFactory> store =
      NewChromePrefsAccessTokenStoreFactory();
  store->CreateAccessTokenStores(delegate->AsWeakPtr(), ref_url);
}
}  // namespace

void GeolocationAccessTokenStoreTest::StartThreadAndWaitForResults(
    const char* ref_url, const string16* token_to_expect,
    const string16* token_to_set) {
  ref_url_ = GURL(ref_url);
  token_to_expect_ = token_to_expect;
  token_to_set_ = token_to_set;

  ChromeThread::PostTask(
      kExpectedClientThreadId, FROM_HERE, NewRunnableFunction(
          &StartTestFromClientThread, this, ref_url_));
  ui_test_utils::RunMessageLoop();
}

void GeolocationAccessTokenStoreTest::OnAccessTokenStoresCreated(
      const AccessTokenStoreFactory::TokenStoreSet& access_token_store) {
  ASSERT_TRUE(ChromeThread::CurrentlyOn(kExpectedClientThreadId))
      << "Callback from token factory should be from the same thread as the "
         "CreateAccessTokenStores request was made on";
  EXPECT_TRUE(token_to_set_ || token_to_expect_) << "No work to do?";
  DCHECK_GE(access_token_store.size(), size_t(1));

  AccessTokenStoreFactory::TokenStoreSet::const_iterator item =
      access_token_store.find(ref_url_);
  ASSERT_TRUE(item != access_token_store.end());
  scoped_refptr<AccessTokenStore> store = item->second;
  ASSERT_TRUE(NULL != store);
  string16 token;
  bool read_ok = store->GetAccessToken(&token);
  if (!token_to_expect_) {
    EXPECT_FALSE(read_ok);
    EXPECT_TRUE(token.empty());
  } else {
    ASSERT_TRUE(read_ok);
    EXPECT_EQ(*token_to_expect_, token);
  }

  if (token_to_set_) {
    store->SetAccessToken(*token_to_set_);
  }
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE, new MessageLoop::QuitTask);
}

#if defined(OS_LINUX)
// TODO(joth): http://crbug.com/36068 crashes on Linux.
#define MAYBE_SetAcrossInstances DISABLED_SetAcrossInstances
#else
#define MAYBE_SetAcrossInstances SetAcrossInstances
#endif

IN_PROC_BROWSER_TEST_F(GeolocationAccessTokenStoreTest, MAYBE_SetAcrossInstances) {
  const string16 ref_token1 = ASCIIToUTF16("jksdfo90,'s#\"#1*(");
  const string16 ref_token2 = ASCIIToUTF16("\1\2\3\4\5\6\7\10\11\12=023");
  ASSERT_TRUE(ChromeThread::CurrentlyOn(ChromeThread::UI));

  StartThreadAndWaitForResults(kRefServerUrl1, NULL, &ref_token1);
  // Check it was set, and change to new value.
  StartThreadAndWaitForResults(kRefServerUrl1, &ref_token1, &ref_token2);
  // And change back.
  StartThreadAndWaitForResults(kRefServerUrl1, &ref_token2, &ref_token1);
  StartThreadAndWaitForResults(kRefServerUrl1, &ref_token1, NULL);

  // Set a second server URL
  StartThreadAndWaitForResults(kRefServerUrl2, NULL, &ref_token2);
  StartThreadAndWaitForResults(kRefServerUrl2, &ref_token2, NULL);
  StartThreadAndWaitForResults(kRefServerUrl1, &ref_token1, NULL);
}
