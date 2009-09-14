// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_BASE_ASYNC_DNS_LOOKUP_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_BASE_ASYNC_DNS_LOOKUP_H_

#include <vector>

#include "base/scoped_ptr.h"
#include "talk/base/signalthread.h"

namespace talk_base {
class SocketAddress;
class Task;
}

namespace notifier {

class AsyncDNSLookup : public talk_base::SignalThread {
 public:
  explicit AsyncDNSLookup(const talk_base::SocketAddress& server);
  virtual ~AsyncDNSLookup();

  const int error() const {
    return error_;
  }

  const std::vector<uint32>& ip_list() const {
    return ip_list_;
  }

 protected:
  // SignalThread Interface.
  virtual void DoWork();
  virtual void OnMessage(talk_base::Message* message);

 private:
  void OnTimeout();

  scoped_ptr<talk_base::SocketAddress> server_;
  talk_base::CriticalSection cs_;
  int error_;
  std::vector<uint32> ip_list_;

  DISALLOW_COPY_AND_ASSIGN(AsyncDNSLookup);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_BASE_ASYNC_DNS_LOOKUP_H_
