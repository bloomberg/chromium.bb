// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NET_NETWORK_CHANGE_NOTIFIER_PROXY_H_
#define CHROME_BROWSER_SYNC_NET_NETWORK_CHANGE_NOTIFIER_PROXY_H_

// NetworkChangeNotifierProxy is a class that lets observers listen to
// a NetworkChangeNotifier that lives on another thread.

#include "base/basictypes.h"
#include "base/non_thread_safe.h"
#include "base/observer_list.h"
#include "base/ref_counted.h"
#include "net/base/network_change_notifier.h"

class MessageLoop;

namespace browser_sync {

class NetworkChangeObserverProxy;

class NetworkChangeNotifierProxy : public net::NetworkChangeNotifier {
 public:
  // Both source_thread and source_network_change_notifier must be
  // guaranteed to outlive the current thread.  Does not take
  // ownership of any arguments.
  NetworkChangeNotifierProxy(
      MessageLoop* source_message_loop,
      net::NetworkChangeNotifier* source_network_change_notifier);

  virtual ~NetworkChangeNotifierProxy();

  // net::NetworkChangeNotifier implementation.

  virtual void AddObserver(net::NetworkChangeNotifier::Observer* observer);

  virtual void RemoveObserver(net::NetworkChangeNotifier::Observer* observer);

 private:
  typedef ObserverList<net::NetworkChangeNotifier::Observer, true>
      NetworkObserverList;

  // Utility class that routes received notifications to a list of
  // observers.
  class ObserverRepeater : public NonThreadSafe,
                           public net::NetworkChangeNotifier::Observer {
   public:
    // Does not take ownership of the given observer list.
    explicit ObserverRepeater(NetworkObserverList* observers);

    virtual ~ObserverRepeater();

    // net::NetworkChangeNotifier::Observer implementation.
    virtual void OnIPAddressChanged();

   private:
    NetworkObserverList* observers_;

    DISALLOW_COPY_AND_ASSIGN(ObserverRepeater);
  };

  scoped_refptr<NetworkChangeObserverProxy> observer_proxy_;
  NetworkObserverList observers_;
  ObserverRepeater observer_repeater_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierProxy);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_NET_NETWORK_CHANGE_NOTIFIER_PROXY_H_
