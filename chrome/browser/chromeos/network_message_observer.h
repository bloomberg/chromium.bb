// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NETWORK_MESSAGE_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_NETWORK_MESSAGE_OBSERVER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/cros/network_library.h"

class Profile;

namespace chromeos {

// The network message observer displays a system notification for network
// messages.

class NetworkMessageNotification;

class NetworkMessageObserver
  : public NetworkLibrary::NetworkManagerObserver,
    public NetworkLibrary::UserActionObserver,
    public base::SupportsWeakPtr<NetworkMessageObserver> {
 public:
  explicit NetworkMessageObserver(Profile* profile);
  virtual ~NetworkMessageObserver();

 private:
  // NetworkLibrary::NetworkManagerObserver implementation.
  virtual void OnNetworkManagerChanged(NetworkLibrary* obj) OVERRIDE;

  // NetworkLibrary::UserActionObserver implementation.
  virtual void OnConnectionInitiated(NetworkLibrary* obj,
                                     const Network* network) OVERRIDE;

  typedef std::map<std::string, ConnectionState> NetworkStateMap;

  // Notification for connection errors
  scoped_ptr<NetworkMessageNotification> notification_connection_error_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMessageObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NETWORK_MESSAGE_OBSERVER_H_
