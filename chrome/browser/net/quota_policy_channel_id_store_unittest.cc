// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/net/quota_policy_channel_id_store.h"
#include "content/public/test/mock_special_storage_policy.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/test_data_directory.h"
#include "net/cookies/cookie_util.h"
#include "net/ssl/ssl_client_cert_type.h"
#include "net/test/cert_test_util.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

const base::FilePath::CharType kTestChannelIDFilename[] =
    FILE_PATH_LITERAL("ChannelID");

class QuotaPolicyChannelIDStoreTest : public testing::Test {
 public:
  void Load(ScopedVector<net::DefaultChannelIDStore::ChannelID>* channel_ids) {
    base::RunLoop run_loop;
    store_->Load(base::Bind(&QuotaPolicyChannelIDStoreTest::OnLoaded,
                            base::Unretained(this),
                            &run_loop));
    run_loop.Run();
    channel_ids->swap(channel_ids_);
    channel_ids_.clear();
  }

  void OnLoaded(base::RunLoop* run_loop,
                scoped_ptr<ScopedVector<net::DefaultChannelIDStore::ChannelID> >
                    channel_ids) {
    channel_ids_.swap(*channel_ids);
    run_loop->Quit();
  }

 protected:
  static base::Time GetTestCertExpirationTime() {
    // Cert expiration time from 'dumpasn1 unittest.originbound.der':
    // GeneralizedTime 19/11/2111 02:23:45 GMT
    // base::Time::FromUTCExploded can't generate values past 2038 on 32-bit
    // linux, so we use the raw value here.
    return base::Time::FromInternalValue(GG_INT64_C(16121816625000000));
  }

  static base::Time GetTestCertCreationTime() {
    // UTCTime 13/12/2011 02:23:45 GMT
    base::Time::Exploded exploded_time;
    exploded_time.year = 2011;
    exploded_time.month = 12;
    exploded_time.day_of_week = 0;  // Unused.
    exploded_time.day_of_month = 13;
    exploded_time.hour = 2;
    exploded_time.minute = 23;
    exploded_time.second = 45;
    exploded_time.millisecond = 0;
    return base::Time::FromUTCExploded(exploded_time);
  }

  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    store_ = new QuotaPolicyChannelIDStore(
        temp_dir_.path().Append(kTestChannelIDFilename),
        base::ThreadTaskRunnerHandle::Get(),
        NULL);
    ScopedVector<net::DefaultChannelIDStore::ChannelID> channel_ids;
    Load(&channel_ids);
    ASSERT_EQ(0u, channel_ids.size());
    // Make sure the store gets written at least once.
    store_->AddChannelID(
        net::DefaultChannelIDStore::ChannelID("google.com",
                                              base::Time::FromInternalValue(1),
                                              base::Time::FromInternalValue(2),
                                              "a",
                                              "b"));
  }

  virtual void TearDown() {
    store_ = NULL;
    loop_.RunUntilIdle();
  }

  base::ScopedTempDir temp_dir_;
  scoped_refptr<QuotaPolicyChannelIDStore> store_;
  ScopedVector<net::DefaultChannelIDStore::ChannelID> channel_ids_;
  base::MessageLoop loop_;
};

// Test if data is stored as expected in the QuotaPolicy database.
TEST_F(QuotaPolicyChannelIDStoreTest, TestPersistence) {
  store_->AddChannelID(
      net::DefaultChannelIDStore::ChannelID("foo.com",
                                            base::Time::FromInternalValue(3),
                                            base::Time::FromInternalValue(4),
                                            "c",
                                            "d"));

  ScopedVector<net::DefaultChannelIDStore::ChannelID> channel_ids;
  // Replace the store effectively destroying the current one and forcing it
  // to write its data to disk. Then we can see if after loading it again it
  // is still there.
  store_ = NULL;
  // Make sure we wait until the destructor has run.
  base::RunLoop().RunUntilIdle();
  store_ = new QuotaPolicyChannelIDStore(
      temp_dir_.path().Append(kTestChannelIDFilename),
      base::MessageLoopProxy::current(),
      NULL);

  // Reload and test for persistence
  Load(&channel_ids);
  ASSERT_EQ(2U, channel_ids.size());
  net::DefaultChannelIDStore::ChannelID* goog_channel_id;
  net::DefaultChannelIDStore::ChannelID* foo_channel_id;
  if (channel_ids[0]->server_identifier() == "google.com") {
    goog_channel_id = channel_ids[0];
    foo_channel_id = channel_ids[1];
  } else {
    goog_channel_id = channel_ids[1];
    foo_channel_id = channel_ids[0];
  }
  ASSERT_EQ("google.com", goog_channel_id->server_identifier());
  ASSERT_STREQ("a", goog_channel_id->private_key().c_str());
  ASSERT_STREQ("b", goog_channel_id->cert().c_str());
  ASSERT_EQ(1, goog_channel_id->creation_time().ToInternalValue());
  ASSERT_EQ(2, goog_channel_id->expiration_time().ToInternalValue());
  ASSERT_EQ("foo.com", foo_channel_id->server_identifier());
  ASSERT_STREQ("c", foo_channel_id->private_key().c_str());
  ASSERT_STREQ("d", foo_channel_id->cert().c_str());
  ASSERT_EQ(3, foo_channel_id->creation_time().ToInternalValue());
  ASSERT_EQ(4, foo_channel_id->expiration_time().ToInternalValue());

  // Now delete the cert and check persistence again.
  store_->DeleteChannelID(*channel_ids[0]);
  store_->DeleteChannelID(*channel_ids[1]);
  store_ = NULL;
  // Make sure we wait until the destructor has run.
  base::RunLoop().RunUntilIdle();
  channel_ids.clear();
  store_ = new QuotaPolicyChannelIDStore(
      temp_dir_.path().Append(kTestChannelIDFilename),
      base::MessageLoopProxy::current(),
      NULL);

  // Reload and check if the cert has been removed.
  Load(&channel_ids);
  ASSERT_EQ(0U, channel_ids.size());
}

// Test if data is stored as expected in the QuotaPolicy database.
TEST_F(QuotaPolicyChannelIDStoreTest, TestPolicy) {
  store_->AddChannelID(
      net::DefaultChannelIDStore::ChannelID("foo.com",
                                            base::Time::FromInternalValue(3),
                                            base::Time::FromInternalValue(4),
                                            "c",
                                            "d"));

  ScopedVector<net::DefaultChannelIDStore::ChannelID> channel_ids;
  // Replace the store effectively destroying the current one and forcing it
  // to write its data to disk. Then we can see if after loading it again it
  // is still there.
  store_ = NULL;
  // Make sure we wait until the destructor has run.
  base::RunLoop().RunUntilIdle();
  // Specify storage policy that makes "foo.com" session only.
  scoped_refptr<content::MockSpecialStoragePolicy> storage_policy =
      new content::MockSpecialStoragePolicy();
  storage_policy->AddSessionOnly(
      net::cookie_util::CookieOriginToURL("foo.com", true));
  // Reload store, it should still have both channel ids.
  store_ = new QuotaPolicyChannelIDStore(
      temp_dir_.path().Append(kTestChannelIDFilename),
      base::MessageLoopProxy::current(),
      storage_policy);
  Load(&channel_ids);
  ASSERT_EQ(2U, channel_ids.size());

  // Now close the store, and "foo.com" should be deleted according to policy.
  store_ = NULL;
  // Make sure we wait until the destructor has run.
  base::RunLoop().RunUntilIdle();
  channel_ids.clear();
  store_ = new QuotaPolicyChannelIDStore(
      temp_dir_.path().Append(kTestChannelIDFilename),
      base::MessageLoopProxy::current(),
      NULL);

  // Reload and check that the "foo.com" cert has been removed.
  Load(&channel_ids);
  ASSERT_EQ(1U, channel_ids.size());
  ASSERT_EQ("google.com", channel_ids[0]->server_identifier());
}
