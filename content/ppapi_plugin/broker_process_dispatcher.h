// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PPAPI_PLUGIN_BROKER_PROCESS_DISPATCHER_H_
#define CONTENT_PPAPI_PLUGIN_BROKER_PROCESS_DISPATCHER_H_

#include "base/basictypes.h"
#include "ppapi/proxy/broker_dispatcher.h"

// Wrapper around a BrokerDispatcher that provides the necessary integration
// for plugin process management. This class is to avoid direct dependencies
// from the PPAPI proxy on the Chrome multiprocess infrastructure.
class BrokerProcessDispatcher : public ppapi::proxy::BrokerSideDispatcher {
 public:
  BrokerProcessDispatcher(base::ProcessHandle remote_process_handle,
                          PP_ConnectInstance_Func connect_instance);
  virtual ~BrokerProcessDispatcher();

 private:
  DISALLOW_COPY_AND_ASSIGN(BrokerProcessDispatcher);
};

#endif  // CONTENT_PPAPI_PLUGIN_BROKER_PROCESS_DISPATCHER_H_
