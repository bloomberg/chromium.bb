// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chrome_frame_helper_dll.cc : Implementation of a helper DLL that initializes
// a listener for WinEvent hooks on load.
//
// Calling the StartUserModeBrowserInjection will set an in-context WinEvent
// hook that will cause this DLL to get loaded in any process that sends
// EVENT_OBJECT_CREATE accessibility events. This is a poor substitute to
// getting in via vetted means (i.e. real BHO registration).

#include "chrome_frame/bho_loader.h"
#include "chrome_frame/chrome_frame_helper_util.h"

STDAPI StartUserModeBrowserInjection() {
  return BHOLoader::GetInstance()->StartHook() ? S_OK : E_FAIL;
}

STDAPI StopUserModeBrowserInjection() {
  BHOLoader::GetInstance()->StopHook();
  return S_OK;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason_for_call, void* reserved) {
  return TRUE;
}

