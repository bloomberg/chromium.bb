// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/window_list_provider_impl.h"

#include <algorithm>

#include "athena/wm/public/window_list_provider_observer.h"
#include "ui/aura/window.h"

namespace athena {

WindowListProviderImpl::WindowListProviderImpl(aura::Window* container)
    : container_(container) {
  CHECK(container_);
  container_->AddObserver(this);
  RecreateWindowList();
  std::for_each(window_list_.begin(), window_list_.end(),
                std::bind2nd(std::mem_fun(&aura::Window::AddObserver),
                             this));
}

WindowListProviderImpl::~WindowListProviderImpl() {
  // Remove all remaining window observers.
  for (aura::Window::Windows::const_iterator iter = window_list_.begin();
       iter != window_list_.end();
       ++iter) {
    CHECK(IsValidWindow(*iter));
    (*iter)->RemoveObserver(this);
  }
  container_->RemoveObserver(this);
}

void WindowListProviderImpl::RecreateWindowList() {
  window_list_.clear();
  const aura::Window::Windows& container_children = container_->children();
  for (aura::Window::Windows::const_iterator iter = container_children.begin();
       iter != container_children.end();
       ++iter) {
    if (IsValidWindow(*iter))
      window_list_.push_back(*iter);
  }
}

void WindowListProviderImpl::AddObserver(WindowListProviderObserver* observer) {
  observers_.AddObserver(observer);
}

void WindowListProviderImpl::RemoveObserver(
    WindowListProviderObserver* observer) {
  observers_.RemoveObserver(observer);
}

const aura::Window::Windows& WindowListProviderImpl::GetWindowList() const {
  return window_list_;
}

bool WindowListProviderImpl::IsWindowInList(aura::Window* window) const {
  return window->parent() == container_ && IsValidWindow(window);
}

bool WindowListProviderImpl::IsValidWindow(aura::Window* window) const {
  // TODO(oshima): crbug.com/413912
  return window->type() == ui::wm::WINDOW_TYPE_NORMAL ||
      window->type() == ui::wm::WINDOW_TYPE_PANEL;
}

void WindowListProviderImpl::StackWindowFrontOf(
    aura::Window* window,
    aura::Window* reference_window) {
  DCHECK_NE(window, reference_window);
  DCHECK(IsWindowInList(window));
  DCHECK(IsWindowInList(reference_window));
  container_->StackChildAbove(window, reference_window);
}

void WindowListProviderImpl::StackWindowBehindTo(
    aura::Window* window,
    aura::Window* reference_window) {
  DCHECK_NE(window, reference_window);
  DCHECK(IsWindowInList(window));
  DCHECK(IsWindowInList(reference_window));
  container_->StackChildBelow(window, reference_window);
}

void WindowListProviderImpl::OnWindowAdded(aura::Window* window) {
  if (!IsValidWindow(window) || window->parent() != container_)
    return;
  RecreateWindowList();
  DCHECK(IsWindowInList(window));
  window->AddObserver(this);
}

void WindowListProviderImpl::OnWillRemoveWindow(aura::Window* window) {
  if (!IsValidWindow(window) || window->parent() != container_)
    return;
  DCHECK(IsWindowInList(window));
  aura::Window::Windows::iterator find = std::find(window_list_.begin(),
                                                   window_list_.end(),
                                                   window);
  CHECK(find != window_list_.end());
  int index = find - window_list_.begin();
  window_list_.erase(find);
  window->RemoveObserver(this);
  FOR_EACH_OBSERVER(
      WindowListProviderObserver, observers_, OnWindowRemoved(window, index));
}

void WindowListProviderImpl::OnWindowStackingChanged(aura::Window* window) {
  if (window == container_)
    return;
  DCHECK(IsWindowInList(window));
  RecreateWindowList();
  // Inform our listeners that the stacking has been changed.
  FOR_EACH_OBSERVER(WindowListProviderObserver,
                    observers_,
                    OnWindowStackingChanged());
}

}  // namespace athena
