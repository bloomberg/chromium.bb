// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_GPU_PLUGIN_GPU_PLUGIN_H_
#define GPU_GPU_PLUGIN_GPU_PLUGIN_H_

#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"

typedef struct _NPPluginFuncs NPPluginFuncs;
typedef struct _NPNetscapeFuncs NPNetscapeFuncs;

namespace gpu_plugin {

// Declarations of NPAPI plugin entry points.

NPError API_CALL NP_GetEntryPoints(NPPluginFuncs* funcs);

#if defined(OS_POSIX) && !defined(OS_MACOSX)
NPError API_CALL NP_Initialize(NPNetscapeFuncs *browser_funcs,
                               NPPluginFuncs* plugin_funcs);
#else
NPError API_CALL NP_Initialize(NPNetscapeFuncs* browser_funcs);
#endif

NPError API_CALL NP_Shutdown();

}  // namespace gpu_plugin

#endif  // GPU_GPU_PLUGIN_GPU_PLUGIN_H_
