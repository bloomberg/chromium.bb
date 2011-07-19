// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#ifndef CHROME_FRAME_EVENT_HOOKER_H_
#define CHROME_FRAME_EVENT_HOOKER_H_

class EventHooker {
 public:
  EventHooker();
  ~EventHooker();

  // Call this to install event hooks that will trigger on window creation
  // and reparenting. Returns true if the hooks are successfully installed,
  // false otherwise.
  bool StartHook();

  // Call this to remove the event hooks that are installed by StartHook().
  void StopHook();

  // The callback invoked in response to an event registered for in StartHook().
  static VOID CALLBACK WindowCreationHookProc(HWINEVENTHOOK hook,
                                              DWORD event,
                                              HWND window,
                                              LONG object_id,
                                              LONG child_id,
                                              DWORD event_tid,
                                              DWORD event_time);

 private:
  HWINEVENTHOOK window_creation_hook_;
};

#endif  // CHROME_FRAME_EVENT_HOOKER_H_
