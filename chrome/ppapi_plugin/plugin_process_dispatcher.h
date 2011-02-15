// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PPAPI_PLUGIN_PLUGIN_PROCESS_DISPATCHER_H_
#define CHROME_PPAPI_PLUGIN_PLUGIN_PROCESS_DISPATCHER_H_

#include "base/basictypes.h"
#include "ppapi/proxy/plugin_dispatcher.h"

// Wrapper around a PluginDispatcher that provides the necessary integration
// for plugin process management. This class is to avoid direct dependencies
// from the PPAPI proxy on the Chrome multiprocess infrastructure.
class PluginProcessDispatcher : public pp::proxy::PluginDispatcher {
 public:
  PluginProcessDispatcher(base::ProcessHandle remote_process_handle,
                          GetInterfaceFunc get_interface);
  virtual ~PluginProcessDispatcher();

 private:
  DISALLOW_COPY_AND_ASSIGN(PluginProcessDispatcher);
};

#endif  // CHROME_PPAPI_PLUGIN_PLUGIN_PROCESS_DISPATCHER_H_
