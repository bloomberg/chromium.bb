// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_NETWORK_NETWORK_STATUS_LISTENER_MAC_H_
#define COMPONENTS_DOWNLOAD_NETWORK_NETWORK_STATUS_LISTENER_MAC_H_

#include "base/macros.h"
#include "components/download/network/network_status_listener.h"

namespace download {

// Mac implementation of NetworkStatusListener. Always treat Mac device as
// connected to internet.
// We should investigate if platform code can be correctly hooked to
// net::NetworkChangeNotifier on Mac. See https://crbug.com/825878.
class NetworkStatusListenerMac : public NetworkStatusListener {
 public:
  NetworkStatusListenerMac();
  ~NetworkStatusListenerMac() override;

 private:
  // NetworkStatusListener implementation.
  void Start(NetworkStatusListener::Observer* observer) override;
  void Stop() override;
  network::mojom::ConnectionType GetConnectionType() override;

  DISALLOW_COPY_AND_ASSIGN(NetworkStatusListenerMac);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_NETWORK_NETWORK_STATUS_LISTENER_MAC_H_
