// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/window_controller_list.h"

#include <algorithm>

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/window_controller_list_observer.h"
#include "chrome/browser/sessions/session_id.h"
#include "ui/base/base_window.h"

namespace extensions {

///////////////////////////////////////////////////////////////////////////////
// WindowControllerList

// static
WindowControllerList* WindowControllerList::GetInstance() {
  return Singleton<WindowControllerList>::get();
}

WindowControllerList::WindowControllerList() {
}

WindowControllerList::~WindowControllerList() {
}

void WindowControllerList::AddExtensionWindow(WindowController* window) {
  windows_.push_back(window);
  FOR_EACH_OBSERVER(WindowControllerListObserver, observers_,
                    OnWindowControllerAdded(window));
}

void WindowControllerList::RemoveExtensionWindow(WindowController* window) {
  ControllerList::iterator iter = std::find(
      windows_.begin(), windows_.end(), window);
  if (iter != windows_.end()) {
    windows_.erase(iter);
    FOR_EACH_OBSERVER(WindowControllerListObserver, observers_,
                      OnWindowControllerRemoved(window));
  }
}

void WindowControllerList::AddObserver(WindowControllerListObserver* observer) {
  observers_.AddObserver(observer);
}

void WindowControllerList::RemoveObserver(
    WindowControllerListObserver* observer) {
  observers_.RemoveObserver(observer);
}

WindowController* WindowControllerList::FindWindowById(int id) const {
  for (ControllerList::const_iterator iter = windows().begin();
       iter != windows().end(); ++iter) {
    if ((*iter)->GetWindowId() == id)
      return *iter;
  }
  return NULL;
}

WindowController* WindowControllerList::FindWindowForFunctionById(
    const ChromeUIThreadExtensionFunction* function,
    int id) const {
  WindowController* controller = FindWindowById(id);
  if (controller && function->CanOperateOnWindow(controller))
    return controller;
  return NULL;
}

WindowController* WindowControllerList::CurrentWindowForFunction(
    const ChromeUIThreadExtensionFunction* function) const {
  WindowController* result = NULL;
  // Returns either the focused window (if any), or the last window in the list.
  for (ControllerList::const_iterator iter = windows().begin();
       iter != windows().end(); ++iter) {
    if (function->CanOperateOnWindow(*iter)) {
      result = *iter;
      if (result->window()->IsActive())
        break;  // use focused window
    }
  }
  return result;
}

}  // namespace extensions
