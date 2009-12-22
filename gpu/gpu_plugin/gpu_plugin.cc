// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/gpu_plugin/gpu_plugin.h"
#include "webkit/glue/plugins/nphostapi.h"

namespace gpu_plugin {

// Definitions of NPAPI plugin entry points.

namespace {

NPError NPP_New(NPMIMEType plugin_type, NPP instance,
                uint16 mode, int16 argc, char* argn[],
                char* argv[], NPSavedData* saved) {
  if (!instance)
    return NPERR_INVALID_INSTANCE_ERROR;

  return NPERR_NO_ERROR;
}

NPError NPP_Destroy(NPP instance, NPSavedData** saved) {
  if (!instance)
    return NPERR_INVALID_INSTANCE_ERROR;

  return NPERR_NO_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow* window) {
  return NPERR_NO_ERROR;
}

int16 NPP_HandleEvent(NPP instance, void* event) {
  return 0;
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value) {
  if (!instance)
    return NPERR_INVALID_INSTANCE_ERROR;

  return NPERR_GENERIC_ERROR;
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value) {
  return NPERR_NO_ERROR;
}
}

NPError API_CALL NP_GetEntryPoints(NPPluginFuncs* funcs) {
  funcs->newp = NPP_New;
  funcs->destroy = NPP_Destroy;
  funcs->setwindow = NPP_SetWindow;
  funcs->event = NPP_HandleEvent;
  funcs->getvalue = NPP_GetValue;
  funcs->setvalue = NPP_SetValue;
  return NPERR_NO_ERROR;
}

#if defined(OS_LINUX)
NPError API_CALL NP_Initialize(NPNetscapeFuncs *browser_funcs,
                               NPPluginFuncs* plugin_funcs) {
#else
NPError API_CALL NP_Initialize(NPNetscapeFuncs *browser_funcs) {
#endif
  if (!browser_funcs)
    return NPERR_INVALID_FUNCTABLE_ERROR;

#if defined(OS_LINUX)
  NP_GetEntryPoints(plugin_funcs);
#endif

  return NPERR_NO_ERROR;
}

NPError API_CALL NP_Shutdown() {
  return NPERR_NO_ERROR;
}
}  // namespace gpu_plugin
