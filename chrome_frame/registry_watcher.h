// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A utility class that makes it easy to register for registry change
// notifications.
//

#ifndef CHROME_FRAME_REGISTRY_WATCHER_H_
#define CHROME_FRAME_REGISTRY_WATCHER_H_

#include <windows.h>

class RegistryWatcher {
 public:
  typedef void (*NotifyFunc)();
  RegistryWatcher(HKEY hive, const wchar_t* path, NotifyFunc callback);
  ~RegistryWatcher();

  bool StartWatching();
  void StopWatching();

 private:
  static void CALLBACK WaitCallback(void* param, BOOLEAN wait_fired);

  HKEY registry_key_;

  HANDLE wait_event_;
  HANDLE wait_handle_;
  bool stopping_;

  NotifyFunc callback_;
};


#endif  // CHROME_FRAME_REGISTRY_WATCHER_H_
