// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/logging.h"
#include "gpu/demos/framework/plugin.h"
#include "gpu/pgl/pgl.h"
#include "webkit/glue/plugins/nphostapi.h"

namespace {
// AtExitManager is used by singleton classes to delete themselves when
// the program terminates. There should be only one instance of this class
// per thread;
base::AtExitManager* g_at_exit_manager_;
}  // namespace

namespace gpu {
namespace demos {
// NPP entry points.
NPError NPP_New(NPMIMEType pluginType,
                NPP instance,
                uint16 mode,
                int16 argc, char* argn[], char* argv[],
                NPSavedData* saved) {
  if (g_browser->version < NPVERS_HAS_PLUGIN_THREAD_ASYNC_CALL) {
    return NPERR_INCOMPATIBLE_VERSION_ERROR;
  }

  Plugin* plugin = static_cast<Plugin*>(
      g_browser->createobject(instance, Plugin::GetPluginClass()));
  instance->pdata = plugin;
  plugin->New(pluginType, argc, argn, argv);

  return NPERR_NO_ERROR;
}

NPError NPP_Destroy(NPP instance, NPSavedData** save) {
  Plugin* plugin = static_cast<Plugin*>(instance->pdata);
  if (plugin) g_browser->releaseobject(plugin);

  return NPERR_NO_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow* window) {
  Plugin* plugin = static_cast<Plugin*>(instance->pdata);
  if (plugin) plugin->SetWindow(*window);

  return NPERR_NO_ERROR;
}

NPError NPP_NewStream(NPP instance,
                      NPMIMEType type,
                      NPStream* stream,
                      NPBool seekable,
                      uint16* stype) {
  *stype = NP_ASFILEONLY;
  return NPERR_NO_ERROR;
}

NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason) {
  return NPERR_NO_ERROR;
}

void NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname) {
}

int32 NPP_Write(NPP instance,
                NPStream* stream,
                int32 offset,
                int32 len,
                void* buffer) {
  return 0;
}

int32 NPP_WriteReady(NPP instance, NPStream* stream) {
  return 0;
}

void NPP_Print(NPP instance, NPPrint* platformPrint) {
}

int16 NPP_HandleEvent(NPP instance, void* event) {
  return 0;
}

void NPP_URLNotify(NPP instance, const char* url, NPReason reason,
                   void* notify_data) {
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void* value) {
  NPError err = NPERR_NO_ERROR;

  switch (variable) {
    case NPPVpluginScriptableNPObject: {
      void** v = reinterpret_cast<void**>(value);
      Plugin* plugin = static_cast<Plugin*>(instance->pdata);
      // Return value is expected to be retained
      g_browser->retainobject(plugin);
      *v = plugin;
      break;
    }
    default:
      LOG(INFO) << "Unhandled variable to NPP_GetValue\n";
      err = NPERR_GENERIC_ERROR;
      break;
  }

  return err;
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void* value) {
  return NPERR_GENERIC_ERROR;
}
}  // namespace demos
}  // namespace gpu

// NP entry points
extern "C" {
NPError API_CALL NP_GetEntryPoints(NPPluginFuncs* plugin_funcs) {
  plugin_funcs->version = NPVERS_HAS_PLUGIN_THREAD_ASYNC_CALL;
  plugin_funcs->size = sizeof(plugin_funcs);
  plugin_funcs->newp = gpu::demos::NPP_New;
  plugin_funcs->destroy = gpu::demos::NPP_Destroy;
  plugin_funcs->setwindow = gpu::demos::NPP_SetWindow;
  plugin_funcs->newstream = gpu::demos::NPP_NewStream;
  plugin_funcs->destroystream = gpu::demos::NPP_DestroyStream;
  plugin_funcs->asfile = gpu::demos::NPP_StreamAsFile;
  plugin_funcs->writeready = gpu::demos::NPP_WriteReady;
  plugin_funcs->write = gpu::demos::NPP_Write;
  plugin_funcs->print = gpu::demos::NPP_Print;
  plugin_funcs->event = gpu::demos::NPP_HandleEvent;
  plugin_funcs->urlnotify = gpu::demos::NPP_URLNotify;
  plugin_funcs->getvalue = gpu::demos::NPP_GetValue;
  plugin_funcs->setvalue = gpu::demos::NPP_SetValue;

  return NPERR_NO_ERROR;
}

NPError API_CALL NP_Initialize(NPNetscapeFuncs* browser_funcs) {
  g_at_exit_manager_ = new base::AtExitManager();
  gpu::demos::g_browser = browser_funcs;
  pglInitialize();
  return NPERR_NO_ERROR;
}

void API_CALL NP_Shutdown() {
  pglTerminate();
  delete g_at_exit_manager_;
}
}  // extern "C"
