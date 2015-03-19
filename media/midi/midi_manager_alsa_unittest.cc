// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_alsa.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(MidiManagerAlsaTest, ExtractManufacturer) {
  ASSERT_EQ("My\\x20Vendor",
            MidiManagerAlsa::AlsaRawmidi::ExtractManufacturerString(
                "My\\x20Vendor", "1234", "My Vendor, Inc.", "Card",
                "My Vendor Inc Card at bus"));
  ASSERT_EQ("My Vendor",
            MidiManagerAlsa::AlsaRawmidi::ExtractManufacturerString(
                "My Vendor", "1234", "My Vendor, Inc.", "Card",
                "My Vendor Inc Card at bus"));
  ASSERT_EQ("My Vendor, Inc.",
            MidiManagerAlsa::AlsaRawmidi::ExtractManufacturerString(
                "1234", "1234", "My Vendor, Inc.", "Card",
                "My Vendor Inc Card at bus"));
  ASSERT_EQ("My Vendor Inc",
            MidiManagerAlsa::AlsaRawmidi::ExtractManufacturerString(
                "1234", "1234", "", "Card", "My Vendor Inc Card at bus"));
  ASSERT_EQ("My Vendor Inc",
            MidiManagerAlsa::AlsaRawmidi::ExtractManufacturerString(
                "", "", "", "Card", "My Vendor Inc Card at bus"));
  ASSERT_EQ("", MidiManagerAlsa::AlsaRawmidi::ExtractManufacturerString(
                    "1234", "1234", "", "Card", "Longname"));
  ASSERT_EQ("Keystation\\x20Mini\\x2032",
            MidiManagerAlsa::AlsaRawmidi::ExtractManufacturerString(
                "Keystation\\x20Mini\\x2032", "129d",
                "Evolution Electronics, Ltd", "Keystation Mini 32",
                "Keystation Mini 32 Keystation Mini 32 at"
                " usb-0000:00:14.0-2.4.4, full speed"));
  ASSERT_EQ("Keystation Mini 32",
            MidiManagerAlsa::AlsaRawmidi::ExtractManufacturerString(
                "Keystation Mini 32", "129d", "Evolution Electronics, Ltd",
                "Keystation Mini 32",
                "Keystation Mini 32 Keystation Mini 32 at"
                " usb-0000:00:14.0-2.4.4, full speed"));
  ASSERT_EQ("Keystation Mini 32",
            MidiManagerAlsa::AlsaRawmidi::ExtractManufacturerString(
                "", "", "", "Keystation Mini 32",
                "Keystation Mini 32 Keystation Mini 32 at"
                " usb-0000:00:14.0-2.4.4, full speed"));
  ASSERT_EQ("", MidiManagerAlsa::AlsaRawmidi::ExtractManufacturerString(
                    "", "", "", "Serial MIDI (UART16550A)",
                    "Serial MIDI (UART16550A) [Soundcanvas] at 0x3f8, irq 4"));
  ASSERT_EQ("", MidiManagerAlsa::AlsaRawmidi::ExtractManufacturerString(
                    "", "", "", "VirMIDI", "Virtual MIDI Card 1"));
}

TEST(MidiManagerAlsaTest, JSONPortMetadata) {
  snd_seq_addr_t address;
  address.client = 1;
  address.port = 2;

  MidiManagerAlsa::AlsaPortMetadata input(
      "path", "bus", "id", &address, "client_name", "port_name", "card_name",
      "card_longname", MidiManagerAlsa::AlsaPortMetadata::Type::kInput);

  MidiManagerAlsa::AlsaPortMetadata output(
      "path", "bus", "id", &address, "client_name", "port_name", "card_name",
      "card_longname", MidiManagerAlsa::AlsaPortMetadata::Type::kOutput);

  ASSERT_EQ(
      "{\"bus\":\"bus\",\"cardLongname\":\"card_longname\",\"cardName\":\"card_"
      "name\","
      "\"clientAddr\":1,\"clientName\":\"client_name\",\"id\":\"id\",\"path\":"
      "\"path\","
      "\"portAddr\":2,\"portName\":\"port_name\",\"type\":\"input\"}",
      input.JSONValue());

  ASSERT_EQ("6D6186ACF60BB2FD26B5D2E21881CF0541FDB80FAC5BDFFA95CD55739E3BC526",
            input.OpaqueKey());

  ASSERT_EQ(
      "{\"bus\":\"bus\",\"cardLongname\":\"card_longname\",\"cardName\":\"card_"
      "name\","
      "\"clientAddr\":1,\"clientName\":\"client_name\",\"id\":\"id\",\"path\":"
      "\"path\","
      "\"portAddr\":2,\"portName\":\"port_name\",\"type\":\"output\"}",
      output.JSONValue());
  ASSERT_EQ("747E553D40F8388A0C1C51261B82869D5EFA8A54860AAFB2F4F7437744982495",
            output.OpaqueKey());
}

}  // namespace media
