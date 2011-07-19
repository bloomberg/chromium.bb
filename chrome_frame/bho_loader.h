// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_BHO_LOADER_H_
#define CHROME_FRAME_BHO_LOADER_H_

#include <windows.h>

// Forward.
class EventHooker;

// This class simulates BHO loading in an IE process. It watches for the
// creation of a specific window and then retrieves the web browser object
// for that window and simulates loading of the BHO.
class BHOLoader {
 public:
  BHOLoader();
  ~BHOLoader();

  // Callback invoked on receipt of an accessibility event.
  void OnHookEvent(DWORD event, HWND window);

  // Call this to install event hooks that will trigger on window creation
  // and reparenting. Returns true if the hooks are successfully installed,
  // false otherwise.
  bool StartHook();

  // Call this to remove the event hooks that are installed by StartHook().
  void StopHook();

  // Retrieve the BHOLoader instance. Note that this is NOT thread-safe.
  static BHOLoader* GetInstance();

 private:
  EventHooker* hooker_;
};

#endif  // CHROME_FRAME_BHO_LOADER_H_
