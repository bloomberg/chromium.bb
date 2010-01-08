// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/chrome_frame_npapi.h"

#define NPAPI WINAPI

// Plugin entry points.
extern "C" {
  NPError NPAPI NP_Initialize(NPNetscapeFuncs* browser_funcs);
  NPError NPAPI NP_GetEntryPoints(NPPluginFuncs* plugin_funcs);
  void NPAPI NP_Shutdown();
}

NPError NPAPI NP_Initialize(NPNetscapeFuncs* browser_funcs) {
  DLOG(INFO) << __FUNCTION__;
  _pAtlModule->Lock();
  npapi::InitializeBrowserFunctions(browser_funcs);
  return NPERR_NO_ERROR;
}

NPError NPAPI NP_GetEntryPoints(NPPluginFuncs* plugin_funcs) {
  plugin_funcs->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
  plugin_funcs->size = sizeof(plugin_funcs);
  plugin_funcs->newp = NPP_New;
  plugin_funcs->destroy = NPP_Destroy;
  plugin_funcs->setwindow = NPP_SetWindow;
  plugin_funcs->newstream = NPP_NewStream;
  plugin_funcs->destroystream = NPP_DestroyStream;
  plugin_funcs->asfile = NULL;
  plugin_funcs->writeready = NPP_WriteReady;
  plugin_funcs->write = NPP_Write;
  plugin_funcs->print = NPP_Print;
  plugin_funcs->event = NULL;
  plugin_funcs->urlnotify = NPP_URLNotify;
  plugin_funcs->getvalue = NPP_GetValue;
  plugin_funcs->setvalue = NPP_SetValue;
  return NPERR_NO_ERROR;
}

void NPAPI NP_Shutdown() {
  DLOG(INFO) << __FUNCTION__;

  npapi::UninitializeBrowserFunctions();

  _pAtlModule->Unlock();  // matches Lock() inside NP_Initialize

  DLOG_IF(ERROR, _pAtlModule->GetLockCount() != 0)
        << "Being shut down but still have " << _pAtlModule->GetLockCount()
        << " living objects";
}

