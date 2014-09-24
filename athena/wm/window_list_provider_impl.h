// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_WM_WINDOW_LIST_PROVIDER_IMPL_H_
#define ATHENA_WM_WINDOW_LIST_PROVIDER_IMPL_H_

#include "athena/wm/public/window_list_provider.h"
#include "ui/aura/window_observer.h"

namespace athena {

class WindowListProviderObserver;

// This implementation of the WindowListProviderImpl uses the same order as in
// the container window's stacking order.
class ATHENA_EXPORT WindowListProviderImpl : public WindowListProvider,
                                             public aura::WindowObserver {
 public:
  explicit WindowListProviderImpl(aura::Window* container);
  virtual ~WindowListProviderImpl();

 private:
  void RecreateWindowList();

  // WindowListProvider:
  virtual void AddObserver(WindowListProviderObserver* observer) OVERRIDE;
  virtual void RemoveObserver(WindowListProviderObserver* observer) OVERRIDE;
  virtual const aura::Window::Windows& GetWindowList() const OVERRIDE;
  virtual bool IsWindowInList(aura::Window* window) const OVERRIDE;
  virtual bool IsValidWindow(aura::Window* window) const OVERRIDE;
  virtual void StackWindowFrontOf(aura::Window* window,
                                  aura::Window*reference_window) OVERRIDE;
  virtual void StackWindowBehindTo(aura::Window* window,
                                   aura::Window*reference_window) OVERRIDE;

  // aura::WindowObserver:
  virtual void OnWindowAdded(aura::Window* new_window) OVERRIDE;
  virtual void OnWillRemoveWindow(aura::Window* old_window) OVERRIDE;
  virtual void OnWindowStackingChanged(aura::Window* window) OVERRIDE;

  aura::Window* container_;
  aura::Window::Windows window_list_;
  ObserverList<WindowListProviderObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(WindowListProviderImpl);
};

}  // namespace athena

#endif  // ATHENA_WM_WINDOW_LIST_PROVIDER_IMPL_H_
