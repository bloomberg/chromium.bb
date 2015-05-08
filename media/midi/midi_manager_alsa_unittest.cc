// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_alsa.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace midi {

class MidiManagerAlsaTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Pre-instantiate typical MidiPort instances that are often used in
    // following tests.

    // Inputs. port_input_0_ == port_input_1_.
    port_input_0_.reset(new MidiManagerAlsa::MidiPort(
        "path", "id", 1, 2, 5, "client_name", "port_name", "manufacturer",
        "version", MidiManagerAlsa::MidiPort::Type::kInput));
    port_input_1_.reset(new MidiManagerAlsa::MidiPort(
        "path", "id", 1, 2, 5, "client_name", "port_name", "manufacturer",
        "version", MidiManagerAlsa::MidiPort::Type::kInput));
    port_input_minimal_.reset(
        new MidiManagerAlsa::MidiPort("", "", 0, 0, 0, "", "", "", "",
                                      MidiManagerAlsa::MidiPort::Type::kInput));
    // Outputs. port_output_0_ == port_output_1_.
    port_output_0_.reset(new MidiManagerAlsa::MidiPort(
        "path", "id", 1, 2, 5, "client_name", "port_name", "manufacturer",
        "version", MidiManagerAlsa::MidiPort::Type::kOutput));
    port_output_1_.reset(new MidiManagerAlsa::MidiPort(
        "path", "id", 1, 2, 5, "client_name", "port_name", "manufacturer",
        "version", MidiManagerAlsa::MidiPort::Type::kOutput));

    // MidiPort fields that differ from port_input_0_ in a single way each time.
    // Used for testing the Match* and Find* methods.
    port_input_0_alt_path_.reset(new MidiManagerAlsa::MidiPort(
        "path2", "id", 1, 2, 5, "client_name", "port_name", "manufacturer",
        "version", MidiManagerAlsa::MidiPort::Type::kInput));
    port_input_0_alt_id_.reset(new MidiManagerAlsa::MidiPort(
        "path", "id2", 1, 2, 5, "client_name", "port_name", "manufacturer",
        "version", MidiManagerAlsa::MidiPort::Type::kInput));
    port_input_0_alt_client_name_.reset(new MidiManagerAlsa::MidiPort(
        "path", "id", 1, 2, 5, "client_name2", "port_name", "manufacturer",
        "version", MidiManagerAlsa::MidiPort::Type::kInput));
    port_input_0_alt_port_name_.reset(new MidiManagerAlsa::MidiPort(
        "path", "id", 1, 2, 5, "client_name", "port_name2", "manufacturer",
        "version", MidiManagerAlsa::MidiPort::Type::kInput));
    port_input_0_alt_client_id_.reset(new MidiManagerAlsa::MidiPort(
        "path", "id", 2, 2, 5, "client_name", "port_name", "manufacturer",
        "version", MidiManagerAlsa::MidiPort::Type::kInput));
    port_input_0_alt_port_id_.reset(new MidiManagerAlsa::MidiPort(
        "path", "id", 1, 3, 5, "client_name", "port_name", "manufacturer",
        "version", MidiManagerAlsa::MidiPort::Type::kInput));
    port_input_0_alt_midi_device_.reset(new MidiManagerAlsa::MidiPort(
        "path", "id", 1, 2, 6, "client_name", "port_name", "manufacturer",
        "version", MidiManagerAlsa::MidiPort::Type::kInput));

    // "No card" variants of above. For testing FindDisconnected.
    port_input_0_no_card_.reset(new MidiManagerAlsa::MidiPort(
        "", "", 1, 2, -1, "client_name", "port_name", "manufacturer", "version",
        MidiManagerAlsa::MidiPort::Type::kInput));
    port_input_1_no_card_.reset(new MidiManagerAlsa::MidiPort(
        "", "", 1, 2, -1, "client_name", "port_name", "manufacturer", "version",
        MidiManagerAlsa::MidiPort::Type::kInput));
    port_output_0_no_card_.reset(new MidiManagerAlsa::MidiPort(
        "", "", 1, 2, -1, "client_name", "port_name", "manufacturer", "version",
        MidiManagerAlsa::MidiPort::Type::kOutput));

    // No card variants of the alt variants from above. For more testing
    // of Match* and Find*.
    port_input_0_no_card_alt_client_name_.reset(new MidiManagerAlsa::MidiPort(
        "", "", 1, 2, -1, "client_name2", "port_name", "manufacturer",
        "version", MidiManagerAlsa::MidiPort::Type::kInput));
    port_input_0_no_card_alt_port_name_.reset(new MidiManagerAlsa::MidiPort(
        "", "", 1, 2, -1, "client_name", "port_name2", "manufacturer",
        "version", MidiManagerAlsa::MidiPort::Type::kInput));
    port_input_0_no_card_alt_client_id_.reset(new MidiManagerAlsa::MidiPort(
        "", "", 2, 2, -1, "client_name", "port_name", "manufacturer", "version",
        MidiManagerAlsa::MidiPort::Type::kInput));
    port_input_0_no_card_alt_port_id_.reset(new MidiManagerAlsa::MidiPort(
        "", "", 1, 3, -1, "client_name", "port_name", "manufacturer", "version",
        MidiManagerAlsa::MidiPort::Type::kInput));
  }

  // Counts ports for help with testing ToMidiPortState().
  int CountPorts(MidiManagerAlsa::TemporaryMidiPortState& state) {
    int count = 0;
    for (auto it = state.begin(); it != state.end(); ++it)
      ++count;
    return count;
  }

  scoped_ptr<MidiManagerAlsa::MidiPort> port_input_0_;
  scoped_ptr<MidiManagerAlsa::MidiPort> port_input_1_;
  scoped_ptr<MidiManagerAlsa::MidiPort> port_input_minimal_;
  scoped_ptr<MidiManagerAlsa::MidiPort> port_output_0_;
  scoped_ptr<MidiManagerAlsa::MidiPort> port_output_1_;

  scoped_ptr<MidiManagerAlsa::MidiPort> port_input_0_alt_path_;
  scoped_ptr<MidiManagerAlsa::MidiPort> port_input_0_alt_id_;
  scoped_ptr<MidiManagerAlsa::MidiPort> port_input_0_alt_client_name_;
  scoped_ptr<MidiManagerAlsa::MidiPort> port_input_0_alt_port_name_;
  scoped_ptr<MidiManagerAlsa::MidiPort> port_input_0_alt_client_id_;
  scoped_ptr<MidiManagerAlsa::MidiPort> port_input_0_alt_port_id_;
  scoped_ptr<MidiManagerAlsa::MidiPort> port_input_0_alt_midi_device_;

  scoped_ptr<MidiManagerAlsa::MidiPort> port_input_0_no_card_;
  scoped_ptr<MidiManagerAlsa::MidiPort> port_input_1_no_card_;
  scoped_ptr<MidiManagerAlsa::MidiPort> port_output_0_no_card_;

  scoped_ptr<MidiManagerAlsa::MidiPort> port_input_0_no_card_alt_client_name_;
  scoped_ptr<MidiManagerAlsa::MidiPort> port_input_0_no_card_alt_port_name_;
  scoped_ptr<MidiManagerAlsa::MidiPort> port_input_0_no_card_alt_client_id_;
  scoped_ptr<MidiManagerAlsa::MidiPort> port_input_0_no_card_alt_port_id_;

  // State fields to avoid declaring in test fixtures below.
  MidiManagerAlsa::MidiPortState midi_port_state_0_;
  MidiManagerAlsa::MidiPortState midi_port_state_1_;
  MidiManagerAlsa::TemporaryMidiPortState temporary_midi_port_state_0_;
  MidiManagerAlsa::AlsaSeqState alsa_seq_state_0_;
};

// Tests that ExtractManufacturerString works as expected.
TEST_F(MidiManagerAlsaTest, ExtractManufacturer) {
  EXPECT_EQ("My\\x20Vendor", MidiManagerAlsa::ExtractManufacturerString(
                                 "My\\x20Vendor", "1234", "My Vendor, Inc.",
                                 "Card", "My Vendor Inc Card at bus"));
  EXPECT_EQ("My Vendor", MidiManagerAlsa::ExtractManufacturerString(
                             "My Vendor", "1234", "My Vendor, Inc.", "Card",
                             "My Vendor Inc Card at bus"));
  EXPECT_EQ("My Vendor, Inc.", MidiManagerAlsa::ExtractManufacturerString(
                                   "1234", "1234", "My Vendor, Inc.", "Card",
                                   "My Vendor Inc Card at bus"));
  EXPECT_EQ("My Vendor Inc",
            MidiManagerAlsa::ExtractManufacturerString(
                "1234", "1234", "", "Card", "My Vendor Inc Card at bus"));
  EXPECT_EQ("My Vendor Inc",
            MidiManagerAlsa::ExtractManufacturerString(
                "", "", "", "Card", "My Vendor Inc Card at bus"));
  EXPECT_EQ("", MidiManagerAlsa::ExtractManufacturerString("1234", "1234", "",
                                                           "Card", "Longname"));
  EXPECT_EQ("Keystation\\x20Mini\\x2032",
            MidiManagerAlsa::ExtractManufacturerString(
                "Keystation\\x20Mini\\x2032", "129d",
                "Evolution Electronics, Ltd", "Keystation Mini 32",
                "Keystation Mini 32 Keystation Mini 32 at"
                " usb-0000:00:14.0-2.4.4, full speed"));
  EXPECT_EQ("Keystation Mini 32",
            MidiManagerAlsa::ExtractManufacturerString(
                "Keystation Mini 32", "129d", "Evolution Electronics, Ltd",
                "Keystation Mini 32",
                "Keystation Mini 32 Keystation Mini 32 at"
                " usb-0000:00:14.0-2.4.4, full speed"));
  EXPECT_EQ("Keystation Mini 32", MidiManagerAlsa::ExtractManufacturerString(
                                      "", "", "", "Keystation Mini 32",
                                      "Keystation Mini 32 Keystation Mini 32 at"
                                      " usb-0000:00:14.0-2.4.4, full speed"));
  EXPECT_EQ("", MidiManagerAlsa::ExtractManufacturerString(
                    "", "", "", "Serial MIDI (UART16550A)",
                    "Serial MIDI (UART16550A) [Soundcanvas] at 0x3f8, irq 4"));
  EXPECT_EQ("", MidiManagerAlsa::ExtractManufacturerString(
                    "", "", "", "VirMIDI", "Virtual MIDI Card 1"));
  EXPECT_EQ("C-Media Electronics Inc",
            MidiManagerAlsa::ExtractManufacturerString(
                "", "0x13f6", "C-Media Electronics Inc", "C-Media CMI8738 MIDI",
                "C-Media CMI8738 (model 55) at 0xd000, irq 19"));
  EXPECT_EQ("C-Media Electronics Inc",
            MidiManagerAlsa::ExtractManufacturerString(
                "", "0x13f6", "C-Media Electronics Inc", "C-Media CMI8738 FM",
                "C-Media CMI8738 (model 55) at 0xd000, irq 19"));
}

// Tests that verify proper serialization and generation of opaque key for
// MidiPort.
TEST_F(MidiManagerAlsaTest, JSONPortMetadata) {
  EXPECT_EQ(
      "{\"clientId\":1,\"clientName\":\"client_name\",\"id\":\"id\","
      "\"midiDevice\":5,\"path\":\"path\",\"portId\":2,\"portName\":\"port_"
      "name\",\"type\":\"input\"}",
      port_input_0_->JSONValue());

  EXPECT_EQ("03F255B7EE4D9D061597289CB16B45F997DBDB20D8E44429B052019C84E20A4A",
            port_input_0_->OpaqueKey());

  EXPECT_EQ(
      "{\"clientId\":1,\"clientName\":\"client_name\",\"id\":\"id\","
      "\"midiDevice\":5,\"path\":\"path\",\"portId\":2,\"portName\":\"port_"
      "name\",\"type\":\"output\"}",
      port_output_0_->JSONValue());
  EXPECT_EQ("3A3380FD64B8C79900C052D64C3F52E9ECD6537D00ECB02B8FA30032C0C03924",
            port_output_0_->OpaqueKey());

  EXPECT_EQ("{\"clientId\":0,\"midiDevice\":0,\"portId\":0,\"type\":\"input\"}",
            port_input_minimal_->JSONValue());
  EXPECT_EQ("3BC2A85598E5026D01DBCB022016C8A3362A9C7F912B88E303BF619C56D0C111",
            port_input_minimal_->OpaqueKey());
}

// Tests Match* methods.
TEST_F(MidiManagerAlsaTest, MatchConnected) {
  // The query can be disconnected or connected, but the target
  // must be connected.
  port_input_1_->set_connected(false);
  EXPECT_TRUE(port_input_0_->MatchConnected(*port_input_1_.get()));
  EXPECT_FALSE(port_input_1_->MatchConnected(*port_input_0_.get()));

  // Differing types.
  EXPECT_FALSE(port_input_0_->MatchConnected(*port_output_0_.get()));

  // Differing in 1 field. None should succeed.
  EXPECT_FALSE(port_input_0_->MatchConnected(*port_input_0_alt_path_.get()));
  EXPECT_FALSE(port_input_0_->MatchConnected(*port_input_0_alt_id_.get()));
  EXPECT_FALSE(
      port_input_0_->MatchConnected(*port_input_0_alt_client_name_.get()));
  EXPECT_FALSE(
      port_input_0_->MatchConnected(*port_input_0_alt_port_name_.get()));
  EXPECT_FALSE(
      port_input_0_->MatchConnected(*port_input_0_alt_client_id_.get()));
  EXPECT_FALSE(port_input_0_->MatchConnected(*port_input_0_alt_port_id_.get()));
  EXPECT_FALSE(
      port_input_0_->MatchConnected(*port_input_0_alt_midi_device_.get()));
}

TEST_F(MidiManagerAlsaTest, MatchCard1) {
  // The query can be disconnected or connected, but the target
  // must be disonnected.
  EXPECT_FALSE(port_input_0_->MatchCardPass1(*port_input_1_.get()));
  port_input_0_->set_connected(false);
  EXPECT_TRUE(port_input_0_->MatchCardPass1(*port_input_1_.get()));

  // Differing types.
  EXPECT_FALSE(port_input_0_->MatchCardPass1(*port_output_0_.get()));

  // Tests matches differing in 1 field.
  // client_name, port_name, client_id are ok to differ.
  EXPECT_FALSE(port_input_0_->MatchCardPass1(*port_input_0_alt_path_.get()));
  EXPECT_FALSE(port_input_0_->MatchCardPass1(*port_input_0_alt_id_.get()));
  EXPECT_TRUE(
      port_input_0_->MatchCardPass1(*port_input_0_alt_client_name_.get()));
  EXPECT_TRUE(
      port_input_0_->MatchCardPass1(*port_input_0_alt_port_name_.get()));
  EXPECT_TRUE(
      port_input_0_->MatchCardPass1(*port_input_0_alt_client_id_.get()));
  EXPECT_FALSE(port_input_0_->MatchCardPass1(*port_input_0_alt_port_id_.get()));
  EXPECT_FALSE(
      port_input_0_->MatchCardPass1(*port_input_0_alt_midi_device_.get()));
}

TEST_F(MidiManagerAlsaTest, MatchCard2) {
  // The query can be disconnected or connected, but the target
  // must be disonnected.
  EXPECT_FALSE(port_input_0_->MatchCardPass2(*port_input_1_.get()));
  port_input_0_->set_connected(false);
  EXPECT_TRUE(port_input_0_->MatchCardPass2(*port_input_1_.get()));

  // Differing types.
  EXPECT_FALSE(port_input_0_->MatchCardPass2(*port_output_0_.get()));

  // Tests matches differing in 1 field.
  // client_name, port_name, path, client_id are ok to differ.
  EXPECT_TRUE(port_input_0_->MatchCardPass2(*port_input_0_alt_path_.get()));
  EXPECT_FALSE(port_input_0_->MatchCardPass2(*port_input_0_alt_id_.get()));
  EXPECT_TRUE(
      port_input_0_->MatchCardPass2(*port_input_0_alt_client_name_.get()));
  EXPECT_TRUE(
      port_input_0_->MatchCardPass2(*port_input_0_alt_port_name_.get()));
  EXPECT_TRUE(
      port_input_0_->MatchCardPass2(*port_input_0_alt_client_id_.get()));
  EXPECT_FALSE(port_input_0_->MatchCardPass2(*port_input_0_alt_port_id_.get()));
  EXPECT_FALSE(
      port_input_0_->MatchCardPass2(*port_input_0_alt_midi_device_.get()));
}

TEST_F(MidiManagerAlsaTest, MatchNoCard1) {
  // The query can be disconnected or connected, but the target
  // must be disonnected.
  // path and id must be empty. midi_device must be -1.
  EXPECT_FALSE(port_input_0_no_card_->MatchNoCardPass1(*port_input_1_.get()));
  port_input_0_no_card_->set_connected(false);
  EXPECT_FALSE(port_input_0_no_card_->MatchNoCardPass1(*port_input_1_.get()));
  EXPECT_TRUE(
      port_input_0_no_card_->MatchNoCardPass1(*port_input_1_no_card_.get()));

  // Differing types.
  EXPECT_FALSE(
      port_input_0_no_card_->MatchNoCardPass1(*port_output_0_no_card_.get()));

  // Tests matches differing in 1 field.
  EXPECT_FALSE(port_input_0_no_card_->MatchNoCardPass1(
      *port_input_0_no_card_alt_client_name_.get()));
  EXPECT_FALSE(port_input_0_no_card_->MatchNoCardPass1(
      *port_input_0_no_card_alt_port_name_.get()));
  EXPECT_FALSE(port_input_0_no_card_->MatchNoCardPass1(
      *port_input_0_no_card_alt_client_id_.get()));
  EXPECT_FALSE(port_input_0_no_card_->MatchNoCardPass1(
      *port_input_0_no_card_alt_port_id_.get()));
}

TEST_F(MidiManagerAlsaTest, MatchNoCard2) {
  // The query can be disconnected or connected, but the target
  // must be disonnected.
  // path and id must be empty. midi_device must be -1.
  EXPECT_FALSE(port_input_0_no_card_->MatchNoCardPass2(*port_input_1_.get()));
  port_input_0_no_card_->set_connected(false);
  EXPECT_FALSE(port_input_0_no_card_->MatchNoCardPass2(*port_input_1_.get()));
  EXPECT_TRUE(
      port_input_0_no_card_->MatchNoCardPass2(*port_input_1_no_card_.get()));

  // Differing types.
  EXPECT_FALSE(
      port_input_0_no_card_->MatchNoCardPass2(*port_output_0_no_card_.get()));

  // Tests matches differing in 1 field.
  // client_id ok to differ.
  EXPECT_FALSE(port_input_0_no_card_->MatchNoCardPass2(
      *port_input_0_no_card_alt_client_name_.get()));
  EXPECT_FALSE(port_input_0_no_card_->MatchNoCardPass2(
      *port_input_0_no_card_alt_port_name_.get()));
  EXPECT_TRUE(port_input_0_no_card_->MatchNoCardPass2(
      *port_input_0_no_card_alt_client_id_.get()));
  EXPECT_FALSE(port_input_0_no_card_->MatchNoCardPass2(
      *port_input_0_no_card_alt_port_id_.get()));
}

// Tests that MidiPorts start connected.
TEST_F(MidiManagerAlsaTest, PortStartsConnected) {
  EXPECT_TRUE(port_output_0_->connected());
  EXPECT_TRUE(port_input_0_->connected());
}

// Tests that the web_port_index gets updated by MidiPortState.
TEST_F(MidiManagerAlsaTest, PortIndexSet) {
  auto* port_input_0_tracking_pointer = port_input_0_.get();
  auto* port_output_0_tracking_pointer = port_output_0_.get();
  auto* port_input_1_tracking_pointer = port_input_1_.get();
  auto* port_output_1_tracking_pointer = port_input_1_.get();

  // Explicitly initialize web_port_index.
  port_input_1_->set_web_port_index(5000);
  port_output_1_->set_web_port_index(5000);

  midi_port_state_0_.Insert(port_input_0_.Pass());
  midi_port_state_0_.Insert(port_output_0_.Pass());
  midi_port_state_0_.Insert(port_input_1_.Pass());
  midi_port_state_0_.Insert(port_output_1_.Pass());

  // First port of each type has index of 0.
  EXPECT_EQ(0U, port_input_0_tracking_pointer->web_port_index());
  EXPECT_EQ(0U, port_output_0_tracking_pointer->web_port_index());
  // Second port of each type has index of 1.
  EXPECT_EQ(1U, port_input_1_tracking_pointer->web_port_index());
  EXPECT_EQ(1U, port_output_1_tracking_pointer->web_port_index());
}

// Tests that the web_port_index is not updated by TemporaryMidiPortState.
TEST_F(MidiManagerAlsaTest, PortIndexNotSet) {
  auto* port_input_0_tracking_pointer = port_input_0_.get();
  auto* port_output_0_tracking_pointer = port_output_0_.get();
  auto* port_input_1_tracking_pointer = port_input_1_.get();
  auto* port_output_1_tracking_pointer = port_input_1_.get();

  // Explicitly initialize web_port_index.
  port_input_1_->set_web_port_index(5000);
  port_output_1_->set_web_port_index(5000);

  temporary_midi_port_state_0_.Insert(port_input_0_.Pass());
  temporary_midi_port_state_0_.Insert(port_output_0_.Pass());
  temporary_midi_port_state_0_.Insert(port_input_1_.Pass());
  temporary_midi_port_state_0_.Insert(port_output_1_.Pass());

  // web_port_index is untouched.
  EXPECT_EQ(0U, port_input_0_tracking_pointer->web_port_index());
  EXPECT_EQ(0U, port_output_0_tracking_pointer->web_port_index());
  EXPECT_EQ(5000U, port_input_1_tracking_pointer->web_port_index());
  EXPECT_EQ(5000U, port_output_1_tracking_pointer->web_port_index());
}

// Tests that inputs and outputs stay separate in MidiPortState.
TEST_F(MidiManagerAlsaTest, SeparateInputOutput) {
  auto* port_input_0_tracking_pointer = port_input_0_.get();
  auto* port_output_0_tracking_pointer = port_output_0_.get();
  auto* port_input_1_tracking_pointer = port_input_1_.get();
  auto* port_output_1_tracking_pointer = port_input_1_.get();

  // First port of each type has index of 0.
  EXPECT_EQ(0U, midi_port_state_0_.Insert(port_input_0_.Pass()));
  EXPECT_EQ(0U, midi_port_state_0_.Insert(port_output_0_.Pass()));

  // Second port of each type has index of 1.
  EXPECT_EQ(1U, midi_port_state_0_.Insert(port_input_1_.Pass()));
  EXPECT_EQ(1U, midi_port_state_0_.Insert(port_output_1_.Pass()));

  // Check again that the field matches what was returned.
  EXPECT_EQ(0U, port_input_0_tracking_pointer->web_port_index());
  EXPECT_EQ(0U, port_output_0_tracking_pointer->web_port_index());
  EXPECT_EQ(1U, port_input_1_tracking_pointer->web_port_index());
  EXPECT_EQ(1U, port_output_1_tracking_pointer->web_port_index());
}

// Tests FindConnected.
TEST_F(MidiManagerAlsaTest, FindConnected) {
  auto* port_input_0_tracking_pointer = port_input_0_.get();
  auto* port_input_1_tracking_pointer = port_input_1_.get();

  // Insert port_input_0.
  midi_port_state_0_.Insert(port_input_0_.Pass());
  // Look for port_input_1 (every field matches port_input_0).
  auto it = midi_port_state_0_.FindConnected(*port_input_1_tracking_pointer);
  EXPECT_EQ(port_input_0_tracking_pointer, *it);
  // Look for something else that we won't find.
  EXPECT_EQ(midi_port_state_0_.end(),
            midi_port_state_0_.FindConnected(*port_input_0_alt_path_));
}

TEST_F(MidiManagerAlsaTest, FindConnected2) {
  auto* port_input_0_tracking_pointer = port_input_0_.get();
  auto* port_input_1_tracking_pointer = port_input_1_.get();

  // Insert some stuff.
  midi_port_state_0_.Insert(port_input_0_alt_path_.Pass());
  midi_port_state_0_.Insert(port_input_0_alt_id_.Pass());
  midi_port_state_0_.Insert(port_input_0_alt_client_name_.Pass());
  // Insert port_input_0.
  midi_port_state_0_.Insert(port_input_0_.Pass());
  // Insert some more stuff.
  midi_port_state_0_.Insert(port_input_0_alt_port_id_.Pass());
  // Look for port_input_1 (matches to port_input_0).
  auto it = midi_port_state_0_.FindConnected(*port_input_1_tracking_pointer);
  EXPECT_EQ(port_input_0_tracking_pointer, *it);
  // Look for something else that we won't find.
  EXPECT_EQ(midi_port_state_0_.end(),
            midi_port_state_0_.FindConnected(*port_input_minimal_));
}

TEST_F(MidiManagerAlsaTest, FindConnected3) {
  // midi_port_state_0_ is empty to start.
  EXPECT_EQ(midi_port_state_0_.end(),
            midi_port_state_0_.FindConnected(*port_input_minimal_));
}

// Tests FindDisconnected.
TEST_F(MidiManagerAlsaTest, FindDisconnected) {
  // midi_port_state_0_ is empty to start.
  EXPECT_EQ(midi_port_state_0_.end(),
            midi_port_state_0_.FindDisconnected(*port_input_minimal_));
}

TEST_F(MidiManagerAlsaTest, FindDisconnected2) {
  auto* port_input_0_tracking_pointer = port_input_0_.get();
  auto* port_input_1_tracking_pointer = port_input_1_.get();
  auto* port_input_1_no_card_tracking_pointer = port_input_1_no_card_.get();

  // Ports need to be disconnected to find them.
  port_input_0_alt_id_->set_connected(false);
  port_input_0_alt_path_->set_connected(false);
  port_input_0_->set_connected(false);

  // Insert some stuff.
  midi_port_state_0_.Insert(port_input_0_alt_id_.Pass());
  midi_port_state_0_.Insert(port_input_0_alt_path_.Pass());
  // Insert port_input_0.
  midi_port_state_0_.Insert(port_input_0_.Pass());

  // Add "no card" stuff.
  port_input_1_no_card_->set_connected(false);
  midi_port_state_0_.Insert(port_input_1_no_card_.Pass());

  // Insert some more stuff.
  midi_port_state_0_.Insert(port_input_0_alt_port_id_.Pass());

  // Look for port_input_1, should trigger exact match.
  EXPECT_EQ(port_input_0_tracking_pointer, *midi_port_state_0_.FindDisconnected(
                                               *port_input_1_tracking_pointer));

  // Look for no card exact match.
  EXPECT_EQ(port_input_1_no_card_tracking_pointer,
            *midi_port_state_0_.FindDisconnected(*port_input_0_no_card_.get()));

  // Look for something else that we won't find.
  EXPECT_EQ(midi_port_state_0_.end(),
            midi_port_state_0_.FindDisconnected(*port_input_minimal_));
}

TEST_F(MidiManagerAlsaTest, FindDisconnected3) {
  auto* port_input_0_tracking_pointer = port_input_0_.get();
  auto* port_input_0_alt_path_tracking_pointer = port_input_0_alt_path_.get();
  auto* port_input_1_no_card_tracking_pointer = port_input_1_no_card_.get();

  // Ports need to be disconnected to find them.
  port_input_0_alt_path_->set_connected(false);
  port_input_0_->set_connected(false);

  // Insert some stuff.
  midi_port_state_0_.Insert(port_input_0_alt_path_.Pass());
  midi_port_state_0_.Insert(port_input_0_alt_id_.Pass());

  // Add no card stuff.
  port_input_1_no_card_->set_connected(false);
  midi_port_state_0_.Insert(port_input_1_no_card_.Pass());

  // Look for port_input_0, should find port_input_0_alt_path.
  EXPECT_EQ(
      port_input_0_alt_path_tracking_pointer,
      *midi_port_state_0_.FindDisconnected(*port_input_0_tracking_pointer));

  // Look for no card partial match.
  EXPECT_EQ(port_input_1_no_card_tracking_pointer,
            *midi_port_state_0_.FindDisconnected(
                *port_input_0_no_card_alt_client_id_.get()));

  // Won't find this.
  EXPECT_EQ(midi_port_state_0_.end(),
            midi_port_state_0_.FindDisconnected(
                *port_input_0_no_card_alt_port_id_.get()));

  // Look for something else that we won't find.
  EXPECT_EQ(midi_port_state_0_.end(),
            midi_port_state_0_.FindDisconnected(*port_input_minimal_));
}

// Tests AlsaSeqState -> MidiPortState.
TEST_F(MidiManagerAlsaTest, ToMidiPortState) {
  // Empty state.
  EXPECT_EQ(0, CountPorts(*alsa_seq_state_0_.ToMidiPortState().get()));

  // Still empty, because there are no ports yet.
  alsa_seq_state_0_.ClientStart(0, "0", SND_SEQ_KERNEL_CLIENT);
  EXPECT_EQ(0, CountPorts(*alsa_seq_state_0_.ToMidiPortState().get()));

  // Add a port, now it has 1 item when converted.
  alsa_seq_state_0_.PortStart(
      0, 0, "0:0", MidiManagerAlsa::AlsaSeqState::PortDirection::kInput, true);
  EXPECT_EQ(1, CountPorts(*alsa_seq_state_0_.ToMidiPortState().get()));

  // Close client. This closes its ports and returns count to 0.
  alsa_seq_state_0_.ClientExit(0);
  EXPECT_EQ(0, CountPorts(*alsa_seq_state_0_.ToMidiPortState().get()));

  // Add another port, without client. This does nothing.
  alsa_seq_state_0_.PortStart(
      0, 0, "0:0", MidiManagerAlsa::AlsaSeqState::PortDirection::kInput, true);
  EXPECT_EQ(0, CountPorts(*alsa_seq_state_0_.ToMidiPortState().get()));

  // Close client again. This does nothing.
  alsa_seq_state_0_.ClientExit(0);
  EXPECT_EQ(0, CountPorts(*alsa_seq_state_0_.ToMidiPortState().get()));

  // Add duplex port. This will add 2 ports when converted.
  alsa_seq_state_0_.ClientStart(0, "0", SND_SEQ_KERNEL_CLIENT);
  alsa_seq_state_0_.PortStart(
      0, 0, "0:0", MidiManagerAlsa::AlsaSeqState::PortDirection::kDuplex, true);
  EXPECT_EQ(2, CountPorts(*alsa_seq_state_0_.ToMidiPortState().get()));

  // Add an output port. Now we are at 3.
  alsa_seq_state_0_.PortStart(
      0, 1, "0:1", MidiManagerAlsa::AlsaSeqState::PortDirection::kOutput, true);
  EXPECT_EQ(3, CountPorts(*alsa_seq_state_0_.ToMidiPortState().get()));

  // Add another client. Still at 3.
  alsa_seq_state_0_.ClientStart(1, "1", SND_SEQ_KERNEL_CLIENT);
  EXPECT_EQ(3, CountPorts(*alsa_seq_state_0_.ToMidiPortState().get()));

  // Add a port. Now at 4.
  alsa_seq_state_0_.PortStart(
      1, 0, "1:0", MidiManagerAlsa::AlsaSeqState::PortDirection::kInput, true);
  EXPECT_EQ(4, CountPorts(*alsa_seq_state_0_.ToMidiPortState().get()));

  // Add a duplicate port. Still at 4.
  alsa_seq_state_0_.PortStart(
      1, 0, "1:0", MidiManagerAlsa::AlsaSeqState::PortDirection::kInput, true);
  EXPECT_EQ(4, CountPorts(*alsa_seq_state_0_.ToMidiPortState().get()));

  // Add a duplicate client. This will close the ports from the previous client.
  alsa_seq_state_0_.ClientStart(1, "1", SND_SEQ_KERNEL_CLIENT);
  EXPECT_EQ(3, CountPorts(*alsa_seq_state_0_.ToMidiPortState().get()));

  // Remove a duplex port. This reduces count by 2.
  alsa_seq_state_0_.PortExit(0, 0);
  EXPECT_EQ(1, CountPorts(*alsa_seq_state_0_.ToMidiPortState().get()));

  // Remove a non-existent port. No change.
  alsa_seq_state_0_.PortExit(0, 0);
  EXPECT_EQ(1, CountPorts(*alsa_seq_state_0_.ToMidiPortState().get()));

  // Verify the last entry.
  EXPECT_TRUE((*alsa_seq_state_0_.ToMidiPortState()->begin())
                  ->MatchConnected(MidiManagerAlsa::MidiPort(
                      "", "", 0, 1, -1, "0", "0:1", "", "",
                      MidiManagerAlsa::MidiPort::Type::kOutput)));
}

// TODO(agoode): Test old -> new state event generation, using mocks.

}  // namespace midi
}  // namespace media
