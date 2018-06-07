// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_DISCOVERY_NETBIOS_CLIENT_INTERFACE_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_DISCOVERY_NETBIOS_CLIENT_INTERFACE_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"

namespace net {

class IPAddress;
class IPEndPoint;

}  // namespace net

namespace chromeos {
namespace smb_client {

using NetBiosResponseCallback = base::RepeatingCallback<
    void(const std::vector<uint8_t>&, uint16_t, const net::IPEndPoint&)>;

class NetBiosClientInterface {
 public:
  virtual ~NetBiosClientInterface() = default;

  // Starts the Name Query Request process. Any response packets that match
  // |transaction_id| are passed to |callback|.
  virtual void ExecuteNameRequest(const net::IPAddress& broadcast_address,
                                  uint16_t transaction_id,
                                  NetBiosResponseCallback callback) = 0;

 protected:
  NetBiosClientInterface() = default;

  DISALLOW_COPY_AND_ASSIGN(NetBiosClientInterface);
};

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_DISCOVERY_NETBIOS_CLIENT_INTERFACE_H_
