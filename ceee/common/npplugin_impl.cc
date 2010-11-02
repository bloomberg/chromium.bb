// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of NPAPI plugin base class.
#include "base/logging.h"
#include "ceee/common/npplugin_impl.h"

NpPluginBase::~NpPluginBase() {
}

NPError NpPluginBase::SetWindow(NPWindow* window) {
  return NPERR_NO_ERROR;
}

NPError NpPluginBase::NewStream(NPMIMEType type, NPStream* stream,
                                NPBool seekable, uint16* stype) {
  return NPERR_GENERIC_ERROR;
}

NPError NpPluginBase::DestroyStream(NPStream* stream,
                                    NPReason reason) {
  DCHECK(false);  // You need to override this if you override NewStream
  return NPERR_GENERIC_ERROR;
}

int32 NpPluginBase::WriteReady(NPStream* stream) {
  return 0;
}

int32 NpPluginBase::Write(NPStream* stream, int32 offset,
                          int32 len, void* buffer) {
  return 0;
}

void NpPluginBase::StreamAsFile(NPStream* stream,
                                const char* fname) {
}

void NpPluginBase::Print(NPPrint* platform_print) {
}

int16 NpPluginBase::HandleEvent(void* event) {
  return 0;
}

void NpPluginBase::URLNotify(const char* url,
                             NPReason reason, void* notify_data) {
}

NPError NpPluginBase::GetValue(NPPVariable variable, void* value) {
  return NPERR_GENERIC_ERROR;
}

NPError NpPluginBase::SetValue(NPNVariable variable,
                               void* value) {
  return NPERR_GENERIC_ERROR;
}

NPError NpPluginBase::NPP_Destroy(NPP instance, NPSavedData** save) {
  NpPluginBase* plugin = reinterpret_cast<NpPluginBase*>(instance->pdata);
  delete plugin;
  instance->pdata = NULL;
  return NPERR_NO_ERROR;
}

NPError NpPluginBase::NPP_SetWindow(NPP instance, NPWindow* window) {
  NpPluginBase* plugin = reinterpret_cast<NpPluginBase*>(instance->pdata);

  return plugin->SetWindow(window);
}

NPError NpPluginBase::NPP_NewStream(NPP instance, NPMIMEType type,
                                NPStream* stream, NPBool seekable,
                                uint16* stype) {
  NpPluginBase* plugin = reinterpret_cast<NpPluginBase*>(instance->pdata);
  return plugin->NewStream(type, stream, seekable, stype);
}

NPError NpPluginBase::NPP_DestroyStream(NPP instance, NPStream* stream,
                                    NPReason reason) {
  NpPluginBase* plugin = reinterpret_cast<NpPluginBase*>(instance->pdata);

  return plugin->DestroyStream(stream, reason);
}

int32 NpPluginBase::NPP_WriteReady(NPP instance, NPStream* stream) {
  NpPluginBase* plugin = reinterpret_cast<NpPluginBase*>(instance->pdata);

  return plugin->WriteReady(stream);
}

int32 NpPluginBase::NPP_Write(NPP instance, NPStream* stream, int32 offset,
                            int32 len, void* buffer) {
  NpPluginBase* plugin = reinterpret_cast<NpPluginBase*>(instance->pdata);

  return plugin->Write(stream, offset, len, buffer);
}

void NpPluginBase::NPP_StreamAsFile(NPP instance, NPStream* stream,
                                   const char* fname) {
  NpPluginBase* plugin = reinterpret_cast<NpPluginBase*>(instance->pdata);

  return plugin->StreamAsFile(stream, fname);
}

void NpPluginBase::NPP_Print(NPP instance, NPPrint* platform_print) {
  NpPluginBase* plugin = reinterpret_cast<NpPluginBase*>(instance->pdata);

  return plugin->Print(platform_print);
}

int16 NpPluginBase::NPP_HandleEvent(NPP instance, void* event) {
  NpPluginBase* plugin = reinterpret_cast<NpPluginBase*>(instance->pdata);

  return plugin->HandleEvent(event);
}

void NpPluginBase::NPP_URLNotify(NPP instance, const char* url,
                   NPReason reason, void* notify_data) {
  NpPluginBase* plugin = reinterpret_cast<NpPluginBase*>(instance->pdata);

  return plugin->URLNotify(url, reason, notify_data);
}

NPError NpPluginBase::NPP_GetValue(NPP instance, NPPVariable variable,
                                   void* value) {
  NpPluginBase* plugin = reinterpret_cast<NpPluginBase*>(instance->pdata);

  return plugin->GetValue(variable, value);
}

NPError NpPluginBase::NPP_SetValue(NPP instance, NPNVariable variable,
                                   void* value) {
  NpPluginBase* plugin = reinterpret_cast<NpPluginBase*>(instance->pdata);

  return plugin->SetValue(variable, value);
}
