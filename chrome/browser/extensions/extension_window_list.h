// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WINDOW_LIST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WINDOW_LIST_H_
#pragma once

#include <list>

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "chrome/browser/extensions/extension_window_controller.h"

class Profile;
class UIThreadExtensionFunction;

// Class to maintain a list of ExtensionWindowControllers.
class ExtensionWindowList {
 public:
  typedef std::list<ExtensionWindowController*> WindowList;

  ExtensionWindowList();
  ~ExtensionWindowList();

  void AddExtensionWindow(ExtensionWindowController* window);
  void RemoveExtensionWindow(ExtensionWindowController* window);

  // Returns a window matching the context the function was invoked in.
  ExtensionWindowController* FindWindowForFunctionById(
      const UIThreadExtensionFunction* function,
      int id) const;

  // Returns the focused or last added window matching the context the function
  // was invoked in.
  ExtensionWindowController* CurrentWindowForFunction(
      const UIThreadExtensionFunction* function) const;

  const WindowList& windows() const { return windows_; }

  static ExtensionWindowList* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ExtensionWindowList>;

  // Entries are not owned by this class and must be removed when destroyed.
  WindowList windows_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWindowList);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WINDOW_LIST_H_
