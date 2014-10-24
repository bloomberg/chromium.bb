// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_WM_WINDOW_LIST_PROVIDER_IMPL_H_
#define ATHENA_WM_WINDOW_LIST_PROVIDER_IMPL_H_

#include "athena/wm/public/window_list_provider.h"
#include "base/gtest_prod_util.h"
#include "ui/aura/window_observer.h"

namespace athena {

class WindowListProviderObserver;

// This implementation of the WindowListProviderImpl uses the same order as in
// the container window's stacking order.
class ATHENA_EXPORT WindowListProviderImpl : public WindowListProvider,
                                             public aura::WindowObserver {
 public:
  explicit WindowListProviderImpl(aura::Window* container);
  ~WindowListProviderImpl() override;

  // TODO(oshima): Remove this method. This should live outside.
  bool IsValidWindow(aura::Window* window) const;

  // WindowListProvider:
  void AddObserver(WindowListProviderObserver* observer) override;
  void RemoveObserver(WindowListProviderObserver* observer) override;
  const aura::Window::Windows& GetWindowList() const override;
  bool IsWindowInList(aura::Window* window) const override;
  void StackWindowFrontOf(aura::Window* window,
                          aura::Window* reference_window) override;
  void StackWindowBehindTo(aura::Window* window,
                           aura::Window* reference_window) override;

 private:
  void RecreateWindowList();

  // aura::WindowObserver:
  void OnWindowAdded(aura::Window* new_window) override;
  void OnWillRemoveWindow(aura::Window* old_window) override;
  void OnWindowStackingChanged(aura::Window* window) override;

  aura::Window* container_;
  aura::Window::Windows window_list_;
  ObserverList<WindowListProviderObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(WindowListProviderImpl);
};

}  // namespace athena

#endif  // ATHENA_WM_WINDOW_LIST_PROVIDER_IMPL_H_
