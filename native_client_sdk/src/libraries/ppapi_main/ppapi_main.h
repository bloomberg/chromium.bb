// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_MAIN_PPAPI_MAIN_H_
#define PPAPI_MAIN_PPAPI_MAIN_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "utils/macros.h"

EXTERN_C_BEGIN

// Prototype for 'main' which will get called on startup
int ppapi_main(int argc, const char *argv[]);

// Provided by "main" module, use one of the macros below
void* UserCreateInstance(PP_Instance inst);

// Helpers to access PPAPI interfaces
PP_Instance PPAPI_GetInstanceId();
const void* PPAPI_GetInterface(const char *name);

// Provided by library for basic instance types
void* PPAPI_CreateInstance(PP_Instance inst, const char *args[]);
void* PPAPI_CreateInstance2D(PP_Instance inst, const char *args[]);
void* PPAPI_CreateInstance3D(PP_Instance inst, const char *args[]);

EXTERN_C_END

#define PPAPI_MAIN_DEFAULT_ARGS       \
  {                                   \
    "PM_STDIN", "/dev/null",          \
    "PM_STDIO", "/dev/console0",      \
    "PM_STDERR", "/dev/console3",     \
    NULL, NULL                        \
  }


#define PPAPI_MAIN_USE(factory, args)                         \
void* UserCreateInstance(PP_Instance inst) {                  \
  static const char *params[] = args;                         \
  return factory(inst, params);                               \
}

#define PPAPI_MAIN_WITH_DEFAULT_ARGS \
  PPAPI_MAIN_USE(PPAPI_CreateInstance, PPAPI_MAIN_DEFAULT_ARGS)

#define PPAPI_MAIN_WITH_ARGS(args) \
  PPAPI_MAIN_USE(PPAPI_CreateInstance, args)


#endif  // PPAPI_MAIN_PPAPI_MAIN_H_
