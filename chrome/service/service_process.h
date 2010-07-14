// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_SERVICE_PROCESS_H_
#define CHROME_SERVICE_SERVICE_PROCESS_H_

#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/thread.h"

class CloudPrintProxy;
class JsonPrefStore;
namespace net {
class NetworkChangeNotifier;
}

namespace remoting {
class ChromotingHost;
class ChromotingHostContext;
class MutableHostConfig;
}

// The ServiceProcess does not inherit from ChildProcess because this
// process can live independently of the browser process.
class ServiceProcess {
 public:
  ServiceProcess();
  ~ServiceProcess();

  bool Initialize();
  bool Teardown();
  // TODO(sanjeevr): Change various parts of the code such as
  // net::ProxyService::CreateSystemProxyConfigService to take in
  // MessageLoopProxy* instead of MessageLoop*. When we have done that, we can
  // remove the io_thread() and file_thread() accessors and replace them with
  // io_message_loop_proxy() and file_message_loop_proxy() respectively.

  // Returns the thread that we perform I/O coordination on (network requests,
  // communication with renderers, etc.
  // NOTE: You should ONLY use this to pass to IPC or other objects which must
  // need a MessageLoop*. If you just want to post a task, use the thread's
  // message_loop_proxy() as it takes care of checking that a thread is still
  // alive, race conditions, lifetime differences etc.
  // If you still must use this, need to check the return value for NULL.
  base::Thread* io_thread() const {
    return io_thread_.get();
  }
  // Returns the thread that we perform random file operations on. For code
  // that wants to do I/O operations (not network requests or even file: URL
  // requests), this is the thread to use to avoid blocking the UI thread.
  base::Thread* file_thread() const {
    return file_thread_.get();
  }
  CloudPrintProxy* CreateCloudPrintProxy(JsonPrefStore* service_prefs);
#if defined(ENABLE_REMOTING)
  remoting::ChromotingHost* CreateChromotingHost(
      remoting::ChromotingHostContext* context,
      remoting::MutableHostConfig* config);
#endif

 private:
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  scoped_ptr<base::Thread> io_thread_;
  scoped_ptr<base::Thread> file_thread_;
  std::vector<CloudPrintProxy*> cloud_print_proxy_list_;

  DISALLOW_COPY_AND_ASSIGN(ServiceProcess);
};

extern ServiceProcess* g_service_process;

#endif  // CHROME_SERVICE_SERVICE_PROCESS_H_

