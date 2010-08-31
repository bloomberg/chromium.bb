// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_FORWARDING_NET_LOG_H_
#define NET_BASE_FORWARDING_NET_LOG_H_
#pragma once

#include "base/basictypes.h"
#include "net/base/net_log.h"

class MessageLoop;

namespace net {

// ForwardingNetLog is a wrapper that can be called on any thread, and will
// forward any calls to NetLog::AddEntry() over to |impl| on the specified
// message loop.
//
// This allows using a non-threadsafe NetLog implementation from another
// thread.
//
// TODO(eroman): Explore making NetLog threadsafe and obviating the need
//               for this class.
class ForwardingNetLog : public NetLog {
 public:
  // Both |impl| and |loop| must outlive the lifetime of this instance.
  // |impl| will be operated only from |loop|.
  ForwardingNetLog(NetLog* impl, MessageLoop* loop);

  // On destruction any outstanding call to AddEntry() which didn't make
  // it to |loop| yet will be cancelled.
  ~ForwardingNetLog();

  // NetLog methods:
  virtual void AddEntry(EventType type,
                        const base::TimeTicks& time,
                        const Source& source,
                        EventPhase phase,
                        EventParameters* params);
  virtual uint32 NextID();
  virtual LogLevel GetLogLevel() const;

 private:
  class Core;
  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(ForwardingNetLog);
};

}  // namespace net

#endif  // NET_BASE_FORWARDING_NET_LOG_H_

