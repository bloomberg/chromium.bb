// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/easy_unlock_private/easy_unlock_private_bluetooth_util.h"

#include <stdint.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/sdp.h>
#include <sys/socket.h>
#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/bluetooth_device.h"
#include "net/socket/socket_descriptor.h"

namespace extensions {
namespace api {
namespace easy_unlock {
namespace {

using device::BluetoothDevice;

const char kInvalidDeviceAddress[] =
    "Given address is not a valid Bluetooth device.";
const char kUnableToConnectToDevice[] =
    "Unable to connect to the remote device.";

// Delay prior to closing an SDP connection opened to register a Bluetooth
// device with the system BlueZ daemon.
const int kCloseSDPConnectionDelaySec = 5;

// Writes |address| into the |result|. Return true on success, false if the
// |address| is not a valid Bluetooth address.
bool BluetoothAddressToBdaddr(const std::string& address,
                              bdaddr_t* result) {
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
SeekDeviceResult SeekBluetoothDeviceByAddressImpl(
    const std::string& device_address) {
  base::TaskRunner* blocking_pool = content::BrowserThread::GetBlockingPool();
  DCHECK(blocking_pool->RunsTasksOnCurrentThread());

  SeekDeviceResult seek_result;
  seek_result.success = false;

  struct sockaddr_l2 addr;
  memset(&addr, 0, sizeof(addr));
  addr.l2_family = AF_BLUETOOTH;
  addr.l2_psm = htobs(SDP_PSM);
  if (!BluetoothAddressToBdaddr(device_address, &addr.l2_bdaddr)) {
    seek_result.error_message = kInvalidDeviceAddress;
    return seek_result;
  }

  net::SocketDescriptor socket_descriptor =
      socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
  int result = connect(socket_descriptor,
                       reinterpret_cast<struct sockaddr *>(&addr),
                       sizeof(addr));
  if (result == 0) {
    seek_result.success = true;
    blocking_pool->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CloseSocket, socket_descriptor),
        base::TimeDelta::FromSeconds(kCloseSDPConnectionDelaySec));
  } else {
    // TODO(isherman): Pass a better message based on the errno?
    seek_result.error_message = kUnableToConnectToDevice;
  }
  return seek_result;
}

}  // namespace

void SeekBluetoothDeviceByAddress(const std::string& device_address,
                                  const SeekDeviceCallback& callback) {
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&SeekBluetoothDeviceByAddressImpl, device_address),
      callback);
}

}  // namespace easy_unlock
}  // namespace api
}  // namespace extensions
