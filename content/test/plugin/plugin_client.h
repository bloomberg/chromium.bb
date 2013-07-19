// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_PLUGIN_PLUGIN_CLIENT_H_
#define CONTENT_TEST_PLUGIN_PLUGIN_CLIENT_H_

#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/nphostapi.h"

namespace NPAPIClient {

// A PluginClient is a NPAPI Plugin.  This class contains
// the bootstrapping functions used by the browser to load
// the plugin.
class PluginClient {
 public:
  // Although not documented in the NPAPI specification, this function
  // gets the list of entry points in the NPAPI Plugin (client) for the
  // NPAPI Host to call.
  static NPError GetEntryPoints(NPPluginFuncs* pFuncs);

  // The browser calls this function only once: when a plug-in is loaded,
  // before the first instance is created. This is the first function that
  // the browser calls. NP_Initialize tells the plug-in that the browser has
  // loaded it and provides global initialization. Allocate any memory or
  // resources shared by all instances of your plug-in at this time.
  static NPError Initialize(NPNetscapeFuncs* pFuncs);

  // The browser calls this function once after the last instance of your
  // plug-in is destroyed, before unloading the plug-in library itself. Use
  // NP_Shutdown to delete any data allocated in NP_Initialize to be shared
  // by all instances of a plug-in.
  static NPError Shutdown();

  // The table of functions provided by the host.
  static NPNetscapeFuncs *HostFunctions() { return host_functions_; }

 private:
  static NPNetscapeFuncs* host_functions_;
};

}  // namespace NPAPIClient

#endif  // CONTENT_TEST_PLUGIN_PLUGIN_CLIENT_H_
