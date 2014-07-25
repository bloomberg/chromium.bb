// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_channel_id_helper.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/ssl/channel_id_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class BrowsingDataChannelIDHelperTest
    : public testing::Test,
      public net::SSLConfigService::Observer {
 public:
  BrowsingDataChannelIDHelperTest() : ssl_config_changed_count_(0) {
  }

  virtual void SetUp() OVERRIDE {
    testing_profile_.reset(new TestingProfile());

    testing_profile_->GetSSLConfigService()->AddObserver(this);
  }

  virtual void TearDown() OVERRIDE {
    testing_profile_->GetSSLConfigService()->RemoveObserver(this);
  }

  void CreateChannelIDsForTest() {
    net::URLRequestContext* context =
        testing_profile_->GetRequestContext()->GetURLRequestContext();
    net::ChannelIDStore* channel_id_store =
        context->channel_id_service()->GetChannelIDStore();
    channel_id_store->SetChannelID("https://www.google.com:443",
                                   base::Time(), base::Time(),
                                   "key", "cert");
    channel_id_store->SetChannelID("https://www.youtube.com:443",
                                   base::Time(), base::Time(),
                                   "key", "cert");
  }

  void FetchCallback(
      const net::ChannelIDStore::ChannelIDList& channel_ids) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    channel_id_list_ = channel_ids;
  }

  // net::SSLConfigService::Observer implementation:
  virtual void OnSSLConfigChanged() OVERRIDE {
    ssl_config_changed_count_++;
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> testing_profile_;

  net::ChannelIDStore::ChannelIDList channel_id_list_;

  int ssl_config_changed_count_;
};

TEST_F(BrowsingDataChannelIDHelperTest, FetchData) {
  CreateChannelIDsForTest();
  scoped_refptr<BrowsingDataChannelIDHelper> helper(
      BrowsingDataChannelIDHelper::Create(testing_profile_.get()));

  helper->StartFetching(
      base::Bind(&BrowsingDataChannelIDHelperTest::FetchCallback,
                 base::Unretained(this)));

  // Blocks until BrowsingDataChannelIDHelperTest::FetchCallback is
  // notified.
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(2UL, channel_id_list_.size());
  net::ChannelIDStore::ChannelIDList::const_iterator it =
      channel_id_list_.begin();

  // Correct because fetching channel_id_list_ will get them out in the
  // same order CreateChannelIDsForTest put them in.
  ASSERT_TRUE(it != channel_id_list_.end());
  EXPECT_EQ("https://www.google.com:443", it->server_identifier());

  ASSERT_TRUE(++it != channel_id_list_.end());
  EXPECT_EQ("https://www.youtube.com:443", it->server_identifier());

  ASSERT_TRUE(++it == channel_id_list_.end());

  EXPECT_EQ(0, ssl_config_changed_count_);
}

TEST_F(BrowsingDataChannelIDHelperTest, DeleteChannelID) {
  CreateChannelIDsForTest();
  scoped_refptr<BrowsingDataChannelIDHelper> helper(
      BrowsingDataChannelIDHelper::Create(testing_profile_.get()));

  helper->DeleteChannelID("https://www.google.com:443");

  helper->StartFetching(
      base::Bind(&BrowsingDataChannelIDHelperTest::FetchCallback,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, ssl_config_changed_count_);
  ASSERT_EQ(1UL, channel_id_list_.size());
  net::ChannelIDStore::ChannelIDList::const_iterator it =
      channel_id_list_.begin();

  ASSERT_TRUE(it != channel_id_list_.end());
  EXPECT_EQ("https://www.youtube.com:443", it->server_identifier());

  ASSERT_TRUE(++it == channel_id_list_.end());

  helper->DeleteChannelID("https://www.youtube.com:443");

  helper->StartFetching(
      base::Bind(&BrowsingDataChannelIDHelperTest::FetchCallback,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, ssl_config_changed_count_);
  ASSERT_EQ(0UL, channel_id_list_.size());
}

TEST_F(BrowsingDataChannelIDHelperTest, CannedEmpty) {
  std::string origin = "https://www.google.com";

  scoped_refptr<CannedBrowsingDataChannelIDHelper> helper(
      new CannedBrowsingDataChannelIDHelper());

  ASSERT_TRUE(helper->empty());
  helper->AddChannelID(net::ChannelIDStore::ChannelID(
      origin, base::Time(), base::Time(), "key", "cert"));
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}
