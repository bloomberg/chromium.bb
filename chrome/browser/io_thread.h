// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IO_THREAD_H_
#define CHROME_BROWSER_IO_THREAD_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/browser_process_sub_thread.h"
#include "chrome/common/net/dns.h"
#include "net/base/host_resolver.h"

class ChromeNetLog;
class ListValue;

namespace chrome_browser_net {
class DnsMaster;
}  // namespace chrome_browser_net

namespace net {
class HttpAuthHandlerFactory;
class NetworkChangeNotifier;
}  // namespace net

class IOThread : public BrowserProcessSubThread {
 public:
  struct Globals {
    scoped_ptr<ChromeNetLog> net_log;
    scoped_ptr<net::NetworkChangeNotifier> network_change_notifier;
    // TODO(willchan): Stop reference counting HostResolver.  It's owned by
    // IOThread now.
    scoped_refptr<net::HostResolver> host_resolver;
    scoped_ptr<net::HttpAuthHandlerFactory> http_auth_handler_factory;
  };

  IOThread();

  virtual ~IOThread();

  // Can only be called on the IO thread.
  Globals* globals();

  // Initializes the DnsMaster.  |prefetching_enabled| indicates whether or
  // not dns prefetching should be enabled.  This should be called by the UI
  // thread.  It will post a task to the IO thread to perform the actual
  // initialization.
  void InitDnsMaster(bool prefetching_enabled,
                     base::TimeDelta max_queue_delay,
                     size_t max_concurrent,
                     const chrome_common_net::NameList& hostnames_to_prefetch,
                     ListValue* referral_list);

  // Handles changing to On The Record mode.  Posts a task for this onto the
  // IOThread's message loop.
  void ChangedToOnTheRecord();

 protected:
  virtual void Init();
  virtual void CleanUp();

 private:
  net::HttpAuthHandlerFactory* CreateDefaultAuthHandlerFactory();

  void InitDnsMasterOnIOThread(
      bool prefetching_enabled,
      base::TimeDelta max_queue_delay,
      size_t max_concurrent,
      chrome_common_net::NameList hostnames_to_prefetch,
      ListValue* referral_list);

  void ChangedToOnTheRecordOnIOThread();

  // These member variables are basically global, but their lifetimes are tied
  // to the IOThread.  IOThread owns them all, despite not using scoped_ptr.
  // This is because the destructor of IOThread runs on the wrong thread.  All
  // member variables should be deleted in CleanUp().

  // These member variables are initialized in Init() and do not change for the
  // lifetime of the IO thread.

  Globals* globals_;

  // These member variables are initialized by a task posted to the IO thread,
  // which gets posted by calling certain member functions of IOThread.

  net::HostResolver::Observer* prefetch_observer_;
  chrome_browser_net::DnsMaster* dns_master_;

  DISALLOW_COPY_AND_ASSIGN(IOThread);
};

#endif  // CHROME_BROWSER_IO_THREAD_H_
