// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_alsa.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(MidiManagerAlsaTest, ExtractManufacturer) {
  ASSERT_EQ("My\\x20Vendor",
            MidiManagerAlsa::MidiDevice::ExtractManufacturerString(
                "My\\x20Vendor", "1234", "My Vendor, Inc.", "Card",
                "My Vendor Inc Card at bus"));
  ASSERT_EQ("My Vendor", MidiManagerAlsa::MidiDevice::ExtractManufacturerString(
                             "My Vendor", "1234", "My Vendor, Inc.", "Card",
                             "My Vendor Inc Card at bus"));
  ASSERT_EQ("My Vendor, Inc.",
            MidiManagerAlsa::MidiDevice::ExtractManufacturerString(
                "1234", "1234", "My Vendor, Inc.", "Card",
                "My Vendor Inc Card at bus"));
  ASSERT_EQ("My Vendor Inc",
            MidiManagerAlsa::MidiDevice::ExtractManufacturerString(
                "1234", "1234", "", "Card", "My Vendor Inc Card at bus"));
  ASSERT_EQ("My Vendor Inc",
            MidiManagerAlsa::MidiDevice::ExtractManufacturerString(
                "", "", "", "Card", "My Vendor Inc Card at bus"));
  ASSERT_EQ("", MidiManagerAlsa::MidiDevice::ExtractManufacturerString(
                    "1234", "1234", "", "Card", "Longname"));
  ASSERT_EQ("Keystation\\x20Mini\\x2032",
            MidiManagerAlsa::MidiDevice::ExtractManufacturerString(
                "Keystation\\x20Mini\\x2032", "129d",
                "Evolution Electronics, Ltd", "Keystation Mini 32",
                "Keystation Mini 32 Keystation Mini 32 at"
                " usb-0000:00:14.0-2.4.4, full speed"));
  ASSERT_EQ("Keystation Mini 32",
            MidiManagerAlsa::MidiDevice::ExtractManufacturerString(
                "Keystation Mini 32", "129d", "Evolution Electronics, Ltd",
                "Keystation Mini 32",
                "Keystation Mini 32 Keystation Mini 32 at"
                " usb-0000:00:14.0-2.4.4, full speed"));
  ASSERT_EQ("Keystation Mini 32",
            MidiManagerAlsa::MidiDevice::ExtractManufacturerString(
                "", "", "", "Keystation Mini 32",
                "Keystation Mini 32 Keystation Mini 32 at"
                " usb-0000:00:14.0-2.4.4, full speed"));
}

}  // namespace media
