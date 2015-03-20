// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/privet_daemon_manager_client.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "dbus/message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(PrivetDaemonManagerClientTest, ReadPairingInfo) {
  scoped_ptr<dbus::Response> message(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(message.get());
  dbus::MessageWriter variant_writer(nullptr);
  dbus::MessageWriter variant_array_writer(nullptr);
  dbus::MessageWriter struct_entry_writer(nullptr);

  writer.OpenVariant("a{sv}", &variant_writer);
  variant_writer.OpenArray("{sv}", &variant_array_writer);

  const uint8_t code_value[] = {0x67, 0x12, 0x23, 0x45, 0x64};
  variant_array_writer.OpenDictEntry(&struct_entry_writer);
  struct_entry_writer.AppendString("code");
  dbus::MessageWriter struct_field_writer(nullptr);
  struct_entry_writer.OpenVariant("ay", &struct_field_writer);
  struct_field_writer.AppendArrayOfBytes(code_value, arraysize(code_value));
  struct_entry_writer.CloseContainer(&struct_field_writer);
  variant_array_writer.CloseContainer(&struct_entry_writer);

  const char* string_items[] = {"mode", "sessionId"};
  for (size_t i = 0; i < arraysize(string_items); ++i) {
    variant_array_writer.OpenDictEntry(&struct_entry_writer);
    struct_entry_writer.AppendString(string_items[i]);
    struct_entry_writer.AppendVariantOfString(base::UintToString(i + 1));
    variant_array_writer.CloseContainer(&struct_entry_writer);
  }
  writer.CloseContainer(&variant_writer);
  variant_writer.CloseContainer(&variant_array_writer);

  dbus::MessageReader reader(message.get());
  PrivetDaemonManagerClient::PairingInfoProperty pairing_info;
  EXPECT_TRUE(pairing_info.PopValueFromReader(&reader));

  EXPECT_EQ(
      std::vector<uint8_t>(code_value, code_value + arraysize(code_value)),
      pairing_info.value().code());
  EXPECT_EQ("1", pairing_info.value().mode());
  EXPECT_EQ("2", pairing_info.value().session_id());
}

}  // namespace chromeos
