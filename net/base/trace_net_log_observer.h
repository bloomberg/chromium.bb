// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TRACE_NET_LOG_OBSERVER_H_
#define NET_BASE_TRACE_NET_LOG_OBSERVER_H_

#include "base/debug/trace_event_impl.h"
#include "base/macros.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"

namespace net {

// TraceNetLogObserver watches for TraceLog enable, and sends NetLog
// events to TraceLog if it is enabled.
class NET_EXPORT TraceNetLogObserver
    : public NetLog::ThreadSafeObserver,
      public base::debug::TraceLog::EnabledStateObserver {
 public:
  TraceNetLogObserver();
  virtual ~TraceNetLogObserver();

  // net::NetLog::ThreadSafeObserver implementation:
  virtual void OnAddEntry(const NetLog::Entry& entry) OVERRIDE;

  // Start to watch for TraceLog enable and disable events.
  // This can't be called if already watching for events.
  // Watches NetLog only when tracing is enabled.
  void WatchForTraceStart(NetLog* net_log);

  // Stop watching for TraceLog enable and disable events.
  // If WatchForTraceStart is called, this must be called before
  // TraceNetLogObserver is destroyed.
  void StopWatchForTraceStart();

  // base::debug::TraceLog::EnabledStateChangedObserver implementation:
  virtual void OnTraceLogEnabled() OVERRIDE;
  virtual void OnTraceLogDisabled() OVERRIDE;

 private:
  NetLog* net_log_to_watch_;

  DISALLOW_COPY_AND_ASSIGN(TraceNetLogObserver);
};

}  // namespace net

#endif  // NET_BASE_TRACE_NET_LOG_OBSERVER_H_
