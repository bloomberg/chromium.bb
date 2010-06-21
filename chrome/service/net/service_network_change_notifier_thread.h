// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_NET_SERVICE_NETWORK_CHANGE_NOTIFIER_THREAD_H_
#define CHROME_SERVICE_NET_SERVICE_NETWORK_CHANGE_NOTIFIER_THREAD_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/common/net/network_change_notifier_thread.h"

class MessageLoop;

namespace net {
class NetworkChangeNotifier;
}  // namespace net

// This is a simple implementation of NetworkChangeNotifierThread.
// Note that Initialize MUST be called before this class can be used.
class ServiceNetworkChangeNotifierThread
    : public chrome_common_net::NetworkChangeNotifierThread,
      public base::RefCountedThreadSafe<ServiceNetworkChangeNotifierThread> {
 public:
  // Does not take ownership of |io_thread_message_loop|. This instance must
  // live no longer than |io_thread_message_loop|.
  // TODO(sanjeevr): Change NetworkChangeNotifierThread to use MessageLoopProxy
  explicit ServiceNetworkChangeNotifierThread(
      MessageLoop* io_thread_message_loop);
  virtual ~ServiceNetworkChangeNotifierThread();

  // Initialize MUST be called before this class can be used.
  void Initialize();

  // chrome_common_net::NetworkChangeNotifierThread implementation.

  virtual MessageLoop* GetMessageLoop() const;

  virtual net::NetworkChangeNotifier* GetNetworkChangeNotifier() const;

 private:
  MessageLoop* const io_thread_message_loop_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;

  void CreateNetworkChangeNotifier();
  DISALLOW_COPY_AND_ASSIGN(ServiceNetworkChangeNotifierThread);
};

#endif  // CHROME_SERVICE_NET_SERVICE_NETWORK_CHANGE_NOTIFIER_THREAD_H_

