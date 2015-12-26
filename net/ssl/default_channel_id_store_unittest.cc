// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/default_channel_id_store.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "crypto/ec_private_key.h"
#include "net/base/net_errors.h"
#include "net/test/channel_id_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

void CallCounter(int* counter) {
  (*counter)++;
}

void GetChannelIDCallbackNotCalled(
    int err,
    const std::string& server_identifier,
    scoped_ptr<crypto::ECPrivateKey> key_result) {
  ADD_FAILURE() << "Unexpected callback execution.";
}

class AsyncGetChannelIDHelper {
 public:
  AsyncGetChannelIDHelper() : called_(false) {}

  void Callback(int err,
                const std::string& server_identifier,
                scoped_ptr<crypto::ECPrivateKey> key_result) {
    err_ = err;
    server_identifier_ = server_identifier;
    key_ = std::move(key_result);
    called_ = true;
  }

  int err_;
  std::string server_identifier_;
  scoped_ptr<crypto::ECPrivateKey> key_;
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
  void Load(const LoadedCallback& loaded_callback) override;
  void AddChannelID(
      const DefaultChannelIDStore::ChannelID& channel_id) override;
  void DeleteChannelID(
      const DefaultChannelIDStore::ChannelID& channel_id) override;
  void SetForceKeepSessionState() override;

 protected:
  ~MockPersistentStore() override;

 private:
  typedef std::map<std::string, DefaultChannelIDStore::ChannelID>
      ChannelIDMap;

  ChannelIDMap channel_ids_;
};

MockPersistentStore::MockPersistentStore() {}

void MockPersistentStore::Load(const LoadedCallback& loaded_callback) {
  scoped_ptr<std::vector<scoped_ptr<DefaultChannelIDStore::ChannelID>>>
      channel_ids(
          new std::vector<scoped_ptr<DefaultChannelIDStore::ChannelID>>());
  ChannelIDMap::iterator it;

  for (it = channel_ids_.begin(); it != channel_ids_.end(); ++it) {
    channel_ids->push_back(
        make_scoped_ptr(new DefaultChannelIDStore::ChannelID(it->second)));
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
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

  persistent_store->AddChannelID(DefaultChannelIDStore::ChannelID(
      "google.com", base::Time(),
      make_scoped_ptr(crypto::ECPrivateKey::Create())));
  persistent_store->AddChannelID(DefaultChannelIDStore::ChannelID(
      "verisign.com", base::Time(),
      make_scoped_ptr(crypto::ECPrivateKey::Create())));

  // Make sure channel_ids load properly.
  DefaultChannelIDStore store(persistent_store.get());
  // Load has not occurred yet.
  EXPECT_EQ(0, store.GetChannelIDCount());
  store.SetChannelID(make_scoped_ptr(new ChannelIDStore::ChannelID(
      "verisign.com", base::Time(),
      make_scoped_ptr(crypto::ECPrivateKey::Create()))));
  // Wait for load & queued set task.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2, store.GetChannelIDCount());
  store.SetChannelID(make_scoped_ptr(new ChannelIDStore::ChannelID(
      "twitter.com", base::Time(),
      make_scoped_ptr(crypto::ECPrivateKey::Create()))));
  // Set should be synchronous now that load is done.
  EXPECT_EQ(3, store.GetChannelIDCount());
}

//TODO(mattm): add more tests of without a persistent store?
TEST(DefaultChannelIDStoreTest, TestSettingAndGetting) {
  // No persistent store, all calls will be synchronous.
  DefaultChannelIDStore store(NULL);
  scoped_ptr<crypto::ECPrivateKey> expected_key(crypto::ECPrivateKey::Create());

  scoped_ptr<crypto::ECPrivateKey> key;
  EXPECT_EQ(0, store.GetChannelIDCount());
  EXPECT_EQ(ERR_FILE_NOT_FOUND,
            store.GetChannelID("verisign.com", &key,
                               base::Bind(&GetChannelIDCallbackNotCalled)));
  EXPECT_FALSE(key);
  store.SetChannelID(make_scoped_ptr(new ChannelIDStore::ChannelID(
      "verisign.com", base::Time::FromInternalValue(123),
      make_scoped_ptr(expected_key->Copy()))));
  EXPECT_EQ(OK, store.GetChannelID("verisign.com", &key,
                                   base::Bind(&GetChannelIDCallbackNotCalled)));
  EXPECT_TRUE(KeysEqual(expected_key.get(), key.get()));
}

TEST(DefaultChannelIDStoreTest, TestDuplicateChannelIds) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultChannelIDStore store(persistent_store.get());
  scoped_ptr<crypto::ECPrivateKey> expected_key(crypto::ECPrivateKey::Create());

  scoped_ptr<crypto::ECPrivateKey> key;
  EXPECT_EQ(0, store.GetChannelIDCount());
  store.SetChannelID(make_scoped_ptr(new ChannelIDStore::ChannelID(
      "verisign.com", base::Time::FromInternalValue(123),
      make_scoped_ptr(crypto::ECPrivateKey::Create()))));
  store.SetChannelID(make_scoped_ptr(new ChannelIDStore::ChannelID(
      "verisign.com", base::Time::FromInternalValue(456),
      make_scoped_ptr(expected_key->Copy()))));

  // Wait for load & queued set tasks.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, store.GetChannelIDCount());
  EXPECT_EQ(OK, store.GetChannelID("verisign.com", &key,
                                   base::Bind(&GetChannelIDCallbackNotCalled)));
  EXPECT_TRUE(KeysEqual(expected_key.get(), key.get()));
}

TEST(DefaultChannelIDStoreTest, TestAsyncGet) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  scoped_ptr<crypto::ECPrivateKey> expected_key(crypto::ECPrivateKey::Create());
  persistent_store->AddChannelID(ChannelIDStore::ChannelID(
      "verisign.com", base::Time::FromInternalValue(123),
      make_scoped_ptr(expected_key->Copy())));

  DefaultChannelIDStore store(persistent_store.get());
  AsyncGetChannelIDHelper helper;
  scoped_ptr<crypto::ECPrivateKey> key;
  EXPECT_EQ(0, store.GetChannelIDCount());
  EXPECT_EQ(ERR_IO_PENDING,
            store.GetChannelID("verisign.com", &key,
                               base::Bind(&AsyncGetChannelIDHelper::Callback,
                                          base::Unretained(&helper))));

  // Wait for load & queued get tasks.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, store.GetChannelIDCount());
  EXPECT_FALSE(key);
  EXPECT_TRUE(helper.called_);
  EXPECT_EQ(OK, helper.err_);
  EXPECT_EQ("verisign.com", helper.server_identifier_);
  EXPECT_TRUE(KeysEqual(expected_key.get(), helper.key_.get()));
}

TEST(DefaultChannelIDStoreTest, TestDeleteAll) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultChannelIDStore store(persistent_store.get());

  store.SetChannelID(make_scoped_ptr(new ChannelIDStore::ChannelID(
      "verisign.com", base::Time(),
      make_scoped_ptr(crypto::ECPrivateKey::Create()))));
  store.SetChannelID(make_scoped_ptr(new ChannelIDStore::ChannelID(
      "google.com", base::Time(),
      make_scoped_ptr(crypto::ECPrivateKey::Create()))));
  store.SetChannelID(make_scoped_ptr(new ChannelIDStore::ChannelID(
      "harvard.com", base::Time(),
      make_scoped_ptr(crypto::ECPrivateKey::Create()))));
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
      "verisign.com", base::Time(),
      make_scoped_ptr(crypto::ECPrivateKey::Create())));
  persistent_store->AddChannelID(ChannelIDStore::ChannelID(
      "google.com", base::Time(),
      make_scoped_ptr(crypto::ECPrivateKey::Create())));

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

  scoped_ptr<crypto::ECPrivateKey> key;
  EXPECT_EQ(0, store.GetChannelIDCount());
  store.SetChannelID(make_scoped_ptr(new ChannelIDStore::ChannelID(
      "verisign.com", base::Time(),
      make_scoped_ptr(crypto::ECPrivateKey::Create()))));
  // Wait for load & queued set task.
  base::MessageLoop::current()->RunUntilIdle();

  store.SetChannelID(make_scoped_ptr(new ChannelIDStore::ChannelID(
      "google.com", base::Time(),
      make_scoped_ptr(crypto::ECPrivateKey::Create()))));

  EXPECT_EQ(2, store.GetChannelIDCount());
  int delete_finished = 0;
  store.DeleteChannelID("verisign.com",
                              base::Bind(&CallCounter, &delete_finished));
  ASSERT_EQ(1, delete_finished);
  EXPECT_EQ(1, store.GetChannelIDCount());
  EXPECT_EQ(ERR_FILE_NOT_FOUND,
            store.GetChannelID("verisign.com", &key,
                               base::Bind(&GetChannelIDCallbackNotCalled)));
  EXPECT_EQ(OK, store.GetChannelID("google.com", &key,
                                   base::Bind(&GetChannelIDCallbackNotCalled)));
  int delete2_finished = 0;
  store.DeleteChannelID("google.com",
                        base::Bind(&CallCounter, &delete2_finished));
  ASSERT_EQ(1, delete2_finished);
  EXPECT_EQ(0, store.GetChannelIDCount());
  EXPECT_EQ(ERR_FILE_NOT_FOUND,
            store.GetChannelID("google.com", &key,
                               base::Bind(&GetChannelIDCallbackNotCalled)));
}

TEST(DefaultChannelIDStoreTest, TestAsyncDelete) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  scoped_ptr<crypto::ECPrivateKey> expected_key(crypto::ECPrivateKey::Create());
  persistent_store->AddChannelID(ChannelIDStore::ChannelID(
      "a.com", base::Time::FromInternalValue(1),
      make_scoped_ptr(crypto::ECPrivateKey::Create())));
  persistent_store->AddChannelID(
      ChannelIDStore::ChannelID("b.com", base::Time::FromInternalValue(3),
                                make_scoped_ptr(expected_key->Copy())));
  DefaultChannelIDStore store(persistent_store.get());
  int delete_finished = 0;
  store.DeleteChannelID("a.com",
                        base::Bind(&CallCounter, &delete_finished));

  AsyncGetChannelIDHelper a_helper;
  AsyncGetChannelIDHelper b_helper;
  scoped_ptr<crypto::ECPrivateKey> key;
  EXPECT_EQ(0, store.GetChannelIDCount());
  EXPECT_EQ(ERR_IO_PENDING,
            store.GetChannelID("a.com", &key,
                               base::Bind(&AsyncGetChannelIDHelper::Callback,
                                          base::Unretained(&a_helper))));
  EXPECT_EQ(ERR_IO_PENDING,
            store.GetChannelID("b.com", &key,
                               base::Bind(&AsyncGetChannelIDHelper::Callback,
                                          base::Unretained(&b_helper))));

  EXPECT_EQ(0, delete_finished);
  EXPECT_FALSE(a_helper.called_);
  EXPECT_FALSE(b_helper.called_);
  // Wait for load & queued tasks.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, delete_finished);
  EXPECT_EQ(1, store.GetChannelIDCount());
  EXPECT_FALSE(key);
  EXPECT_TRUE(a_helper.called_);
  EXPECT_EQ(ERR_FILE_NOT_FOUND, a_helper.err_);
  EXPECT_EQ("a.com", a_helper.server_identifier_);
  EXPECT_FALSE(a_helper.key_);
  EXPECT_TRUE(b_helper.called_);
  EXPECT_EQ(OK, b_helper.err_);
  EXPECT_EQ("b.com", b_helper.server_identifier_);
  EXPECT_TRUE(KeysEqual(expected_key.get(), b_helper.key_.get()));
}

TEST(DefaultChannelIDStoreTest, TestGetAll) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultChannelIDStore store(persistent_store.get());

  EXPECT_EQ(0, store.GetChannelIDCount());
  store.SetChannelID(make_scoped_ptr(new ChannelIDStore::ChannelID(
      "verisign.com", base::Time(),
      make_scoped_ptr(crypto::ECPrivateKey::Create()))));
  store.SetChannelID(make_scoped_ptr(new ChannelIDStore::ChannelID(
      "google.com", base::Time(),
      make_scoped_ptr(crypto::ECPrivateKey::Create()))));
  store.SetChannelID(make_scoped_ptr(new ChannelIDStore::ChannelID(
      "harvard.com", base::Time(),
      make_scoped_ptr(crypto::ECPrivateKey::Create()))));
  store.SetChannelID(make_scoped_ptr(new ChannelIDStore::ChannelID(
      "mit.com", base::Time(),
      make_scoped_ptr(crypto::ECPrivateKey::Create()))));
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
  scoped_ptr<crypto::ECPrivateKey> preexisting_key(
      crypto::ECPrivateKey::Create());
  scoped_ptr<crypto::ECPrivateKey> both_key(crypto::ECPrivateKey::Create());
  scoped_ptr<crypto::ECPrivateKey> copied_key(crypto::ECPrivateKey::Create());

  store.SetChannelID(make_scoped_ptr(
      new ChannelIDStore::ChannelID("preexisting.com", base::Time(),
                                    make_scoped_ptr(preexisting_key->Copy()))));
  store.SetChannelID(make_scoped_ptr(new ChannelIDStore::ChannelID(
      "both.com", base::Time(),
      make_scoped_ptr(crypto::ECPrivateKey::Create()))));
  // Wait for load & queued set tasks.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2, store.GetChannelIDCount());

  ChannelIDStore::ChannelIDList source_channel_ids;
  source_channel_ids.push_back(ChannelIDStore::ChannelID(
      "both.com", base::Time(),
      // Key differs from above to test that existing entries are overwritten.
      make_scoped_ptr(both_key->Copy())));
  source_channel_ids.push_back(ChannelIDStore::ChannelID(
      "copied.com", base::Time(), make_scoped_ptr(copied_key->Copy())));
  store.InitializeFrom(source_channel_ids);
  EXPECT_EQ(3, store.GetChannelIDCount());

  ChannelIDStore::ChannelIDList channel_ids;
  store.GetAllChannelIDs(base::Bind(GetAllCallback, &channel_ids));
  ASSERT_EQ(3u, channel_ids.size());

  ChannelIDStore::ChannelIDList::iterator channel_id = channel_ids.begin();
  EXPECT_EQ("both.com", channel_id->server_identifier());
  EXPECT_TRUE(KeysEqual(both_key.get(), channel_id->key()));

  ++channel_id;
  EXPECT_EQ("copied.com", channel_id->server_identifier());
  EXPECT_TRUE(KeysEqual(copied_key.get(), channel_id->key()));

  ++channel_id;
  EXPECT_EQ("preexisting.com", channel_id->server_identifier());
  EXPECT_TRUE(KeysEqual(preexisting_key.get(), channel_id->key()));
}

TEST(DefaultChannelIDStoreTest, TestAsyncInitializeFrom) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  scoped_ptr<crypto::ECPrivateKey> preexisting_key(
      crypto::ECPrivateKey::Create());
  scoped_ptr<crypto::ECPrivateKey> both_key(crypto::ECPrivateKey::Create());
  scoped_ptr<crypto::ECPrivateKey> copied_key(crypto::ECPrivateKey::Create());

  persistent_store->AddChannelID(
      ChannelIDStore::ChannelID("preexisting.com", base::Time(),
                                make_scoped_ptr(preexisting_key->Copy())));
  persistent_store->AddChannelID(ChannelIDStore::ChannelID(
      "both.com", base::Time(),
      make_scoped_ptr(crypto::ECPrivateKey::Create())));

  DefaultChannelIDStore store(persistent_store.get());
  ChannelIDStore::ChannelIDList source_channel_ids;
  source_channel_ids.push_back(ChannelIDStore::ChannelID(
      "both.com", base::Time(),
      // Key differs from above to test that existing entries are overwritten.
      make_scoped_ptr(both_key->Copy())));
  source_channel_ids.push_back(ChannelIDStore::ChannelID(
      "copied.com", base::Time(), make_scoped_ptr(copied_key->Copy())));
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
  EXPECT_TRUE(KeysEqual(both_key.get(), channel_id->key()));

  ++channel_id;
  EXPECT_EQ("copied.com", channel_id->server_identifier());
  EXPECT_TRUE(KeysEqual(copied_key.get(), channel_id->key()));

  ++channel_id;
  EXPECT_EQ("preexisting.com", channel_id->server_identifier());
  EXPECT_TRUE(KeysEqual(preexisting_key.get(), channel_id->key()));
}

}  // namespace net
