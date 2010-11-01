// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "build/build_config.h"
#include "gpu/gpu_plugin/gpu_plugin.h"
#include "third_party/npapi/bindings/nphostapi.h"

namespace gpu_plugin {

// Definitions of NPAPI plugin entry points.

namespace {

// TODO(apatrick): move this to platform specific source files.
#if defined(OS_WIN)
class PluginObject {
 public:
  PluginObject();
  ~PluginObject();

  void SetWindow(HWND hwnd);

 private:
  HWND hwnd_;

  DISALLOW_COPY_AND_ASSIGN(PluginObject);
};

const wchar_t* const kPluginObject = L"GPUPluginObject";

PluginObject::PluginObject() : hwnd_(NULL) {
}

PluginObject::~PluginObject() {
}

void PluginObject::SetWindow(HWND hwnd) {
  hwnd_ = hwnd;
  if (hwnd_) {
    // TODO: convert this to using app::win::ScopedProp.
    // Store plugin object in window property.
    SetProp(hwnd_, kPluginObject, reinterpret_cast<HANDLE>(this));

    // Disable plugin window so mouse messages are passed to the parent window.
    EnableWindow(hwnd_, false);
  } else {
    // Clean up properties.
    RemoveProp(hwnd_, kPluginObject);
  }
}

#endif  // defined(OS_WIN)

NPError NPP_New(NPMIMEType plugin_type, NPP instance,
                uint16 mode, int16 argc, char* argn[],
                char* argv[], NPSavedData* saved) {
  if (!instance)
    return NPERR_INVALID_INSTANCE_ERROR;

#if defined(OS_WIN)
  instance->pdata = new PluginObject;
#endif

  return NPERR_NO_ERROR;
}

NPError NPP_Destroy(NPP instance, NPSavedData** saved) {
  if (!instance)
    return NPERR_INVALID_INSTANCE_ERROR;

#if defined(OS_WIN)
  delete static_cast<PluginObject*>(instance->pdata);
#endif

  return NPERR_NO_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow* window) {
#if defined(OS_WIN)
  PluginObject* plugin_object = static_cast<PluginObject*>(instance->pdata);
  plugin_object->SetWindow(reinterpret_cast<HWND>(window->window));
#endif

  return NPERR_NO_ERROR;
}

int16 NPP_HandleEvent(NPP instance, void* event) {
  return 0;
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value) {
  if (!instance)
    return NPERR_INVALID_INSTANCE_ERROR;
  switch (variable) {
    case NPPVpluginNeedsXEmbed:
      *static_cast<NPBool *>(value) = 1;
      return NPERR_NO_ERROR;
    default:
      return NPERR_INVALID_PARAM;
  }
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value) {
  return NPERR_NO_ERROR;
}

}  // namespace

NPError API_CALL NP_GetEntryPoints(NPPluginFuncs* funcs) {
  funcs->newp = NPP_New;
  funcs->destroy = NPP_Destroy;
  funcs->setwindow = NPP_SetWindow;
  funcs->event = NPP_HandleEvent;
  funcs->getvalue = NPP_GetValue;
  funcs->setvalue = NPP_SetValue;
  return NPERR_NO_ERROR;
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
NPError API_CALL NP_Initialize(NPNetscapeFuncs *browser_funcs,
                               NPPluginFuncs* plugin_funcs) {
#else
NPError API_CALL NP_Initialize(NPNetscapeFuncs *browser_funcs) {
#endif
  if (!browser_funcs)
    return NPERR_INVALID_FUNCTABLE_ERROR;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  NP_GetEntryPoints(plugin_funcs);
#endif

  return NPERR_NO_ERROR;
}

NPError API_CALL NP_Shutdown() {
  return NPERR_NO_ERROR;
}

}  // namespace gpu_plugin
