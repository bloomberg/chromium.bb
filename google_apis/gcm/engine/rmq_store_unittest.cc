// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/rmq_store.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "components/webdata/encryptor/encryptor.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

// Number of persistent ids to use in tests.
const int kNumPersistentIds = 10;

const uint64 kDeviceId = 22;
const uint64 kDeviceToken = 55;

class RMQStoreTest : public testing::Test {
 public:
  RMQStoreTest();
  virtual ~RMQStoreTest();

  scoped_ptr<RMQStore> BuildRMQStore();

  std::string GetNextPersistentId();

  void PumpLoop();

  void LoadCallback(RMQStore::LoadResult* result_dst,
                    const RMQStore::LoadResult& result);
  void UpdateCallback(bool success);

 private:
  base::MessageLoop message_loop_;
  base::ScopedTempDir temp_directory_;
  scoped_ptr<base::RunLoop> run_loop_;
};

RMQStoreTest::RMQStoreTest() {
  EXPECT_TRUE(temp_directory_.CreateUniqueTempDir());
  run_loop_.reset(new base::RunLoop());

  // On OSX, prevent the Keychain permissions popup during unit tests.
  #if defined(OS_MACOSX)
    Encryptor::UseMockKeychain(true);
  #endif
}

RMQStoreTest::~RMQStoreTest() {
}

scoped_ptr<RMQStore> RMQStoreTest::BuildRMQStore() {
  return scoped_ptr<RMQStore>(new RMQStore(temp_directory_.path(),
                              message_loop_.message_loop_proxy()));
}

std::string RMQStoreTest::GetNextPersistentId() {
  return base::Uint64ToString(base::Time::Now().ToInternalValue());
}

void RMQStoreTest::PumpLoop() {
  message_loop_.RunUntilIdle();
}

void RMQStoreTest::LoadCallback(RMQStore::LoadResult* result_dst,
                                const RMQStore::LoadResult& result) {
  ASSERT_TRUE(result.success);
  *result_dst = result;
  run_loop_->Quit();
  run_loop_.reset(new base::RunLoop());
}

void RMQStoreTest::UpdateCallback(bool success) {
  ASSERT_TRUE(success);
}

// Verify creating a new database and loading it.
TEST_F(RMQStoreTest, LoadNew) {
  scoped_ptr<RMQStore> rmq_store(BuildRMQStore());
  RMQStore::LoadResult load_result;
  rmq_store->Load(base::Bind(&RMQStoreTest::LoadCallback,
                             base::Unretained(this),
                             &load_result));
  PumpLoop();

  ASSERT_EQ(0U, load_result.device_android_id);
  ASSERT_EQ(0U, load_result.device_security_token);
  ASSERT_TRUE(load_result.incoming_messages.empty());
  ASSERT_TRUE(load_result.outgoing_messages.empty());
}

TEST_F(RMQStoreTest, DeviceCredentials) {
  scoped_ptr<RMQStore> rmq_store(BuildRMQStore());
  RMQStore::LoadResult load_result;
  rmq_store->Load(base::Bind(&RMQStoreTest::LoadCallback,
                             base::Unretained(this),
                             &load_result));
  PumpLoop();

  rmq_store->SetDeviceCredentials(kDeviceId,
                                  kDeviceToken,
                                  base::Bind(&RMQStoreTest::UpdateCallback,
                                             base::Unretained(this)));
  PumpLoop();

  rmq_store = BuildRMQStore().Pass();
  rmq_store->Load(base::Bind(&RMQStoreTest::LoadCallback,
                             base::Unretained(this),
                             &load_result));
  PumpLoop();

  ASSERT_EQ(kDeviceId, load_result.device_android_id);
  ASSERT_EQ(kDeviceToken, load_result.device_security_token);
}

// Verify saving some incoming messages, reopening the directory, and then
// removing those incoming messages.
TEST_F(RMQStoreTest, IncomingMessages) {
  scoped_ptr<RMQStore> rmq_store(BuildRMQStore());
  RMQStore::LoadResult load_result;
  rmq_store->Load(base::Bind(&RMQStoreTest::LoadCallback,
                             base::Unretained(this),
                             &load_result));
  PumpLoop();

  std::vector<std::string> persistent_ids;
  for (int i = 0; i < kNumPersistentIds; ++i) {
    persistent_ids.push_back(GetNextPersistentId());
    rmq_store->AddIncomingMessage(persistent_ids.back(),
                                  base::Bind(&RMQStoreTest::UpdateCallback,
                                             base::Unretained(this)));
    PumpLoop();
  }

  rmq_store = BuildRMQStore().Pass();
  rmq_store->Load(base::Bind(&RMQStoreTest::LoadCallback,
                             base::Unretained(this),
                             &load_result));
  PumpLoop();

  ASSERT_EQ(persistent_ids, load_result.incoming_messages);
  ASSERT_TRUE(load_result.outgoing_messages.empty());

  rmq_store->RemoveIncomingMessages(persistent_ids,
                                    base::Bind(&RMQStoreTest::UpdateCallback,
                                            base::Unretained(this)));
  PumpLoop();

  rmq_store = BuildRMQStore().Pass();
  load_result.incoming_messages.clear();
  rmq_store->Load(base::Bind(&RMQStoreTest::LoadCallback,
                             base::Unretained(this),
                             &load_result));
  PumpLoop();

  ASSERT_TRUE(load_result.incoming_messages.empty());
  ASSERT_TRUE(load_result.outgoing_messages.empty());
}

// Verify saving some outgoing messages, reopening the directory, and then
// removing those outgoing messages.
TEST_F(RMQStoreTest, OutgoingMessages) {
  scoped_ptr<RMQStore> rmq_store(BuildRMQStore());
  RMQStore::LoadResult load_result;
  rmq_store->Load(base::Bind(&RMQStoreTest::LoadCallback,
                             base::Unretained(this),
                             &load_result));
  PumpLoop();

  std::vector<std::string> persistent_ids;
  const int kNumPersistentIds = 10;
  for (int i = 0; i < kNumPersistentIds; ++i) {
    persistent_ids.push_back(GetNextPersistentId());
    mcs_proto::DataMessageStanza message;
    message.set_from(persistent_ids.back());
    message.set_category(persistent_ids.back());
    rmq_store->AddOutgoingMessage(persistent_ids.back(),
                                  MCSMessage(message),
                                  base::Bind(&RMQStoreTest::UpdateCallback,
                                             base::Unretained(this)));
    PumpLoop();
  }

  rmq_store = BuildRMQStore().Pass();
  rmq_store->Load(base::Bind(&RMQStoreTest::LoadCallback,
                             base::Unretained(this),
                             &load_result));
  PumpLoop();

  ASSERT_TRUE(load_result.incoming_messages.empty());
  ASSERT_EQ(load_result.outgoing_messages.size(), persistent_ids.size());
  for (int i =0 ; i < kNumPersistentIds; ++i) {
    std::string id = persistent_ids[i];
    ASSERT_TRUE(load_result.outgoing_messages[id]);
    const mcs_proto::DataMessageStanza* message =
        reinterpret_cast<mcs_proto::DataMessageStanza *>(
            load_result.outgoing_messages[id]);
    ASSERT_EQ(message->from(), id);
    ASSERT_EQ(message->category(), id);
  }

  rmq_store->RemoveOutgoingMessages(persistent_ids,
                                    base::Bind(&RMQStoreTest::UpdateCallback,
                                            base::Unretained(this)));
  PumpLoop();

  rmq_store = BuildRMQStore().Pass();
  load_result.outgoing_messages.clear();
  rmq_store->Load(base::Bind(&RMQStoreTest::LoadCallback,
                             base::Unretained(this),
                             &load_result));
  PumpLoop();

  ASSERT_TRUE(load_result.incoming_messages.empty());
  ASSERT_TRUE(load_result.outgoing_messages.empty());
}

// Verify incoming and outgoing messages don't conflict.
TEST_F(RMQStoreTest, IncomingAndOutgoingMessages) {
  scoped_ptr<RMQStore> rmq_store(BuildRMQStore());
  RMQStore::LoadResult load_result;
  rmq_store->Load(base::Bind(&RMQStoreTest::LoadCallback,
                             base::Unretained(this),
                             &load_result));
  PumpLoop();

  std::vector<std::string> persistent_ids;
  const int kNumPersistentIds = 10;
  for (int i = 0; i < kNumPersistentIds; ++i) {
    persistent_ids.push_back(GetNextPersistentId());
    rmq_store->AddIncomingMessage(persistent_ids.back(),
                                  base::Bind(&RMQStoreTest::UpdateCallback,
                                             base::Unretained(this)));
    PumpLoop();

    mcs_proto::DataMessageStanza message;
    message.set_from(persistent_ids.back());
    message.set_category(persistent_ids.back());
    rmq_store->AddOutgoingMessage(persistent_ids.back(),
                                  MCSMessage(message),
                                  base::Bind(&RMQStoreTest::UpdateCallback,
                                             base::Unretained(this)));
    PumpLoop();
  }


  rmq_store = BuildRMQStore().Pass();
  rmq_store->Load(base::Bind(&RMQStoreTest::LoadCallback,
                             base::Unretained(this),
                             &load_result));
  PumpLoop();

  ASSERT_EQ(persistent_ids, load_result.incoming_messages);
  ASSERT_EQ(load_result.outgoing_messages.size(), persistent_ids.size());
  for (int i =0 ; i < kNumPersistentIds; ++i) {
    std::string id = persistent_ids[i];
    ASSERT_TRUE(load_result.outgoing_messages[id]);
    const mcs_proto::DataMessageStanza* message =
        reinterpret_cast<mcs_proto::DataMessageStanza *>(
            load_result.outgoing_messages[id]);
    ASSERT_EQ(message->from(), id);
    ASSERT_EQ(message->category(), id);
  }

  rmq_store->RemoveIncomingMessages(persistent_ids,
                                    base::Bind(&RMQStoreTest::UpdateCallback,
                                            base::Unretained(this)));
  PumpLoop();
  rmq_store->RemoveOutgoingMessages(persistent_ids,
                                    base::Bind(&RMQStoreTest::UpdateCallback,
                                            base::Unretained(this)));
  PumpLoop();

  rmq_store = BuildRMQStore().Pass();
  load_result.incoming_messages.clear();
  load_result.outgoing_messages.clear();
  rmq_store->Load(base::Bind(&RMQStoreTest::LoadCallback,
                             base::Unretained(this),
                             &load_result));
  PumpLoop();

  ASSERT_TRUE(load_result.incoming_messages.empty());
  ASSERT_TRUE(load_result.outgoing_messages.empty());
}

}  // namespace

}  // namespace gcm
