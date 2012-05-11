// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PPAPI_PLUGIN_PLUGIN_PROCESS_DISPATCHER_H_
#define CONTENT_PPAPI_PLUGIN_PLUGIN_PROCESS_DISPATCHER_H_

#include "base/basictypes.h"
#include "ppapi/proxy/plugin_dispatcher.h"

// Wrapper around a PluginDispatcher that provides the necessary integration
// for plugin process management. This class is to avoid direct dependencies
// from the PPAPI proxy on the Chrome multiprocess infrastructure.
class PluginProcessDispatcher : public ppapi::proxy::PluginDispatcher {
 public:
  PluginProcessDispatcher(PP_GetInterface_Func get_interface,
                          bool incognito);
  virtual ~PluginProcessDispatcher();

 private:
  DISALLOW_COPY_AND_ASSIGN(PluginProcessDispatcher);
};

#endif  // CONTENT_PPAPI_PLUGIN_PLUGIN_PROCESS_DISPATCHER_H_
