// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Base classes to unclutter the writing of NPAPI plugins.
#ifndef CEEE_COMMON_NPPLUGIN_IMPL_H_
#define CEEE_COMMON_NPPLUGIN_IMPL_H_

#include "base/basictypes.h"
#include "third_party/npapi/bindings/nphostapi.h"

// Base class for NPAPI plugins.
class NpPluginBase {
 public:
  explicit NpPluginBase(NPP npp) : npp_(npp) {
  }
  virtual ~NpPluginBase();

 protected:
  NPError Initialize();

  // @name NP Plugin interface.
  // Override these to implement your plugin's functionality
  // For documentation on these methods, see the corresponding callback
  // function description in https://developer.mozilla.org/en
  // /Gecko_Plugin_API_Reference/Plug-in_Side_Plug-in_API
  // @{
  virtual NPError SetWindow(NPWindow* window);
  virtual NPError NewStream(NPMIMEType type, NPStream* stream,
                            NPBool seekable, uint16* stype);
  virtual NPError DestroyStream(NPStream* stream,
                                NPReason reason);
  virtual int32 WriteReady(NPStream* stream);
  virtual int32 Write(NPStream* stream, int32 offset,
                          int32 len, void* buffer);
  virtual void StreamAsFile(NPStream* stream,
                                const char* fname);
  virtual void Print(NPPrint* platform_print);
  virtual int16 HandleEvent(void* event);
  virtual void URLNotify(const char* url, NPReason reason, void* notify_data);
  virtual NPError GetValue(NPPVariable variable, void* value);
  virtual NPError SetValue(NPNVariable variable, void* value);
  // @}

  // @name NP callback functions.
  // @{
  static NPError NPP_Destroy(NPP instance, NPSavedData** save);
  static NPError NPP_SetWindow(NPP instance, NPWindow* window);
  static NPError NPP_NewStream(NPP instance, NPMIMEType type,
                               NPStream* stream, NPBool seekable,
                               uint16* stype);
  static NPError NPP_DestroyStream(NPP instance, NPStream* stream,
                                   NPReason reason);
  static int32 NPP_WriteReady(NPP instance, NPStream* stream);
  static int32 NPP_Write(NPP instance, NPStream* stream, int32 offset,
                         int32 len, void* buffer);
  static void NPP_StreamAsFile(NPP instance, NPStream* stream,
                               const char* fname);
  static void NPP_Print(NPP instance, NPPrint* platform_print);
  static int16 NPP_HandleEvent(NPP instance, void* event);
  static void NPP_URLNotify(NPP instance, const char* url,
                            NPReason reason, void* notifyData);
  static NPError NPP_GetValue(NPP instance, NPPVariable variable, void* value);
  static NPError NPP_SetValue(NPP instance, NPNVariable variable, void* value);
  // @}

  // Our instance.
  NPP npp_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NpPluginBase);
};

// Derive your NPAPI implementation class from NpPluginImpl<MyClass>.
template <class ImplClass> class NpPluginImpl: public NpPluginBase {
 public:
  explicit NpPluginImpl(NPP npp) : NpPluginBase(npp) {
  }

  // Retrieve a set of plugin entrypoints suitable for ImplClass.
  static NPError GetEntryPoints(NPPluginFuncs* plugin_funcs);

 protected:
  static NPError NPP_New(NPMIMEType plugin_type, NPP instance,
                         uint16 mode, int16 argc, char* argn[],
                         char* argv[], NPSavedData* saved);
  // Override this in ImplClass if you need custom creation logic.
  static NPError Create(NPMIMEType plugin_type, NPP instance,
                        uint16 mode, int16 argc, char* argn[],
                        char* argv[], NPSavedData* saved);

 private:
  DISALLOW_COPY_AND_ASSIGN(NpPluginImpl);
};

template <class ImplClass>
NPError NpPluginImpl<ImplClass>::GetEntryPoints(NPPluginFuncs* plugin_funcs) {
  plugin_funcs->version = 11;
  plugin_funcs->size = sizeof(plugin_funcs);
  plugin_funcs->newp = ImplClass::NPP_New;
  plugin_funcs->destroy = ImplClass::NPP_Destroy;
  plugin_funcs->setwindow = ImplClass::NPP_SetWindow;
  plugin_funcs->newstream = ImplClass::NPP_NewStream;
  plugin_funcs->destroystream = ImplClass::NPP_DestroyStream;
  plugin_funcs->asfile = ImplClass::NPP_StreamAsFile;
  plugin_funcs->writeready = ImplClass::NPP_WriteReady;
  plugin_funcs->write = ImplClass::NPP_Write;
  plugin_funcs->print = ImplClass::NPP_Print;
  plugin_funcs->event = ImplClass::NPP_HandleEvent;
  plugin_funcs->urlnotify = ImplClass::NPP_URLNotify;
  plugin_funcs->getvalue = ImplClass::NPP_GetValue;
  plugin_funcs->setvalue = ImplClass::NPP_SetValue;

  return NPERR_NO_ERROR;
}

template <class ImplClass>
NPError NpPluginImpl<ImplClass>::NPP_New(NPMIMEType plugin_type, NPP instance,
                                         uint16 mode, int16 argc, char* argn[],
                                         char* argv[], NPSavedData* saved) {
  // This gives the implclass an opportunity to
  // override default creation behavior.
  return ImplClass::Create(plugin_type,
                           instance,
                           mode,
                           argc,
                           argn,
                           argv,
                           saved);
}

template <class ImplClass>
NPError NpPluginImpl<ImplClass>::Create(NPMIMEType plugin_type, NPP instance,
                                        uint16 mode, int16 argc, char* argn[],
                                        char* argv[], NPSavedData* saved) {
  ImplClass* impl = new ImplClass(instance);
  NPError error = impl->Initialize();
  if (NPERR_NO_ERROR == error) {
    instance->pdata = impl;
  } else {
    delete impl;
  }

  return error;
}

#endif  // CEEE_COMMON_NPPLUGIN_IMPL_H_
