// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NET_NETWORK_CHANGE_NOTIFIER_THREAD_H_
#define CHROME_BROWSER_SYNC_NET_NETWORK_CHANGE_NOTIFIER_THREAD_H_

// A simple interface that represents a thread which owns a
// NetworkChangeNotifier.

class MessageLoop;

namespace net {
class NetworkChangeNotifier;
}  // namespace net

namespace browser_sync {

// An instance of this interface must live no longer than the thread
// it represents and its message loop and network change notifier.
class NetworkChangeNotifierThread {
 public:
  virtual ~NetworkChangeNotifierThread() {}

  // Returns the message loop for the thread that owns the
  // NetworkChangeNotifier.  Can be called on any thread.
  virtual MessageLoop* GetMessageLoop() const = 0;

  // Returns the NetworkChangeNotifier of the thread.  This method
  // must be called only from the owning thread (i.e., by posting a
  // task onto the message loop returned by GetMessageLoop()).
  virtual net::NetworkChangeNotifier* GetNetworkChangeNotifier() const = 0;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_NET_NETWORK_CHANGE_NOTIFIER_THREAD_H_
