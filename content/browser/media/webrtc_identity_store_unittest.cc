// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/browser/media/webrtc_identity_store.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// TODO(jiayl): the tests fail on Android since the openssl version of
// CreateSelfSignedCert is not implemented. We should mock out this dependency
// and remove the if-defined.

#if !defined(OS_ANDROID)
class WebRTCIdentityStoreTest : public testing::Test {
 public:
  WebRTCIdentityStoreTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        sequenced_worker_pool_(
            new base::SequencedWorkerPool(3, "WebRTCIdentityStoreTest")),
        webrtc_identity_store_(new WebRTCIdentityStore()) {
    webrtc_identity_store_->SetTaskRunnerForTesting(sequenced_worker_pool_);
  }

  virtual ~WebRTCIdentityStoreTest() { sequenced_worker_pool_->Shutdown(); }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_refptr<base::SequencedWorkerPool> sequenced_worker_pool_;
  scoped_ptr<WebRTCIdentityStore> webrtc_identity_store_;
};

static void OnRequestCompleted(bool* completed,
                        int error,
                        const std::string& certificate,
                        const std::string& private_key) {
  ASSERT_EQ(net::OK, error);
  ASSERT_NE("", certificate);
  ASSERT_NE("", private_key);
  *completed = true;
}

TEST_F(WebRTCIdentityStoreTest, RequestIdentity) {
  bool completed = false;
  base::Closure cancel_callback =
      webrtc_identity_store_->RequestIdentity(
          GURL("http://google.com"),
          "a",
          "b",
          base::Bind(&OnRequestCompleted, &completed));
  ASSERT_FALSE(cancel_callback.is_null());
  sequenced_worker_pool_->FlushForTesting();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(completed);
}

TEST_F(WebRTCIdentityStoreTest, CancelRequest) {
  bool completed = false;
  base::Closure cancel_callback =
      webrtc_identity_store_->RequestIdentity(
          GURL("http://google.com"),
          "a",
          "b",
          base::Bind(&OnRequestCompleted, &completed));
  ASSERT_FALSE(cancel_callback.is_null());
  cancel_callback.Run();
  sequenced_worker_pool_->FlushForTesting();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);
}

TEST_F(WebRTCIdentityStoreTest, MultipleRequests) {
  bool completed_1 = false;
  bool completed_2 = false;
  base::Closure cancel_callback_1 =
    webrtc_identity_store_->RequestIdentity(
        GURL("http://foo.com"),
        "a",
        "b",
        base::Bind(&OnRequestCompleted, &completed_1));
  ASSERT_FALSE(cancel_callback_1.is_null());

  base::Closure cancel_callback_2 =
    webrtc_identity_store_->RequestIdentity(
        GURL("http://bar.com"),
        "a",
        "b",
        base::Bind(&OnRequestCompleted, &completed_2));
  ASSERT_FALSE(cancel_callback_2.is_null());

  sequenced_worker_pool_->FlushForTesting();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(completed_1);
  EXPECT_TRUE(completed_2);
}
#endif
}  // namespace content
