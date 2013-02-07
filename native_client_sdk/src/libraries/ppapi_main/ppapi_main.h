// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_MAIN_PPAPI_MAIN_H_
#define PPAPI_MAIN_PPAPI_MAIN_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi_main/ppapi_event.h"

#include "utils/macros.h"

EXTERN_C_BEGIN

// Prototype for 'main' which will get called on startup
int ppapi_main(int argc, const char *argv[]);

// Provided by "main" module, use one of the macros below
void* UserCreateInstance(PP_Instance inst);

// Helpers to access PPAPI interfaces
void* PPAPI_GetInstanceObject();
PP_Instance PPAPI_GetInstanceId();
const void* PPAPI_GetInterface(const char *name);

// Provided by library for basic instance types
void* PPAPI_CreateInstance(PP_Instance inst, const char *args[]);
void* PPAPI_CreateInstance2D(PP_Instance inst, const char *args[]);
void* PPAPI_CreateInstance3D(PP_Instance inst, const char *args[]);

// Event APIs
PPAPIEvent* PPAPI_AcquireEvent();
void PPAPI_ReleaseEvent(PPAPIEvent* event);


// Functions for 3D instances
int32_t *PPAPIGet3DAttribs(uint32_t width, uint32_t height);
void PPAPIBuildContext(uint32_t width, uint32_t height);
void PPAPIRender(uint32_t width, uint32_t height);

EXTERN_C_END

#define PPAPI_MAIN_DEFAULT_ARGS       \
  {                                   \
    NULL, NULL                        \
  }


#define PPAPI_MAIN_USE(factory, args)                         \
void* UserCreateInstance(PP_Instance inst) {                  \
  static const char *params[] = args;                         \
  return factory(inst, params);                               \
}

#define PPAPI_MAIN_WITH_DEFAULT_ARGS \
  PPAPI_MAIN_USE(PPAPI_CreateInstance, PPAPI_MAIN_DEFAULT_ARGS)

#define PPAPI_MAIN_3D_WITH_DEFAULT_ARGS \
  PPAPI_MAIN_USE(PPAPI_CreateInstance3D, PPAPI_MAIN_DEFAULT_ARGS)

#define PPAPI_MAIN_WITH_ARGS(args) \
  PPAPI_MAIN_USE(PPAPI_CreateInstance, args)


#endif  // PPAPI_MAIN_PPAPI_MAIN_H_
