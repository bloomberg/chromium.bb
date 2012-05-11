// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PPAPI_PLUGIN_BROKER_PROCESS_DISPATCHER_H_
#define CONTENT_PPAPI_PLUGIN_BROKER_PROCESS_DISPATCHER_H_

#include "base/basictypes.h"
#include "ppapi/c/ppp.h"
#include "ppapi/proxy/broker_dispatcher.h"

// Wrapper around a BrokerDispatcher that provides the necessary integration
// for plugin process management. This class is to avoid direct dependencies
// from the PPAPI proxy on the Chrome multiprocess infrastructure.
class BrokerProcessDispatcher : public ppapi::proxy::BrokerSideDispatcher {
 public:
  BrokerProcessDispatcher(PP_GetInterface_Func get_plugin_interface,
                          PP_ConnectInstance_Func connect_instance);
  virtual ~BrokerProcessDispatcher();

  // IPC::Channel::Listener overrides.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

 private:
  void OnMsgClearSiteData(const FilePath& plugin_data_path,
                          const std::string& site,
                          uint64 flags,
                          uint64 max_age);

  // Requests that the plugin clear data, returning true on success.
  bool ClearSiteData(const FilePath& plugin_data_path,
                     const std::string& site,
                     uint64 flags,
                     uint64 max_age);

  PP_GetInterface_Func get_plugin_interface_;

  DISALLOW_COPY_AND_ASSIGN(BrokerProcessDispatcher);
};

#endif  // CONTENT_PPAPI_PLUGIN_BROKER_PROCESS_DISPATCHER_H_
