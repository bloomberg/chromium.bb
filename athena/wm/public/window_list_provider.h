// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_WM_PUBLIC_WINDOW_LIST_PROVIDER_H_
#define ATHENA_WM_PUBLIC_WINDOW_LIST_PROVIDER_H_

#include "athena/athena_export.h"
#include "ui/aura/window.h"

namespace athena {

class WindowListProviderObserver;

// Interface for an ordered list of aura::Window objects.
// Note that lists returned by GetCurrentWindowList() will not change if any of
// the other member functions will be called later.
class ATHENA_EXPORT WindowListProvider {
 public:
  virtual ~WindowListProvider() {}

  // Adding/removing an observer to status changes.
  virtual void AddObserver(WindowListProviderObserver* observer) = 0;
  virtual void RemoveObserver(WindowListProviderObserver* observer) = 0;

  // Returns an ordered list of the current window configuration.
  virtual const aura::Window::Windows& GetWindowList() const = 0;

  // Returns true if the |window| is part of the list.
  virtual bool IsWindowInList(aura::Window* window) const = 0;

  // Returns true if the given window is a window which can be handled by the
  // WindowListProvider.
  virtual bool IsValidWindow(aura::Window* window) const = 0;

  // Stacks a given |window| in direct front of a |reference_window|.
  // Note: The |window| and |reference_window| has to be in the list already.
  virtual void StackWindowFrontOf(aura::Window* window,
                                  aura::Window* reference_window) = 0;

  // Stacks a given |window| directly behind a |reference_window|.
  // Note: The |window| and |reference_window| has to be in the list already.
  virtual void StackWindowBehindTo(aura::Window* window,
                                   aura::Window* reference_window) = 0;
};

}  // namespace athena

#endif  // ATHENA_WM_PUBLIC_WINDOW_LIST_PROVIDER_H_
