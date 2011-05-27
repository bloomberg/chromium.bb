// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A utility class that makes it easy to register for registry change
// notifications.
//

#include "chrome_frame/registry_watcher.h"

#include "chrome_frame/chrome_frame_helper_util.h"

namespace {
const wchar_t kRegistryWatcherEventName[] = L"chrome_registry_watcher_event";
}  // namespace

RegistryWatcher::RegistryWatcher(HKEY hive,
                                 const wchar_t* path,
                                 NotifyFunc callback)
    : callback_(callback),
      wait_event_(NULL),
      wait_handle_(NULL),
      stopping_(false) {
  // Enforce that we can open the given registry path with the KEY_NOTIFY
  // permission.
  LONG result = RegOpenKeyEx(hive, path, 0, KEY_NOTIFY, &registry_key_);
  if (result != ERROR_SUCCESS) {
    registry_key_ = NULL;
  }
}

RegistryWatcher::~RegistryWatcher() {
  StopWatching();
  if (registry_key_) {
    RegCloseKey(registry_key_);
    registry_key_ = NULL;
  }
}

bool RegistryWatcher::StartWatching() {
  if (!registry_key_ || wait_event_ || !callback_) {
    return false;
  }

  bool result = false;
  wait_event_ = CreateEvent(NULL,
                            FALSE,  // Auto-resets
                            FALSE,  // Initially non-signalled
                            kRegistryWatcherEventName);
  if (wait_event_ != NULL) {
    LONG notify_result = RegNotifyChangeKeyValue(
        registry_key_,
        TRUE,  // Watch subtree
        REG_NOTIFY_CHANGE_NAME,  // Notifies if a subkey is added.
        wait_event_,
        TRUE);  // Asynchronous, signal the event when a change occurs.

    if (notify_result == ERROR_SUCCESS) {
      if (RegisterWaitForSingleObject(&wait_handle_,
                                      wait_event_,
                                      &RegistryWatcher::WaitCallback,
                                      reinterpret_cast<void*>(this),
                                      INFINITE,
                                      WT_EXECUTEDEFAULT)) {
        stopping_ = false;
        result = true;
      }
    }
  }

  // If we're not good to go we don't need to hold onto the event.
  if (!result && wait_event_) {
    CloseHandle(wait_event_);
    wait_event_ = NULL;
  }

  return result;
}

void RegistryWatcher::StopWatching() {
  stopping_ = true;
  if (wait_handle_) {
    // Unregister the wait and block until any current handlers have returned.
    UnregisterWaitEx(wait_handle_, INVALID_HANDLE_VALUE);
    wait_handle_ = NULL;
  }
  if (wait_event_) {
    CloseHandle(wait_event_);
    wait_event_ = NULL;
  }
}

void CALLBACK RegistryWatcher::WaitCallback(void* param, BOOLEAN wait_fired) {
  RegistryWatcher* watcher = reinterpret_cast<RegistryWatcher*>(param);
  if (watcher->stopping_)
    return;

  if (watcher->callback_) {
    watcher->callback_();
  }
}
