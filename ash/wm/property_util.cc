// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/property_util.h"

#include "ash/ash_export.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/rect.h"

namespace ash {
namespace {
bool g_default_windows_persist_across_all_workspaces = false;
}  // namespace

void SetRestoreBounds(aura::Window* window, const gfx::Rect& bounds) {
  window->SetProperty(aura::client::kRestoreBoundsKey, new gfx::Rect(bounds));
}

void SetRestoreBoundsIfNotSet(aura::Window* window) {
  if (!GetRestoreBounds(window))
    SetRestoreBounds(window, window->bounds());
}

const gfx::Rect* GetRestoreBounds(aura::Window* window) {
  return window->GetProperty(aura::client::kRestoreBoundsKey);
}

void ClearRestoreBounds(aura::Window* window) {
  window->ClearProperty(aura::client::kRestoreBoundsKey);
}

void ToggleMaximizedState(aura::Window* window) {
  window->SetProperty(aura::client::kShowStateKey,
                      wm::IsWindowMaximized(window) ? ui::SHOW_STATE_NORMAL
                                                    : ui::SHOW_STATE_MAXIMIZED);
}

void SetTrackedByWorkspace(aura::Window* window, bool value) {
  window->SetProperty(internal::kWindowTrackedByWorkspaceKey, value);
}

bool GetTrackedByWorkspace(aura::Window* window) {
  return window->GetProperty(internal::kWindowTrackedByWorkspaceKey);
}

void SetPersistsAcrossAllWorkspaces(
    aura::Window* window,
    WindowPersistsAcrossAllWorkspacesType type) {
  window->SetProperty(
      internal::kWindowPersistsAcrossAllWorkspacesKey, type);
}

bool GetPersistsAcrossAllWorkspaces(aura::Window* window) {
  switch (window->GetProperty(
      internal::kWindowPersistsAcrossAllWorkspacesKey)) {
    case WINDOW_PERSISTS_ACROSS_ALL_WORKSPACES_VALUE_YES:
      return true;
    case WINDOW_PERSISTS_ACROSS_ALL_WORKSPACES_VALUE_NO:
      return false;
    case WINDOW_PERSISTS_ACROSS_ALL_WORKSPACES_VALUE_DEFAULT:
      return g_default_windows_persist_across_all_workspaces;
  }
  return false;
}

void SetDefaultPersistsAcrossAllWorkspaces(bool value) {
  g_default_windows_persist_across_all_workspaces = value;
}

}
