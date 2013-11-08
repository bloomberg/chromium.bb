// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/apps/js/v8_environment.h"
#include "mojo/public/system/core.h"
#include "mojo/public/system/macros.h"

#if defined(WIN32)
#if !defined(CDECL)
#define CDECL __cdecl
#endif
#define MOJO_APPS_JS_EXPORT __declspec(dllexport)
#else
#define CDECL
#define MOJO_APPS_JS_EXPORT __attribute__((visibility("default")))
#endif

extern "C" MOJO_APPS_JS_EXPORT MojoResult CDECL MojoMain(
    mojo::Handle pipe) {
  mojo::apps::InitializeV8();
  // TODO(abarth): Load JS off the network and execute it.
  return MOJO_RESULT_OK;
}
