// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_BLE_BLUETOOTH_LOW_ENERGY_WEAVE_PACKET_GENERATOR_H_
#define COMPONENTS_PROXIMITY_AUTH_BLE_BLUETOOTH_LOW_ENERGY_WEAVE_PACKET_GENERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"

namespace proximity_auth {
// Generates the messages sent using the uWeave protocol.
class BluetoothLowEnergyWeavePacketGenerator {
 public:
  class Factory {
   public:
    static std::unique_ptr<BluetoothLowEnergyWeavePacketGenerator>
    NewInstance();

    // Exposed for testing.
    static void SetInstanceForTesting(Factory* factory);

   protected:
    // Exposed for testing.
    virtual std::unique_ptr<BluetoothLowEnergyWeavePacketGenerator>
    BuildInstance();

   private:
    static Factory* factory_instance_;
  };

  // Determine whether a packet is a control packet or a data packet.
  enum PacketType { DATA = 0x00, CONTROL = 0x01 };

  // Identify the action intended by the control packet.
  enum ControlCommand {
    CONNECTION_REQUEST = 0x00,
    CONNECTION_RESPONSE = 0x01,
    CONNECTION_CLOSE = 0x02
  };

  // Sent with the ConnectionClose control packet.
  // Identify why the client/server wished to close the connection.
  enum ReasonForClose {
    CLOSE_WITHOUT_ERROR = 0x00,
    UNKNOWN_ERROR = 0x01,
    NO_COMMON_VERSION_SUPPORTED = 0x02,
    RECEIVED_PACKET_OUT_OF_SEQUENCE = 0x03,
    APPLICATION_ERROR = 0x80
  };

  typedef std::vector<uint8_t> Packet;

  Packet CreateConnectionRequest();
  Packet CreateConnectionResponse();
  Packet CreateConnectionClose(ReasonForClose reason_for_close);

  // Packet size must be greater than or equal to 20.
  void SetMaxPacketSize(uint32_t size);

  // Will crash if message is empty.
  std::vector<Packet> EncodeDataMessage(std::string message);

 protected:
  BluetoothLowEnergyWeavePacketGenerator();

 private:
  void SetShortField(uint32_t byte_offset, uint16_t val, Packet* packet);
  void SetPacketTypeBit(PacketType val, Packet* packet);
  void SetControlCommand(ControlCommand val, Packet* packet);
  void SetPacketCounter(Packet* packet);
  void SetDataFirstBit(Packet* packet);
  void SetDataLastBit(Packet* packet);

  // The default max packet length is 20 unless SetDataPacketLength() is called
  // and specified otherwise.
  uint32_t max_packet_size_;

  // Counter for the number of packets sent, starting at 0.
  uint32_t next_packet_counter_;
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_BLE_BLUETOOTH_LOW_ENERGY_WEAVE_PACKET_GENERATOR_H_
