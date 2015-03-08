// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_alsa.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(MidiManagerAlsaTest, UdevEscape) {
  ASSERT_EQ("", MidiManagerAlsa::CardInfo::UnescapeUdev(""));
  ASSERT_EQ("\\", MidiManagerAlsa::CardInfo::UnescapeUdev("\\x5c"));
  ASSERT_EQ("\\x5", MidiManagerAlsa::CardInfo::UnescapeUdev("\\x5"));
  ASSERT_EQ("049f", MidiManagerAlsa::CardInfo::UnescapeUdev("049f"));
  ASSERT_EQ("HD Pro Webcam C920",
            MidiManagerAlsa::CardInfo::UnescapeUdev(
                "HD\\x20Pro\\x20Webcam\\x20C920"));
  ASSERT_EQ("E-MU Systems,Inc.",
            MidiManagerAlsa::CardInfo::UnescapeUdev(
                "E-MU\\x20Systems\\x2cInc."));
}

TEST(MidiManagerAlsaTest, ExtractManufacturer) {
  ASSERT_EQ("My Vendor",
            MidiManagerAlsa::CardInfo::ExtractManufacturerString(
                "My\\x20Vendor",
                "1234",
                "My Vendor, Inc.",
                "Card",
                "My Vendor Inc Card at bus"));
  ASSERT_EQ("My Vendor, Inc.",
            MidiManagerAlsa::CardInfo::ExtractManufacturerString(
                "1234",
                "1234",
                "My Vendor, Inc.",
                "Card",
                "My Vendor Inc Card at bus"));
  ASSERT_EQ("My Vendor Inc",
            MidiManagerAlsa::CardInfo::ExtractManufacturerString(
                "1234",
                "1234",
                "",
                "Card",
                "My Vendor Inc Card at bus"));
  ASSERT_EQ("My Vendor Inc",
            MidiManagerAlsa::CardInfo::ExtractManufacturerString(
                "",
                "",
                "",
                "Card",
                "My Vendor Inc Card at bus"));
  ASSERT_EQ("",
            MidiManagerAlsa::CardInfo::ExtractManufacturerString("1234",
                                                                 "1234",
                                                                 "",
                                                                 "Card",
                                                                 "Longname"));
  ASSERT_EQ("Keystation Mini 32",
            MidiManagerAlsa::CardInfo::ExtractManufacturerString(
                "Keystation\\x20Mini\\x2032",
                "129d",
                "Evolution Electronics, Ltd",
                "Keystation Mini 32",
                "Keystation Mini 32 Keystation Mini 32 at"
                " usb-0000:00:14.0-2.4.4, full speed"));
  ASSERT_EQ("Keystation Mini 32",
            MidiManagerAlsa::CardInfo::ExtractManufacturerString(
                "",
                "",
                "",
                "Keystation Mini 32",
                "Keystation Mini 32 Keystation Mini 32 at"
                " usb-0000:00:14.0-2.4.4, full speed"));
}

}  // namespace media
