// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/event_hooker.h"

#include <crtdbg.h>
#include "chrome_frame/bho_loader.h"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

EventHooker::EventHooker()
: window_creation_hook_(NULL) {}

EventHooker::~EventHooker() {
  StopHook();
}

bool EventHooker::StartHook() {
  if ((NULL != window_creation_hook_)) {
    return false;
  }

  window_creation_hook_ = SetWinEventHook(EVENT_OBJECT_CREATE,
                                          EVENT_OBJECT_CREATE,
                                          reinterpret_cast<HMODULE>(
                                              &__ImageBase),
                                          WindowCreationHookProc,
                                          0,
                                          0,
                                          WINEVENT_INCONTEXT);
  if (NULL == window_creation_hook_) {
    return false;
  }
  return true;
}

void EventHooker::StopHook() {
  if (NULL != window_creation_hook_) {
    UnhookWinEvent(window_creation_hook_);
    window_creation_hook_ = NULL;
  }
}

VOID CALLBACK EventHooker::WindowCreationHookProc(HWINEVENTHOOK hook,
                                                  DWORD event,
                                                  HWND window,
                                                  LONG object_id,
                                                  LONG child_id,
                                                  DWORD event_tid,
                                                  DWORD event_time) {
  _ASSERTE((EVENT_OBJECT_CREATE == event) ||
           (EVENT_OBJECT_PARENTCHANGE == event));
  if (OBJID_WINDOW == object_id) {
    BHOLoader::GetInstance()->OnHookEvent(event, window);
  }
}

