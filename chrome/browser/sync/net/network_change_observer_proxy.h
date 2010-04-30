// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NET_NETWORK_CHANGE_OBSERVER_PROXY_H_
#define CHROME_BROWSER_SYNC_NET_NETWORK_CHANGE_OBSERVER_PROXY_H_

// NetworkChangeObserverProxy is a class that listens to a
// NetworkChangeNotifier on one thread (the source thread, which is
// usually the Chrome IO thread) and emits events to a target observer
// on another thread (the target thread).
//
// How to use:
//
// In the target thread, create the observer proxy:
//
//   NetworkChangeObserverProxy* proxy =
//       new NetworkChangeObserverProxy(source_thread->message_loop(),
//                                      source_network_change_notifier,
//                                      MessageLoop::current());
//
// Both source_thread and source_network_change_notifier must be
// guaranteed to outlive the target (current) thread.
//
// Then, attach the target observer:
//
//   proxy->Attach(target_observer);
//
// target_observer will then begin to receive events on the target
// thread.
//
// If you call Attach(), you *must* call Detach() before releasing:
//
//   proxy->Detach();
//   proxy->Release();  // omit if proxy is a scoped_refptr
//   proxy = NULL;
//
// The proxy may be destroyed on either the source or the target
// thread (depending on which one ends up holding the last reference).

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "net/base/network_change_notifier.h"

namespace browser_sync {

class NetworkChangeObserverProxy
    : public base::RefCountedThreadSafe<NetworkChangeObserverProxy>,
      private net::NetworkChangeNotifier::Observer {
 public:
  // All public methods (including the constructor) must be called on
  // the target thread.

  // Does not take ownership of any arguments.
  NetworkChangeObserverProxy(
      MessageLoop* source_message_loop,
      net::NetworkChangeNotifier* source_network_change_notifier,
      MessageLoop* target_message_loop);

  // After this method is called, |target_observer| will start
  // receiving events on the target thread.  Once called, Detach()
  // must be called before releasing and you cannot call Attach()
  // again until you have done so.  Does not take ownership of
  // |target_observer|.
  void Attach(net::NetworkChangeNotifier::Observer* target_observer);

  // After this method is called, the target observer will stop
  // receiving events.  You can call Attach() again after this method
  // is called.
  void Detach();

 protected:
  // May be called on either the source or the target thread.  Marked
  // protected instead of private so that this class can be subclassed
  // for unit tests.
  virtual ~NetworkChangeObserverProxy();

 private:
  friend class base::RefCountedThreadSafe<NetworkChangeObserverProxy>;

  // Adds ourselves as an observer of
  // |source_network_change_notifier_|.  Must be called on the source
  // thread.
  void Observe();

  // Removes ourselves as an observer of
  // |source_network_change_notifier_|.  Must be called on the source
  // thread.
  void Unobserve();

  // net::NetworkChangeNotifier::Observer implementation.
  //
  // Called on the source thread.  Posts
  // TargetObserverOnIPAddressChanged() on the target thread.
  virtual void OnIPAddressChanged();

  // Called on the target thread.  Invokes OnIPAddressChanged() on
  // |target_observer_|.
  void TargetObserverOnIPAddressChanged();

  MessageLoop* const source_message_loop_;
  net::NetworkChangeNotifier* const source_network_change_notifier_;
  MessageLoop* const target_message_loop_;
  // |target_observer_| is used only by the target thread.
  net::NetworkChangeNotifier::Observer* target_observer_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeObserverProxy);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_NET_NETWORK_CHANGE_OBSERVER_PROXY_H_
