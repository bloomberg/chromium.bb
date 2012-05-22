// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_window_list.h"

#include <algorithm>

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/ui/base_window.h"

///////////////////////////////////////////////////////////////////////////////
// ExtensionWindowList

// static
ExtensionWindowList* ExtensionWindowList::GetInstance() {
  return Singleton<ExtensionWindowList>::get();
}

ExtensionWindowList::ExtensionWindowList() {
}

ExtensionWindowList::~ExtensionWindowList() {
}

void ExtensionWindowList::AddExtensionWindow(
    ExtensionWindowController* window) {
  windows_.push_back(window);
}

void ExtensionWindowList::RemoveExtensionWindow(
    ExtensionWindowController* window) {
  WindowList::iterator iter = std::find(
      windows_.begin(), windows_.end(), window);
  if (iter != windows_.end())
    windows_.erase(iter);
}

ExtensionWindowController* ExtensionWindowList::FindWindowForFunctionById(
    const UIThreadExtensionFunction* function,
    int id) const {
  for (WindowList::const_iterator iter = windows().begin();
       iter != windows().end(); ++iter) {
    if (function->CanOperateOnWindow(*iter) && (*iter)->GetWindowId() == id)
      return *iter;
  }
  return NULL;
}

ExtensionWindowController* ExtensionWindowList::CurrentWindowForFunction(
    const UIThreadExtensionFunction* function) const {
  ExtensionWindowController* result = NULL;
  // Returns either the focused window (if any), or the last window in the list.
  for (WindowList::const_iterator iter = windows().begin();
       iter != windows().end(); ++iter) {
    if (function->CanOperateOnWindow(*iter)) {
      result = *iter;
      if (result->window()->IsActive())
        break;  // use focused window
    }
  }
  return result;
}
