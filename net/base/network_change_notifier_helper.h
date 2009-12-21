// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NetworkChangeNotifierHelper is a helper class that assists in implementing
// base functionality for a NetworkChangeNotifier implementation.  In
// particular, it manages adding/removing observers and sending them
// notifications of event changes.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_HELPER_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_HELPER_H_

#include <vector>
#include "base/basictypes.h"
#include "net/base/network_change_notifier.h"

namespace net {

namespace internal {

class NetworkChangeNotifierHelper {
 public:
  typedef NetworkChangeNotifier::Observer Observer;

  NetworkChangeNotifierHelper();
  ~NetworkChangeNotifierHelper();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void OnIPAddressChanged();

 private:
  bool is_notifying_observers_;
  std::vector<Observer*> observers_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierHelper);
};

}  // namespace internal

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_HELPER_H_
