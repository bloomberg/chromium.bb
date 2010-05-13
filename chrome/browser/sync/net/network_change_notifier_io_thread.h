// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NET_NETWORK_CHANGE_NOTIFIER_IO_THREAD_H_
#define CHROME_BROWSER_SYNC_NET_NETWORK_CHANGE_NOTIFIER_IO_THREAD_H_

// This is a simple NetworkChangeNotifierThread wrapper around an
// IOThread and its NetworkChangeNotifier.

#include "base/basictypes.h"
#include "chrome/common/net/network_change_notifier_thread.h"

class IOThread;
class MessageLoop;

class NetworkChangeNotifierIOThread
    : public chrome_common_net::NetworkChangeNotifierThread {
 public:
  // Does not take ownership of |io_thread|.  This instance must live
  // no longer than |io_thread|.
  explicit NetworkChangeNotifierIOThread(IOThread* io_thread);

  virtual ~NetworkChangeNotifierIOThread();

  // chrome_common_net::NetworkChangeNotifierThread implementation.

  virtual MessageLoop* GetMessageLoop() const;

  virtual net::NetworkChangeNotifier* GetNetworkChangeNotifier() const;

 private:
  IOThread* const io_thread_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierIOThread);
};

#endif  // CHROME_BROWSER_SYNC_NET_NETWORK_CHANGE_NOTIFIER_IO_THREAD_H_
