// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_mac.h"

#include <CoreMIDI/MIDIServices.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

void Noop(const MIDIPacketList*, void*, void*) {}

class FakeMidiManagerClient : public MidiManagerClient {
 public:
  FakeMidiManagerClient()
      : result_(MIDI_NOT_SUPPORTED),
        wait_for_result_(true),
        wait_for_port_(true) {}

  // MidiManagerClient implementation.
  void AddInputPort(const MidiPortInfo& info) override {}
  void AddOutputPort(const MidiPortInfo& info) override {
    CHECK(!wait_for_result_);
    info_ = info;
    wait_for_port_ = false;
  }
  void SetInputPortState(uint32 port_index, MidiPortState state) override {}
  void SetOutputPortState(uint32 port_index, MidiPortState state) override {}

  void CompleteStartSession(MidiResult result) override {
    EXPECT_TRUE(wait_for_result_);
    result_ = result;
    wait_for_result_ = false;
  }

  void ReceiveMidiData(uint32 port_index, const uint8* data, size_t size,
                       double timestamp) override {}
  void AccumulateMidiBytesSent(size_t size) override {}

  MidiResult WaitForResult() {
    while (wait_for_result_) {
      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
    return result_;
  }
  MidiPortInfo WaitForPort() {
    while (wait_for_port_) {
      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
    return info_;
  }

 private:
  MidiResult result_;
  bool wait_for_result_;
  MidiPortInfo info_;
  bool wait_for_port_;

  DISALLOW_COPY_AND_ASSIGN(FakeMidiManagerClient);
};

class MidiManagerMacTest : public ::testing::Test {
 public:
  MidiManagerMacTest()
      : manager_(new MidiManagerMac),
        message_loop_(new base::MessageLoop) {}

 protected:
  void StartSession(MidiManagerClient* client) {
    manager_->StartSession(client);
  }
  void EndSession(MidiManagerClient* client) {
    manager_->EndSession(client);
  }

 private:
  scoped_ptr<MidiManager> manager_;
  scoped_ptr<base::MessageLoop> message_loop_;

  DISALLOW_COPY_AND_ASSIGN(MidiManagerMacTest);
};


TEST_F(MidiManagerMacTest, MidiNotification) {
  scoped_ptr<FakeMidiManagerClient> client(new FakeMidiManagerClient);
  StartSession(client.get());

  MidiResult result = client->WaitForResult();
  EXPECT_EQ(MIDI_OK, result);

  // Create MIDIClient, and MIDIEndpoint as a MIDIDestination. This should
  // notify MIDIManagerMac as a MIDIObjectAddRemoveNotification.
  MIDIClientRef midi_client = 0;
  OSStatus status = MIDIClientCreate(
      CFSTR("MidiManagerMacTest"), nullptr, nullptr, &midi_client);
  EXPECT_EQ(noErr, status);

  MIDIEndpointRef ep = 0;
  status = MIDIDestinationCreate(
      midi_client, CFSTR("DestinationTest"), Noop, nullptr, &ep);
  EXPECT_EQ(noErr, status);

  // Wait until the created device is notified to MidiManagerMac.
  MidiPortInfo info = client->WaitForPort();
  EXPECT_EQ("DestinationTest", info.name);

  EndSession(client.get());
  if (ep)
    MIDIEndpointDispose(ep);
  if (midi_client)
    MIDIClientDispose(midi_client);
}

}  // namespace

}  // namespace media
