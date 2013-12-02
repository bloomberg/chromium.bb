// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/solo_window_tracker.h"

#include <algorithm>

#include "ash/ash_constants.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_observer.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

namespace ash {

namespace {

// A flag to enable/disable the solo window header across all root windows.
bool g_solo_header_enabled = true;

// Returns the containers from which a solo window is chosen.
std::vector<aura::Window*> GetContainers(aura::RootWindow* root_window) {
  int kContainerIds[] = {
    internal::kShellWindowId_DefaultContainer,
    internal::kShellWindowId_AlwaysOnTopContainer,
    // Docked windows never use the solo header, but regular windows move to the
    // docked container when dragged.
    internal::kShellWindowId_DockedContainer,
  };
  std::vector<aura::Window*> containers;
  for (size_t i = 0; i < arraysize(kContainerIds); ++i) {
    containers.push_back(
        Shell::GetContainer(root_window->window(), kContainerIds[i]));
  }
  return containers;
}

// Returns true if |child| and all of its ancestors are visible and neither
// |child| nor any its ancestors is animating hidden.
bool GetTargetVisibility(aura::Window* child) {
  for (aura::Window* window = child; window; window = window->parent()) {
    if (!window->TargetVisibility())
      return false;
  }
  return true;
}

// Returns true if |window| can use the solo window header. Returns false for
// windows that are:
// * Not drawn (for example, DragDropTracker uses one for mouse capture)
// * Modal alerts (it looks odd for headers to change when an alert opens)
// * Constrained windows (ditto)
bool IsValidCandidate(aura::Window* window) {
  return window->type() == aura::client::WINDOW_TYPE_NORMAL &&
      window->layer() &&
      window->layer()->type() != ui::LAYER_NOT_DRAWN &&
      window->GetProperty(aura::client::kModalKey) == ui::MODAL_TYPE_NONE &&
      !window->GetProperty(aura::client::kConstrainedWindowKey);
}

// Schedule's a paint of the window's entire bounds.
void SchedulePaint(aura::Window* window) {
  window->SchedulePaintInRect(gfx::Rect(window->bounds().size()));
}

}  // namespace


// Class which triggers a repaint of the window which is passed to the
// constructor whenever the window's show type changes. The window's non client
// view is responsible for updating whether it uses the solo header as part of
// the repaint by querying GetWindowWithSoloHeader().
class SoloWindowTracker::SoloWindowObserver
    : public ash::wm::WindowStateObserver {
 public:
  explicit SoloWindowObserver(aura::Window* window) : window_(window) {
    wm::GetWindowState(window_)->AddObserver(this);
  }

  virtual ~SoloWindowObserver() {
    wm::GetWindowState(window_)->RemoveObserver(this);
  }

 private:
  // ash::wm::WindowStateObserver override.
  virtual void OnWindowShowTypeChanged(
      ash::wm::WindowState* window_state,
      ash::wm::WindowShowType old_type) OVERRIDE {
    SchedulePaint(window_);
  }

  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(SoloWindowObserver);
};

SoloWindowTracker::SoloWindowTracker(aura::RootWindow* root_window)
    : containers_(GetContainers(root_window)),
      solo_window_(NULL) {
  for (size_t i = 0; i < containers_.size(); ++i)
    containers_[i]->AddObserver(this);
}

SoloWindowTracker::~SoloWindowTracker() {
  for (size_t i = 0; i < containers_.size(); ++i)
    containers_[i]->RemoveObserver(this);
}

// static
void SoloWindowTracker::SetSoloHeaderEnabled(bool enabled) {
  g_solo_header_enabled = enabled;
  std::vector<aura::Window*> root_windows =
      Shell::GetInstance()->GetAllRootWindows();
  for (size_t i = 0; i < root_windows.size(); ++i) {
    SoloWindowTracker* tracker =
        internal::GetRootWindowController(root_windows[i])->
            solo_window_tracker();
    if (tracker)
      tracker->UpdateSoloWindow(NULL);
  }
}

aura::Window* SoloWindowTracker::GetWindowWithSoloHeader() {
  bool use_solo_header = solo_window_ &&
      !wm::GetWindowState(solo_window_)->IsMaximizedOrFullscreen();
  return use_solo_header ? solo_window_ : NULL;
}

void SoloWindowTracker::UpdateSoloWindow(aura::Window* ignore_window) {
  std::vector<aura::Window*> candidates;
  // Avoid memory allocations for typical window counts.
  candidates.reserve(16);
  for (size_t i = 0; i < containers_.size(); ++i) {
    candidates.insert(candidates.end(),
                      containers_[i]->children().begin(),
                      containers_[i]->children().end());
  }

  aura::Window* old_solo_window = solo_window_;
  solo_window_ = NULL;
  if (g_solo_header_enabled && !AnyVisibleWindowDocked()) {
    for (size_t i = 0; i < candidates.size(); ++i) {
      aura::Window* candidate = candidates[i];
      // Various sorts of windows "don't count" for this computation.
      if (candidate == ignore_window ||
          !IsValidCandidate(candidate) ||
          !GetTargetVisibility(candidate)) {
        continue;
      }

      if (solo_window_) {
        // A window can only use the solo header if it is the only visible valid
        // candidate (and there are no visible docked windows).
        solo_window_ = NULL;
        break;
      } else {
        solo_window_ = candidate;
      }
    }
  }

  if (solo_window_ == old_solo_window)
    return;

  solo_window_observer_.reset(solo_window_ ?
      new SoloWindowObserver(solo_window_) : NULL);
  if (old_solo_window)
    SchedulePaint(old_solo_window);
  if (solo_window_)
    SchedulePaint(solo_window_);
}

bool SoloWindowTracker::AnyVisibleWindowDocked() const {
  // For the purpose of SoloWindowTracker, there is a visible docked window if
  // it causes the dock to have non-empty bounds. This is intentionally
  // different from:
  // DockedWindowLayoutManager::IsAnyWindowDocked() and
  // DockedWindowLayoutManager::is_dragged_window_docked().
  return !dock_bounds_.IsEmpty();
}

void SoloWindowTracker::OnWindowAdded(aura::Window* new_window) {
  UpdateSoloWindow(NULL);
}

void SoloWindowTracker::OnWillRemoveWindow(aura::Window* window) {
  UpdateSoloWindow(window);
}

void SoloWindowTracker::OnWindowVisibilityChanged(aura::Window* window,
                                                  bool visible) {
  // |window| may be a grandchild of |containers_|.
  std::vector<aura::Window*>::const_iterator it = std::find(
      containers_.begin(), containers_.end(), window->parent());
  if (it != containers_.end())
    UpdateSoloWindow(NULL);
}

void SoloWindowTracker::OnDockBoundsChanging(const gfx::Rect& new_bounds,
                                             Reason reason) {
  dock_bounds_ = new_bounds;
  UpdateSoloWindow(NULL);
}

}  // namespace ash
