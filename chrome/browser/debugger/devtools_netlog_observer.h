// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_NETLOG_OBSERVER_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_NETLOG_OBSERVER_H_
#pragma once

#include "base/hash_tables.h"
#include "base/ref_counted.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "webkit/glue/resource_loader_bridge.h"

namespace net {
class URLRequest;
}  // namespace net

class IOThread;
struct ResourceResponse;

// DevToolsNetLogObserver watches the NetLog event stream and collects the
// stuff that may be of interest to DevTools. Currently, this only includes
// actual HTTP/SPDY headers sent and received over the network.
//
// As DevToolsNetLogObserver shares live data with objects that live on the
// IO Thread, it must also reside on the IO Thread.  Only OnAddEntry can be
// called from other threads.
class DevToolsNetLogObserver: public ChromeNetLog::ThreadSafeObserver {
  typedef webkit_glue::ResourceDevToolsInfo ResourceInfo;

 public:
  // ThreadSafeObserver implementation:
  virtual void OnAddEntry(net::NetLog::EventType type,
                          const base::TimeTicks& time,
                          const net::NetLog::Source& source,
                          net::NetLog::EventPhase phase,
                          net::NetLog::EventParameters* params);

  static void Attach(IOThread* thread);
  static void Detach();

  // Must be called on the IO thread. May return NULL if no observers
  // are active.
  static DevToolsNetLogObserver* GetInstance();
  static void PopulateResponseInfo(net::URLRequest*, ResourceResponse*);

 private:
  typedef base::hash_map<uint32, scoped_refptr<ResourceInfo> >
      RequestToInfoMap;

  static DevToolsNetLogObserver* instance_;

  explicit DevToolsNetLogObserver(ChromeNetLog* chrome_net_log);
  ~DevToolsNetLogObserver();

  ResourceInfo* GetResourceInfo(uint32 id);

  ChromeNetLog* chrome_net_log_;
  RequestToInfoMap request_to_info_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsNetLogObserver);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_NETLOG_OBSERVER_H_
