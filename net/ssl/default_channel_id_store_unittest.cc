// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/default_channel_id_store.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

void CallCounter(int* counter) {
  (*counter)++;
}

void GetChannelIDCallbackNotCalled(int err,
                                   const std::string& server_identifier,
                                   base::Time expiration_time,
                                   const std::string& private_key_result,
                                   const std::string& cert_result) {
  ADD_FAILURE() << "Unexpected callback execution.";
}

class AsyncGetChannelIDHelper {
 public:
  AsyncGetChannelIDHelper() : called_(false) {}

  void Callback(int err,
                const std::string& server_identifier,
                base::Time expiration_time,
                const std::string& private_key_result,
                const std::string& cert_result) {
    err_ = err;
    server_identifier_ = server_identifier;
    expiration_time_ = expiration_time;
    private_key_ = private_key_result;
    cert_ = cert_result;
    called_ = true;
  }

  int err_;
  std::string server_identifier_;
  base::Time expiration_time_;
  std::string private_key_;
  std::string cert_;
  bool called_;
};

void GetAllCallback(
    ChannelIDStore::ChannelIDList* dest,
    const ChannelIDStore::ChannelIDList& result) {
  *dest = result;
}

class MockPersistentStore
    : public DefaultChannelIDStore::PersistentStore {
 public:
  MockPersistentStore();

  // DefaultChannelIDStore::PersistentStore implementation.
  virtual void Load(const LoadedCallback& loaded_callback) OVERRIDE;
  virtual void AddChannelID(
      const DefaultChannelIDStore::ChannelID& channel_id) OVERRIDE;
  virtual void DeleteChannelID(
      const DefaultChannelIDStore::ChannelID& channel_id) OVERRIDE;
  virtual void SetForceKeepSessionState() OVERRIDE;

 protected:
  virtual ~MockPersistentStore();

 private:
  typedef std::map<std::string, DefaultChannelIDStore::ChannelID>
      ChannelIDMap;

  ChannelIDMap channel_ids_;
};

MockPersistentStore::MockPersistentStore() {}

void MockPersistentStore::Load(const LoadedCallback& loaded_callback) {
  scoped_ptr<ScopedVector<DefaultChannelIDStore::ChannelID> >
      channel_ids(new ScopedVector<DefaultChannelIDStore::ChannelID>());
  ChannelIDMap::iterator it;

  for (it = channel_ids_.begin(); it != channel_ids_.end(); ++it) {
    channel_ids->push_back(
        new DefaultChannelIDStore::ChannelID(it->second));
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(loaded_callback, base::Passed(&channel_ids)));
}

void MockPersistentStore::AddChannelID(
    const DefaultChannelIDStore::ChannelID& channel_id) {
  channel_ids_[channel_id.server_identifier()] = channel_id;
}

void MockPersistentStore::DeleteChannelID(
    const DefaultChannelIDStore::ChannelID& channel_id) {
  channel_ids_.erase(channel_id.server_identifier());
}

void MockPersistentStore::SetForceKeepSessionState() {}

MockPersistentStore::~MockPersistentStore() {}

}  // namespace

TEST(DefaultChannelIDStoreTest, TestLoading) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);

  persistent_store->AddChannelID(
      DefaultChannelIDStore::ChannelID(
          "google.com",
          base::Time(),
          base::Time(),
          "a", "b"));
  persistent_store->AddChannelID(
      DefaultChannelIDStore::ChannelID(
          "verisign.com",
          base::Time(),
          base::Time(),
          "c", "d"));

  // Make sure channel_ids load properly.
  DefaultChannelIDStore store(persistent_store.get());
  // Load has not occurred yet.
  EXPECT_EQ(0, store.GetChannelIDCount());
  store.SetChannelID(
      "verisign.com",
      base::Time(),
      base::Time(),
      "e", "f");
  // Wait for load & queued set task.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2, store.GetChannelIDCount());
  store.SetChannelID(
      "twitter.com",
      base::Time(),
      base::Time(),
      "g", "h");
  // Set should be synchronous now that load is done.
  EXPECT_EQ(3, store.GetChannelIDCount());
}

//TODO(mattm): add more tests of without a persistent store?
TEST(DefaultChannelIDStoreTest, TestSettingAndGetting) {
  // No persistent store, all calls will be synchronous.
  DefaultChannelIDStore store(NULL);
  base::Time expiration_time;
  std::string private_key, cert;
  EXPECT_EQ(0, store.GetChannelIDCount());
  EXPECT_EQ(ERR_FILE_NOT_FOUND,
            store.GetChannelID("verisign.com",
                               &expiration_time,
                               &private_key,
                               &cert,
                               base::Bind(&GetChannelIDCallbackNotCalled)));
  EXPECT_TRUE(private_key.empty());
  EXPECT_TRUE(cert.empty());
  store.SetChannelID(
      "verisign.com",
      base::Time::FromInternalValue(123),
      base::Time::FromInternalValue(456),
      "i", "j");
  EXPECT_EQ(OK,
            store.GetChannelID("verisign.com",
                               &expiration_time,
                               &private_key,
                               &cert,
                               base::Bind(&GetChannelIDCallbackNotCalled)));
  EXPECT_EQ(456, expiration_time.ToInternalValue());
  EXPECT_EQ("i", private_key);
  EXPECT_EQ("j", cert);
}

TEST(DefaultChannelIDStoreTest, TestDuplicateChannelIds) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultChannelIDStore store(persistent_store.get());

  base::Time expiration_time;
  std::string private_key, cert;
  EXPECT_EQ(0, store.GetChannelIDCount());
  store.SetChannelID(
      "verisign.com",
      base::Time::FromInternalValue(123),
      base::Time::FromInternalValue(1234),
      "a", "b");
  store.SetChannelID(
      "verisign.com",
      base::Time::FromInternalValue(456),
      base::Time::FromInternalValue(4567),
      "c", "d");

  // Wait for load & queued set tasks.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, store.GetChannelIDCount());
  EXPECT_EQ(OK,
            store.GetChannelID("verisign.com",
                               &expiration_time,
                               &private_key,
                               &cert,
                               base::Bind(&GetChannelIDCallbackNotCalled)));
  EXPECT_EQ(4567, expiration_time.ToInternalValue());
  EXPECT_EQ("c", private_key);
  EXPECT_EQ("d", cert);
}

TEST(DefaultChannelIDStoreTest, TestAsyncGet) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  persistent_store->AddChannelID(ChannelIDStore::ChannelID(
      "verisign.com",
      base::Time::FromInternalValue(123),
      base::Time::FromInternalValue(1234),
      "a", "b"));

  DefaultChannelIDStore store(persistent_store.get());
  AsyncGetChannelIDHelper helper;
  base::Time expiration_time;
  std::string private_key;
  std::string cert = "not set";
  EXPECT_EQ(0, store.GetChannelIDCount());
  EXPECT_EQ(ERR_IO_PENDING,
            store.GetChannelID("verisign.com",
                               &expiration_time,
                               &private_key,
                               &cert,
                               base::Bind(&AsyncGetChannelIDHelper::Callback,
                                          base::Unretained(&helper))));

  // Wait for load & queued get tasks.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, store.GetChannelIDCount());
  EXPECT_EQ("not set", cert);
  EXPECT_TRUE(helper.called_);
  EXPECT_EQ(OK, helper.err_);
  EXPECT_EQ("verisign.com", helper.server_identifier_);
  EXPECT_EQ(1234, helper.expiration_time_.ToInternalValue());
  EXPECT_EQ("a", helper.private_key_);
  EXPECT_EQ("b", helper.cert_);
}

TEST(DefaultChannelIDStoreTest, TestDeleteAll) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultChannelIDStore store(persistent_store.get());

  store.SetChannelID(
      "verisign.com",
      base::Time(),
      base::Time(),
      "a", "b");
  store.SetChannelID(
      "google.com",
      base::Time(),
      base::Time(),
      "c", "d");
  store.SetChannelID(
      "harvard.com",
      base::Time(),
      base::Time(),
      "e", "f");
  // Wait for load & queued set tasks.
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(3, store.GetChannelIDCount());
  int delete_finished = 0;
  store.DeleteAll(base::Bind(&CallCounter, &delete_finished));
  ASSERT_EQ(1, delete_finished);
  EXPECT_EQ(0, store.GetChannelIDCount());
}

TEST(DefaultChannelIDStoreTest, TestAsyncGetAndDeleteAll) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  persistent_store->AddChannelID(ChannelIDStore::ChannelID(
      "verisign.com",
      base::Time(),
      base::Time(),
      "a", "b"));
  persistent_store->AddChannelID(ChannelIDStore::ChannelID(
      "google.com",
      base::Time(),
      base::Time(),
      "c", "d"));

  ChannelIDStore::ChannelIDList pre_channel_ids;
  ChannelIDStore::ChannelIDList post_channel_ids;
  int delete_finished = 0;
  DefaultChannelIDStore store(persistent_store.get());

  store.GetAllChannelIDs(base::Bind(GetAllCallback, &pre_channel_ids));
  store.DeleteAll(base::Bind(&CallCounter, &delete_finished));
  store.GetAllChannelIDs(base::Bind(GetAllCallback, &post_channel_ids));
  // Tasks have not run yet.
  EXPECT_EQ(0u, pre_channel_ids.size());
  // Wait for load & queued tasks.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0, store.GetChannelIDCount());
  EXPECT_EQ(2u, pre_channel_ids.size());
  EXPECT_EQ(0u, post_channel_ids.size());
}

TEST(DefaultChannelIDStoreTest, TestDelete) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultChannelIDStore store(persistent_store.get());

  base::Time expiration_time;
  std::string private_key, cert;
  EXPECT_EQ(0, store.GetChannelIDCount());
  store.SetChannelID(
      "verisign.com",
      base::Time(),
      base::Time(),
      "a", "b");
  // Wait for load & queued set task.
  base::MessageLoop::current()->RunUntilIdle();

  store.SetChannelID(
      "google.com",
      base::Time(),
      base::Time(),
      "c", "d");

  EXPECT_EQ(2, store.GetChannelIDCount());
  int delete_finished = 0;
  store.DeleteChannelID("verisign.com",
                              base::Bind(&CallCounter, &delete_finished));
  ASSERT_EQ(1, delete_finished);
  EXPECT_EQ(1, store.GetChannelIDCount());
  EXPECT_EQ(ERR_FILE_NOT_FOUND,
            store.GetChannelID("verisign.com",
                               &expiration_time,
                               &private_key,
                               &cert,
                               base::Bind(&GetChannelIDCallbackNotCalled)));
  EXPECT_EQ(OK,
            store.GetChannelID("google.com",
                               &expiration_time,
                               &private_key,
                               &cert,
                               base::Bind(&GetChannelIDCallbackNotCalled)));
  int delete2_finished = 0;
  store.DeleteChannelID("google.com",
                        base::Bind(&CallCounter, &delete2_finished));
  ASSERT_EQ(1, delete2_finished);
  EXPECT_EQ(0, store.GetChannelIDCount());
  EXPECT_EQ(ERR_FILE_NOT_FOUND,
            store.GetChannelID("google.com",
                               &expiration_time,
                               &private_key,
                               &cert,
                               base::Bind(&GetChannelIDCallbackNotCalled)));
}

TEST(DefaultChannelIDStoreTest, TestAsyncDelete) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  persistent_store->AddChannelID(ChannelIDStore::ChannelID(
      "a.com",
      base::Time::FromInternalValue(1),
      base::Time::FromInternalValue(2),
      "a", "b"));
  persistent_store->AddChannelID(ChannelIDStore::ChannelID(
      "b.com",
      base::Time::FromInternalValue(3),
      base::Time::FromInternalValue(4),
      "c", "d"));
  DefaultChannelIDStore store(persistent_store.get());
  int delete_finished = 0;
  store.DeleteChannelID("a.com",
                        base::Bind(&CallCounter, &delete_finished));

  AsyncGetChannelIDHelper a_helper;
  AsyncGetChannelIDHelper b_helper;
  base::Time expiration_time;
  std::string private_key;
  std::string cert = "not set";
  EXPECT_EQ(0, store.GetChannelIDCount());
  EXPECT_EQ(ERR_IO_PENDING,
      store.GetChannelID(
          "a.com", &expiration_time, &private_key, &cert,
          base::Bind(&AsyncGetChannelIDHelper::Callback,
                     base::Unretained(&a_helper))));
  EXPECT_EQ(ERR_IO_PENDING,
      store.GetChannelID(
          "b.com", &expiration_time, &private_key, &cert,
          base::Bind(&AsyncGetChannelIDHelper::Callback,
                     base::Unretained(&b_helper))));

  EXPECT_EQ(0, delete_finished);
  EXPECT_FALSE(a_helper.called_);
  EXPECT_FALSE(b_helper.called_);
  // Wait for load & queued tasks.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, delete_finished);
  EXPECT_EQ(1, store.GetChannelIDCount());
  EXPECT_EQ("not set", cert);
  EXPECT_TRUE(a_helper.called_);
  EXPECT_EQ(ERR_FILE_NOT_FOUND, a_helper.err_);
  EXPECT_EQ("a.com", a_helper.server_identifier_);
  EXPECT_EQ(0, a_helper.expiration_time_.ToInternalValue());
  EXPECT_EQ("", a_helper.private_key_);
  EXPECT_EQ("", a_helper.cert_);
  EXPECT_TRUE(b_helper.called_);
  EXPECT_EQ(OK, b_helper.err_);
  EXPECT_EQ("b.com", b_helper.server_identifier_);
  EXPECT_EQ(4, b_helper.expiration_time_.ToInternalValue());
  EXPECT_EQ("c", b_helper.private_key_);
  EXPECT_EQ("d", b_helper.cert_);
}

TEST(DefaultChannelIDStoreTest, TestGetAll) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultChannelIDStore store(persistent_store.get());

  EXPECT_EQ(0, store.GetChannelIDCount());
  store.SetChannelID(
      "verisign.com",
      base::Time(),
      base::Time(),
      "a", "b");
  store.SetChannelID(
      "google.com",
      base::Time(),
      base::Time(),
      "c", "d");
  store.SetChannelID(
      "harvard.com",
      base::Time(),
      base::Time(),
      "e", "f");
  store.SetChannelID(
      "mit.com",
      base::Time(),
      base::Time(),
      "g", "h");
  // Wait for load & queued set tasks.
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(4, store.GetChannelIDCount());
  ChannelIDStore::ChannelIDList channel_ids;
  store.GetAllChannelIDs(base::Bind(GetAllCallback, &channel_ids));
  EXPECT_EQ(4u, channel_ids.size());
}

TEST(DefaultChannelIDStoreTest, TestInitializeFrom) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultChannelIDStore store(persistent_store.get());

  store.SetChannelID(
      "preexisting.com",
      base::Time(),
      base::Time(),
      "a", "b");
  store.SetChannelID(
      "both.com",
      base::Time(),
      base::Time(),
      "c", "d");
  // Wait for load & queued set tasks.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2, store.GetChannelIDCount());

  ChannelIDStore::ChannelIDList source_channel_ids;
  source_channel_ids.push_back(ChannelIDStore::ChannelID(
      "both.com",
      base::Time(),
      base::Time(),
      // Key differs from above to test that existing entries are overwritten.
      "e", "f"));
  source_channel_ids.push_back(ChannelIDStore::ChannelID(
      "copied.com",
      base::Time(),
      base::Time(),
      "g", "h"));
  store.InitializeFrom(source_channel_ids);
  EXPECT_EQ(3, store.GetChannelIDCount());

  ChannelIDStore::ChannelIDList channel_ids;
  store.GetAllChannelIDs(base::Bind(GetAllCallback, &channel_ids));
  ASSERT_EQ(3u, channel_ids.size());

  ChannelIDStore::ChannelIDList::iterator channel_id = channel_ids.begin();
  EXPECT_EQ("both.com", channel_id->server_identifier());
  EXPECT_EQ("e", channel_id->private_key());

  ++channel_id;
  EXPECT_EQ("copied.com", channel_id->server_identifier());
  EXPECT_EQ("g", channel_id->private_key());

  ++channel_id;
  EXPECT_EQ("preexisting.com", channel_id->server_identifier());
  EXPECT_EQ("a", channel_id->private_key());
}

TEST(DefaultChannelIDStoreTest, TestAsyncInitializeFrom) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  persistent_store->AddChannelID(ChannelIDStore::ChannelID(
      "preexisting.com",
      base::Time(),
      base::Time(),
      "a", "b"));
  persistent_store->AddChannelID(ChannelIDStore::ChannelID(
      "both.com",
      base::Time(),
      base::Time(),
      "c", "d"));

  DefaultChannelIDStore store(persistent_store.get());
  ChannelIDStore::ChannelIDList source_channel_ids;
  source_channel_ids.push_back(ChannelIDStore::ChannelID(
      "both.com",
      base::Time(),
      base::Time(),
      // Key differs from above to test that existing entries are overwritten.
      "e", "f"));
  source_channel_ids.push_back(ChannelIDStore::ChannelID(
      "copied.com",
      base::Time(),
      base::Time(),
      "g", "h"));
  store.InitializeFrom(source_channel_ids);
  EXPECT_EQ(0, store.GetChannelIDCount());
  // Wait for load & queued tasks.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(3, store.GetChannelIDCount());

  ChannelIDStore::ChannelIDList channel_ids;
  store.GetAllChannelIDs(base::Bind(GetAllCallback, &channel_ids));
  ASSERT_EQ(3u, channel_ids.size());

  ChannelIDStore::ChannelIDList::iterator channel_id = channel_ids.begin();
  EXPECT_EQ("both.com", channel_id->server_identifier());
  EXPECT_EQ("e", channel_id->private_key());

  ++channel_id;
  EXPECT_EQ("copied.com", channel_id->server_identifier());
  EXPECT_EQ("g", channel_id->private_key());

  ++channel_id;
  EXPECT_EQ("preexisting.com", channel_id->server_identifier());
  EXPECT_EQ("a", channel_id->private_key());
}

}  // namespace net
