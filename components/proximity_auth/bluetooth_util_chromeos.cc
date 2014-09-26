// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/bluetooth_util.h"

#include <stdint.h>
#include <sys/socket.h>
#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/sys_byteorder.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_device_chromeos.h"
#include "net/socket/socket_descriptor.h"

// The bluez headers are (intentionally) not available within the Chromium
// repository, so several definitions from these headers are replicated below.

// From <bluetooth/bluetooth.h>:
#define BTPROTO_L2CAP 0
struct bdaddr_t {
  uint8_t b[6];
} __attribute__((packed));

// From <bluetooth/l2cap.h>:
struct sockaddr_l2 {
  sa_family_t l2_family;
  unsigned short l2_psm;
  bdaddr_t l2_bdaddr;
  unsigned short l2_cid;
};

// From <bluetooth/sdp.h>:
#define SDP_PSM 0x0001

namespace proximity_auth {
namespace bluetooth_util {
namespace {

using device::BluetoothDevice;

const char kInvalidDeviceAddress[] =
    "Given address is not a valid Bluetooth device.";
const char kUnableToConnectToDevice[] =
    "Unable to connect to the remote device.";

// Delay prior to closing an SDP connection opened to register a Bluetooth
// device with the system BlueZ daemon.
const int kCloseSDPConnectionDelaySec = 5;

struct SeekDeviceResult {
  // Whether the connection to the device succeeded.
  bool success;

  // If the connection failed, an error message describing the failure.
  std::string error_message;
};

// Writes |address| into the |result|. Return true on success, false if the
// |address| is not a valid Bluetooth address.
bool BluetoothAddressToBdaddr(const std::string& address, bdaddr_t* result) {
  std::string canonical_address = BluetoothDevice::CanonicalizeAddress(address);
  if (canonical_address.empty())
    return false;

  std::vector<std::string> octets;
  base::SplitString(canonical_address, ':', &octets);
  DCHECK_EQ(octets.size(), 6U);

  // BlueZ expects the octets in the reverse order.
  std::reverse(octets.begin(), octets.end());
  for (size_t i = 0; i < octets.size(); ++i) {
    uint32_t octet;
    bool success = base::HexStringToUInt(octets[i], &octet);
    DCHECK(success);
    result->b[i] = base::checked_cast<uint8_t>(octet);
  }

  return true;
}

// Closes the socket with the given |socket_descriptor|.
void CloseSocket(net::SocketDescriptor socket_descriptor) {
  int result = close(socket_descriptor);
  DCHECK_EQ(result, 0);
}

// Connects to the SDP service on the Bluetooth device with the given
// |device_address|, if possible. Returns an indicator of success or an error
// message on failure.
SeekDeviceResult SeekDeviceByAddressImpl(
    const std::string& device_address,
    scoped_refptr<base::TaskRunner> task_runner) {
  SeekDeviceResult seek_result;
  seek_result.success = false;

  struct sockaddr_l2 addr;
  memset(&addr, 0, sizeof(addr));
  addr.l2_family = AF_BLUETOOTH;
  addr.l2_psm = base::ByteSwapToLE16(SDP_PSM);
  if (!BluetoothAddressToBdaddr(device_address, &addr.l2_bdaddr)) {
    seek_result.error_message = kInvalidDeviceAddress;
    return seek_result;
  }

  net::SocketDescriptor socket_descriptor =
      socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
  int result = connect(socket_descriptor,
                       reinterpret_cast<struct sockaddr*>(&addr),
                       sizeof(addr));
  if (result == 0) {
    seek_result.success = true;
    task_runner->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CloseSocket, socket_descriptor),
        base::TimeDelta::FromSeconds(kCloseSDPConnectionDelaySec));
  } else {
    // TODO(isherman): Pass a better message based on the errno?
    seek_result.error_message = kUnableToConnectToDevice;
  }
  return seek_result;
}

void OnSeekDeviceResult(const base::Closure& callback,
                        const ErrorCallback& error_callback,
                        const SeekDeviceResult& result) {
  if (result.success)
    callback.Run();
  else
    error_callback.Run(result.error_message);
}

}  // namespace

void SeekDeviceByAddress(const std::string& device_address,
                         const base::Closure& callback,
                         const ErrorCallback& error_callback,
                         base::TaskRunner* task_runner) {
  base::PostTaskAndReplyWithResult(
      task_runner,
      FROM_HERE,
      base::Bind(&SeekDeviceByAddressImpl,
                 device_address,
                 make_scoped_refptr(task_runner)),
      base::Bind(&OnSeekDeviceResult, callback, error_callback));
}

void ConnectToServiceInsecurely(
    device::BluetoothDevice* device,
    const device::BluetoothUUID& uuid,
    const BluetoothDevice::ConnectToServiceCallback& callback,
    const BluetoothDevice::ConnectToServiceErrorCallback& error_callback) {
  static_cast<chromeos::BluetoothDeviceChromeOS*>(device)
      ->ConnectToServiceInsecurely(uuid, callback, error_callback);
}

}  // namespace bluetooth_util
}  // namespace proximity_auth
