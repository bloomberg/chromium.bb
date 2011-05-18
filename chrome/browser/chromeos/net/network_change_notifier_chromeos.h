// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_CHANGE_NOTIFIER_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_CHANGE_NOTIFIER_CHROMEOS_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "net/base/network_change_notifier.h"

namespace chromeos {

class NetworkChangeNotifierChromeos
    : public net::NetworkChangeNotifier,
      public chromeos::NetworkLibrary::NetworkManagerObserver,
      public chromeos::NetworkLibrary::NetworkObserver {
 public:
  NetworkChangeNotifierChromeos();
  virtual ~NetworkChangeNotifierChromeos();

 private:
  // NetworkChangeNotifier overrides.
  virtual bool IsCurrentlyOffline() const OVERRIDE;

  // NetworkManagerObserver overrides:
  virtual void OnNetworkManagerChanged(chromeos::NetworkLibrary* obj) OVERRIDE;

  // NetworkObserver overrides:
  virtual void OnNetworkChanged(chromeos::NetworkLibrary* cros,
                                const chromeos::Network* network) OVERRIDE;

  // Updates data members that keep the track the network stack state.
  void UpdateNetworkState(chromeos::NetworkLibrary* cros);

  // True if we previously had an active network around.
  bool has_active_network_;
  // Current active network's connectivity state.
  chromeos::ConnectivityState connectivity_state_;
  // Current active network's service path.
  std::string service_path_;
  // Current active network's IP address.
  std::string ip_address_;

  ScopedRunnableMethodFactory<NetworkChangeNotifierChromeos> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierChromeos);
};

}  // namespace net

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_CHANGE_NOTIFIER_CHROMEOS_H_
