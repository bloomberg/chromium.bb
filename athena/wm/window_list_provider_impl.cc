// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/window_list_provider_impl.h"

#include <algorithm>

#include "athena/athena_export.h"
#include "athena/wm/public/window_list_provider_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"

namespace athena {
namespace {

// Used to keep track of which window should be managed. This is necessary
// as the necessary informatino used in IsValidWindow (transient parent
// for example) may not available during destruction.
DEFINE_WINDOW_PROPERTY_KEY(bool, kManagedKey, false);

}  // namespace

WindowListProviderImpl::WindowListProviderImpl(aura::Window* container)
    : container_(container) {
  CHECK(container_);
  container_->AddObserver(this);
  RecreateWindowList();
  for (auto* window : window_list_)
    window->AddObserver(this);
}

WindowListProviderImpl::~WindowListProviderImpl() {
  // Remove all remaining window observers.
  for (auto* window : window_list_) {
    CHECK(window->GetProperty(kManagedKey));
    window->RemoveObserver(this);
  }
  container_->RemoveObserver(this);
}

bool WindowListProviderImpl::IsValidWindow(aura::Window* window) const {
  if (wm::GetTransientParent(window))
    return false;

  // TODO(oshima): crbug.com/413912.
  return window->type() == ui::wm::WINDOW_TYPE_NORMAL ||
         window->type() == ui::wm::WINDOW_TYPE_PANEL;
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
  return std::find(window_list_.begin(), window_list_.end(), window) !=
         window_list_.end();
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

void WindowListProviderImpl::RecreateWindowList() {
  window_list_.clear();
  for (auto* window : container_->children()) {
    if (window->GetProperty(kManagedKey))
      window_list_.push_back(window);
  }
}

void WindowListProviderImpl::OnWindowAdded(aura::Window* window) {
  if (!IsValidWindow(window) || window->parent() != container_)
    return;

  window->SetProperty(kManagedKey, true);
  RecreateWindowList();
  DCHECK(IsWindowInList(window));
  window->AddObserver(this);
  FOR_EACH_OBSERVER(
      WindowListProviderObserver, observers_, OnWindowAddedToList(window));
}

void WindowListProviderImpl::OnWillRemoveWindow(aura::Window* window) {
  if (!window->GetProperty(kManagedKey))
    return;
  DCHECK(IsWindowInList(window));
  aura::Window::Windows::iterator find = std::find(window_list_.begin(),
                                                   window_list_.end(),
                                                   window);
  CHECK(find != window_list_.end());
  int index = find - window_list_.begin();
  window_list_.erase(find);
  window->ClearProperty(kManagedKey);
  window->RemoveObserver(this);
  FOR_EACH_OBSERVER(WindowListProviderObserver,
                    observers_,
                    OnWindowRemovedFromList(window, index));
}

void WindowListProviderImpl::OnWindowStackingChanged(aura::Window* window) {
  if (window == container_)
    return;
  DCHECK(IsWindowInList(window));
  RecreateWindowList();
  // Inform our listeners that the stacking has been changed.
  FOR_EACH_OBSERVER(
      WindowListProviderObserver, observers_, OnWindowStackingChangedInList());
}

}  // namespace athena
