// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CHROME_FRAME_NPAPI_ENTRYPOINTS_H_
#define CHROME_FRAME_CHROME_FRAME_NPAPI_ENTRYPOINTS_H_

#include "chrome_frame/np_browser_functions.h"

namespace chrome_frame {

NPError NPP_New(NPMIMEType plugin_type, NPP instance, uint16 mode, int16 argc,
                char* argn[], char* argv[], NPSavedData* saved);

NPError NPP_Destroy(NPP instance, NPSavedData** save);

NPError NPP_SetWindow(NPP instance, NPWindow* window_info);

NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream,
                      NPBool seekable, uint16* stream_type);

NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason);

NPError NPP_GetValue(NPP instance, NPPVariable variable, void* value);

NPError NPP_SetValue(NPP instance, NPNVariable variable, void* value);

int32 NPP_WriteReady(NPP instance, NPStream* stream);

int32 NPP_Write(NPP instance, NPStream* stream, int32 offset, int32 len,
                void* buffer);
void NPP_URLNotify(NPP instance, const char* url, NPReason reason,
                   void* notifyData);

void NPP_Print(NPP instance, NPPrint* print_info);

}  // namespace chrome_frame

#endif  // CHROME_FRAME_CHROME_FRAME_NPAPI_ENTRYPOINTS_H_
