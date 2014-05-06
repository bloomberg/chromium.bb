// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

class FakeMidiManager : public MidiManager {
 public:
  FakeMidiManager() : start_initialization_is_called_(false) {}
  virtual ~FakeMidiManager() {}

  // MidiManager implementation.
  virtual void StartInitialization() OVERRIDE {
    start_initialization_is_called_ = true;
  }

  virtual void DispatchSendMidiData(MidiManagerClient* client,
                                    uint32 port_index,
                                    const std::vector<uint8>& data,
                                    double timestamp) OVERRIDE {}

  // Utility functions for testing.
  void CallCompleteInitialization(MidiResult result) {
    CompleteInitialization(result);
  }

  size_t GetClientCount() const {
    return get_clients_size_for_testing();
  }

  size_t GetPendingClientCount() const {
    return get_pending_clients_size_for_testing();
  }

  bool start_initialization_is_called_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeMidiManager);
};

class FakeMidiManagerClient : public MidiManagerClient {
 public:
  explicit FakeMidiManagerClient(int client_id)
      : client_id_(client_id),
        result_(MIDI_NOT_SUPPORTED),
        wait_for_result_(true) {}
  virtual ~FakeMidiManagerClient() {}

  // MidiManagerClient implementation.
  virtual void CompleteStartSession(int client_id, MidiResult result) OVERRIDE {
    CHECK_EQ(client_id_, client_id);
    result_ = result;
    wait_for_result_ = false;
  }

  virtual void ReceiveMidiData(uint32 port_index, const uint8* data,
                               size_t size, double timestamp) OVERRIDE {}
  virtual void AccumulateMidiBytesSent(size_t size) OVERRIDE {}

  int get_client_id() const { return client_id_; }
  MidiResult get_result() const { return result_; }

  MidiResult WaitForResult() {
    base::RunLoop run_loop;
    // CompleteStartSession() is called inside the message loop on the same
    // thread. Protection for |wait_for_result_| is not needed.
    while (wait_for_result_)
      run_loop.RunUntilIdle();
    return get_result();
  }

 private:
  int client_id_;
  MidiResult result_;
  bool wait_for_result_;

  DISALLOW_COPY_AND_ASSIGN(FakeMidiManagerClient);
};

class MidiManagerTest : public ::testing::Test {
 public:
  MidiManagerTest()
      : message_loop_(new base::MessageLoop), manager_(new FakeMidiManager) {}
  virtual ~MidiManagerTest() {}

 protected:
  void StartTheFirstSession(FakeMidiManagerClient* client) {
    EXPECT_FALSE(manager_->start_initialization_is_called_);
    EXPECT_EQ(0U, manager_->GetClientCount());
    EXPECT_EQ(0U, manager_->GetPendingClientCount());
    manager_->StartSession(client, client->get_client_id());
    EXPECT_EQ(0U, manager_->GetClientCount());
    EXPECT_EQ(1U, manager_->GetPendingClientCount());
    EXPECT_TRUE(manager_->start_initialization_is_called_);
    EXPECT_EQ(0U, manager_->GetClientCount());
    EXPECT_EQ(1U, manager_->GetPendingClientCount());
    EXPECT_TRUE(manager_->start_initialization_is_called_);
  }

  void StartTheSecondSession(FakeMidiManagerClient* client) {
    EXPECT_TRUE(manager_->start_initialization_is_called_);
    EXPECT_EQ(0U, manager_->GetClientCount());
    EXPECT_EQ(1U, manager_->GetPendingClientCount());

    // StartInitialization() should not be called for the second session.
    manager_->start_initialization_is_called_ = false;
    manager_->StartSession(client, client->get_client_id());
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
  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<FakeMidiManager> manager_;

  DISALLOW_COPY_AND_ASSIGN(MidiManagerTest);
};

TEST_F(MidiManagerTest, StartAndEndSession) {
  scoped_ptr<FakeMidiManagerClient> client;
  client.reset(new FakeMidiManagerClient(0));

  StartTheFirstSession(client.get());
  CompleteInitialization(MIDI_OK);
  EXPECT_EQ(MIDI_OK, client->WaitForResult());
  EndSession(client.get(), 1U, 0U);
}

TEST_F(MidiManagerTest, StartAndEndSessionWithError) {
  scoped_ptr<FakeMidiManagerClient> client;
  client.reset(new FakeMidiManagerClient(1));

  StartTheFirstSession(client.get());
  CompleteInitialization(MIDI_INITIALIZATION_ERROR);
  EXPECT_EQ(MIDI_INITIALIZATION_ERROR, client->WaitForResult());
  EndSession(client.get(), 0U, 0U);
}

TEST_F(MidiManagerTest, StartMultipleSessions) {
  scoped_ptr<FakeMidiManagerClient> client1;
  scoped_ptr<FakeMidiManagerClient> client2;
  client1.reset(new FakeMidiManagerClient(0));
  client2.reset(new FakeMidiManagerClient(1));

  StartTheFirstSession(client1.get());
  StartTheSecondSession(client2.get());
  CompleteInitialization(MIDI_OK);
  EXPECT_EQ(MIDI_OK, client1->WaitForResult());
  EXPECT_EQ(MIDI_OK, client2->WaitForResult());
  EndSession(client1.get(), 2U, 1U);
  EndSession(client2.get(), 1U, 0U);
}

TEST_F(MidiManagerTest, CreateMidiManager) {
  scoped_ptr<FakeMidiManagerClient> client;
  client.reset(new FakeMidiManagerClient(0));

  scoped_ptr<MidiManager> manager(MidiManager::Create());
  manager->StartSession(client.get(), client->get_client_id());
  // This #ifdef needs to be identical to the one in media/midi/midi_manager.cc
#if !defined(OS_MACOSX) && !defined(OS_WIN) && !defined(USE_ALSA) && \
    !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  EXPECT_EQ(MIDI_NOT_SUPPORTED, client->WaitForResult());
#else
  EXPECT_EQ(MIDI_OK, client->WaitForResult());
#endif
}

}  // namespace

}  // namespace media
