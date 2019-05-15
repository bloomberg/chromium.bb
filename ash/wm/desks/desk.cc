// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk.h"

#include <utility>

#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"

namespace ash {

class DeskContainerObserver : public aura::WindowObserver {
 public:
  DeskContainerObserver(Desk* owner, aura::Window* container)
      : owner_(owner), container_(container) {
    DCHECK_EQ(container_->id(), owner_->container_id());
    container->AddObserver(this);
  }

  ~DeskContainerObserver() override { container_->RemoveObserver(this); }

  // aura::WindowObserver:
  void OnWindowAdded(aura::Window* new_window) override {
    // TODO(afakhry): Overview mode creates a new widget for each window under
    // the same parent for the CaptionContainerView. We will be notified with
    // this window addition here. Consider ignoring these windows if they cause
    // problems.
    owner_->AddWindowToDesk(new_window);
  }

  void OnWindowDestroyed(aura::Window* window) override {
    // We should never get here. We should be notified in
    // `OnRootWindowClosing()` before the child containers of the root window
    // are destroyed, and this object should have already been destroyed.
    NOTREACHED();
  }

 private:
  Desk* const owner_;
  aura::Window* const container_;

  DISALLOW_COPY_AND_ASSIGN(DeskContainerObserver);
};

// -----------------------------------------------------------------------------
// Desk:

Desk::Desk(int associated_container_id)
    : container_id_(associated_container_id) {
  // For the very first default desk added during initialization, there won't be
  // any root windows yet. That's OK, OnRootWindowAdded() will be called
  // explicitly by the RootWindowController when they're initialized.
  for (aura::Window* root : Shell::GetAllRootWindows())
    OnRootWindowAdded(root);
}

Desk::~Desk() {
  DCHECK(windows_.empty()) << "DesksController should remove my windows first.";

  for (auto& observer : observers_) {
    observers_.RemoveObserver(&observer);
    observer.OnDeskDestroyed(this);
  }
}

void Desk::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void Desk::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void Desk::OnRootWindowAdded(aura::Window* root) {
  DCHECK(!roots_to_containers_observers_.count(root));

  aura::Window* desk_container = root->GetChildById(container_id_);
  DCHECK(desk_container);

  auto container_observer =
      std::make_unique<DeskContainerObserver>(this, desk_container);
  roots_to_containers_observers_.emplace(root, std::move(container_observer));
}

void Desk::OnRootWindowClosing(aura::Window* root) {
  const size_t count = roots_to_containers_observers_.erase(root);
  DCHECK(count);
}

void Desk::AddWindowToDesk(aura::Window* window) {
  if (windows_.count(window))
    return;

  for (auto* transient_window : wm::GetTransientTreeIterator(window)) {
    const auto result = windows_.emplace(transient_window);

    // GetTransientTreeIterator() starts iterating the transient tree hierarchy
    // from the root transient parent, which may include windows that we already
    // track from before.
    if (result.second)
      transient_window->AddObserver(this);
  }

  NotifyContentChanged();
}

std::unique_ptr<base::AutoReset<bool>>
Desk::GetScopedNotifyContentChangedDisabler() {
  return std::make_unique<base::AutoReset<bool>>(
      &should_notify_content_changed_, false);
}

void Desk::Activate(bool update_window_activation) {
  // Show the associated containers on all roots.
  for (aura::Window* root : Shell::GetAllRootWindows())
    root->GetChildById(container_id_)->Show();

  is_active_ = true;

  if (!update_window_activation || windows_.empty())
    return;

  // Activate the window on this desk that was most recently used right before
  // the user switched to another desk, so as not to break the user's workflow.
  for (auto* window :
       Shell::Get()->mru_window_tracker()->BuildMruWindowList()) {
    if (!windows_.contains(window))
      continue;

    // Do not activate minimized windows, otherwise they will unminimize.
    if (wm::GetWindowState(window)->IsMinimized())
      continue;

    wm::ActivateWindow(window);
    return;
  }
}

void Desk::Deactivate(bool update_window_activation) {
  auto* active_window = wm::GetActiveWindow();

  // Hide the associated containers on all roots.
  for (aura::Window* root : Shell::GetAllRootWindows())
    root->GetChildById(container_id_)->Hide();

  is_active_ = false;

  if (!update_window_activation)
    return;

  // Deactivate the active window (if it belongs to this desk; active window may
  // be on a different container, or one of the widgets created by overview mode
  // which are not considered desk windows) after this desk's associated
  // containers have been hidden. This is to prevent the focus controller from
  // activating another window on the same desk when the active window loses
  // focus.
  if (active_window && windows_.contains(active_window))
    wm::DeactivateWindow(active_window);
}

void Desk::MoveWindowsToDesk(Desk* target_desk) {
  DCHECK(target_desk);

  {
    // Throttle notifying the observers, while we move those windows and notify
    // them only once when done.
    base::AutoReset<bool> this_desk_throttled(&should_notify_content_changed_,
                                              false);
    base::AutoReset<bool> target_desk_throttled(
        &(target_desk->should_notify_content_changed_), false);

    for (auto* window : windows_) {
      window->RemoveObserver(this);

      // Add the window to the target desk before reparenting such that when the
      // target desk's DeskContainerObserver notices this reparenting and calls
      // AddWindowToDesk(), the window had already been added and there's no
      // need to iterate over its transient hierarchy.
      target_desk->windows_.emplace(window);
      window->AddObserver(target_desk);

      // Reparent windows to the target desk's container in the same root
      // window. Note that `windows_` may contain transient children, which may
      // not have the same parent as the source desk's container. So, only
      // reparent the windows which are direct children of the source desks'
      // container.
      // TODO(afakhry): Check if this is necessary.
      aura::Window* root = window->GetRootWindow();
      aura::Window* source_container = GetDeskContainerForRoot(root);
      aura::Window* target_container =
          target_desk->GetDeskContainerForRoot(root);
      if (window->parent() == source_container)
        target_container->AddChild(window);
    }

    windows_.clear();
  }

  NotifyContentChanged();
  target_desk->NotifyContentChanged();
}

aura::Window* Desk::GetDeskContainerForRoot(aura::Window* root) const {
  DCHECK(root);

  return root->GetChildById(container_id_);
}

void Desk::OnWindowDestroyed(aura::Window* window) {
  // We listen to `OnWindowDestroyed()` as opposed to `OnWindowDestroying()`
  // since we want to refresh the mini_views only after the window has been
  // removed from the window tree hierarchy.
  const size_t count = windows_.erase(window);
  DCHECK(count);

  // No need to refresh the mini_views if the destroyed window doesn't show up
  // there in the first place.
  if (!window->GetProperty(kHideInDeskMiniViewKey))
    NotifyContentChanged();
}

void Desk::NotifyContentChanged() {
  if (!should_notify_content_changed_)
    return;

  for (auto& observer : observers_)
    observer.OnContentChanged();
}

}  // namespace ash
