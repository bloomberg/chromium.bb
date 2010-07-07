// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/nphostapi.h"

namespace default_plugin {

extern NPNetscapeFuncs* g_browser;

// Standard NPAPI functions.
NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode,
                int16_t argc, char* argn[], char* argv[], NPSavedData* saved);
NPError NPP_Destroy(NPP instance, NPSavedData** save);
NPError NPP_SetWindow(NPP instance, NPWindow* window);
NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream,
                      NPBool seekable, uint16_t* stype);
NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason);
int32_t NPP_WriteReady(NPP instance, NPStream* stream);
int32_t NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len,
                  void* buffer);
#if defined(OS_POSIX) && !defined(OS_MACOSX)
NPError NPP_GetValue(NPP instance, NPPVariable variable, void* value);
#endif
void NPP_URLNotify(NPP instance, const char* url, NPReason reason,
                   void* notifyData);
int16_t NPP_HandleEvent(NPP instance, void* event);

#if defined(OS_POSIX) && !defined(OS_MACOSX)
NPError API_CALL NP_Initialize(NPNetscapeFuncs* funcs, NPPluginFuncs* p_funcs);
#else
NPError API_CALL NP_GetEntryPoints(NPPluginFuncs* funcs);
NPError API_CALL NP_Initialize(NPNetscapeFuncs* funcs);
#endif
NPError API_CALL NP_Shutdown(void);

}  // default_plugin
