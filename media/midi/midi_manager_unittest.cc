// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

class FakeMidiManager : public MidiManager {
 public:
  FakeMidiManager()
      : start_initialization_is_called_(false),
        complete_initialization_synchronously_(false) {}
  virtual ~FakeMidiManager() {}

  // MidiManager implementation.
  virtual void StartInitialization() OVERRIDE {
    start_initialization_is_called_ = true;
    if (complete_initialization_synchronously_)
      CompleteInitialization(MIDI_OK);
  }

  virtual void DispatchSendMidiData(MidiManagerClient* client,
                                    uint32 port_index,
                                    const std::vector<uint8>& data,
                                    double timestamp) OVERRIDE {}

  // Utility functions for testing.
  void CallCompleteInitialization(MidiResult result) {
    CompleteInitialization(result);
  }

  size_t GetClientCount() {
    return clients_.size();
  }

  size_t GetPendingClientCount() {
    return pending_clients_.size();
  }

  bool start_initialization_is_called_;
  bool complete_initialization_synchronously_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeMidiManager);
};

class FakeMidiManagerClient : public MidiManagerClient {
 public:
  FakeMidiManagerClient(int client_id) : client_id_(client_id) {}
  virtual ~FakeMidiManagerClient() {}

  // MidiManagerClient implementation.
  virtual void CompleteStartSession(int client_id, MidiResult result) OVERRIDE {
    DCHECK_EQ(client_id_, client_id);
    result_ = result;
  }

  virtual void ReceiveMidiData(uint32 port_index, const uint8* data,
                               size_t size, double timestamp) OVERRIDE {}
  virtual void AccumulateMidiBytesSent(size_t size) OVERRIDE {}

  int GetClientId() {
    return client_id_;
  }

  MidiResult GetResult() {
    return result_;
  }

 private:
  int client_id_;
  MidiResult result_;

  DISALLOW_COPY_AND_ASSIGN(FakeMidiManagerClient);
};

class MidiManagerTest : public ::testing::Test {
 public:
  MidiManagerTest() : manager_(new FakeMidiManager) {}
  virtual ~MidiManagerTest() {}

 protected:
  void StartTheFirstSession(FakeMidiManagerClient* client,
                            bool complete_initialization_synchronously) {
    manager_->complete_initialization_synchronously_ =
        complete_initialization_synchronously;
    EXPECT_FALSE(manager_->start_initialization_is_called_);
    EXPECT_EQ(0U, manager_->GetClientCount());
    EXPECT_EQ(0U, manager_->GetPendingClientCount());
    manager_->StartSession(client, client->GetClientId());
    if (complete_initialization_synchronously) {
      EXPECT_EQ(1U, manager_->GetClientCount());
      EXPECT_EQ(0U, manager_->GetPendingClientCount());
      EXPECT_TRUE(manager_->start_initialization_is_called_);
      EXPECT_EQ(MIDI_OK, client->GetResult());
    } else {
      EXPECT_EQ(0U, manager_->GetClientCount());
      EXPECT_EQ(1U, manager_->GetPendingClientCount());
      EXPECT_TRUE(manager_->start_initialization_is_called_);
      EXPECT_EQ(0U, manager_->GetClientCount());
      EXPECT_EQ(1U, manager_->GetPendingClientCount());
      EXPECT_TRUE(manager_->start_initialization_is_called_);
    }
  }

  void StartTheSecondSession(FakeMidiManagerClient* client) {
    EXPECT_TRUE(manager_->start_initialization_is_called_);
    EXPECT_EQ(0U, manager_->GetClientCount());
    EXPECT_EQ(1U, manager_->GetPendingClientCount());

    // StartInitialization() should not be called for the second session.
    manager_->start_initialization_is_called_ = false;
    manager_->StartSession(client, client->GetClientId());
    EXPECT_FALSE(manager_->start_initialization_is_called_);
  }

  void EndSession(FakeMidiManagerClient* client, size_t before, size_t after) {
    EXPECT_EQ(before, manager_->GetClientCount());
    manager_->EndSession(client);
    EXPECT_EQ(after, manager_->GetClientCount());
  }

  void CompleteInitialization(MidiResult result) {
    manager_->CallCompleteInitialization(result);
  }

 private:
  scoped_ptr<FakeMidiManager> manager_;

  DISALLOW_COPY_AND_ASSIGN(MidiManagerTest);
};

// Check if calling CompleteInitialization() does not acquire the same lock
// on the same thread.
TEST_F(MidiManagerTest, StartAndEndSessionSynchronously) {
  scoped_ptr<FakeMidiManagerClient> client;
  client.reset(new FakeMidiManagerClient(0));

  StartTheFirstSession(client.get(), true);
  EndSession(client.get(), 1U, 0U);
}

TEST_F(MidiManagerTest, StartAndEndSession) {
  scoped_ptr<FakeMidiManagerClient> client;
  client.reset(new FakeMidiManagerClient(0));

  StartTheFirstSession(client.get(), false);
  CompleteInitialization(MIDI_OK);
  EXPECT_EQ(MIDI_OK, client->GetResult());
  EndSession(client.get(), 1U, 0U);
}

TEST_F(MidiManagerTest, StartAndEndSessionWithError) {
  scoped_ptr<FakeMidiManagerClient> client;
  client.reset(new FakeMidiManagerClient(1));

  StartTheFirstSession(client.get(), false);
  CompleteInitialization(MIDI_INITIALIZATION_ERROR);
  EXPECT_EQ(MIDI_INITIALIZATION_ERROR, client->GetResult());
  EndSession(client.get(), 0U, 0U);
}

TEST_F(MidiManagerTest, StartMultipleSessions) {
  scoped_ptr<FakeMidiManagerClient> client1;
  scoped_ptr<FakeMidiManagerClient> client2;
  client1.reset(new FakeMidiManagerClient(0));
  client2.reset(new FakeMidiManagerClient(1));

  StartTheFirstSession(client1.get(), false);
  StartTheSecondSession(client2.get());
  CompleteInitialization(MIDI_OK);
  EXPECT_EQ(MIDI_OK, client1->GetResult());
  EXPECT_EQ(MIDI_OK, client2->GetResult());
  EndSession(client1.get(), 2U, 1U);
  EndSession(client2.get(), 1U, 0U);
}

}  // namespace

}  // namespace media
