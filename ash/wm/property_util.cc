// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/property_util.h"

#include "ash/ash_export.h"
#include "ash/screen_ash.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/rect.h"

namespace ash {
namespace {
bool g_default_windows_persist_across_all_workspaces = false;
}  // namespace

void SetRestoreBoundsInScreen(aura::Window* window, const gfx::Rect& bounds) {
  window->SetProperty(aura::client::kRestoreBoundsKey, new gfx::Rect(bounds));
}

void SetRestoreBoundsInParent(aura::Window* window, const gfx::Rect& bounds) {
  window->SetProperty(
      aura::client::kRestoreBoundsKey,
      new gfx::Rect(ScreenAsh::ConvertRectToScreen(window->parent(), bounds)));
}

const gfx::Rect* GetRestoreBoundsInScreen(aura::Window* window) {
  return window->GetProperty(aura::client::kRestoreBoundsKey);
}

gfx::Rect GetRestoreBoundsInParent(aura::Window* window) {
  const gfx::Rect* rect = window->GetProperty(aura::client::kRestoreBoundsKey);
  if (!rect)
    return gfx::Rect();
  return ScreenAsh::ConvertRectFromScreen(window->parent(), *rect);
}

void ClearRestoreBounds(aura::Window* window) {
  window->ClearProperty(aura::client::kRestoreBoundsKey);
}

void SetTrackedByWorkspace(aura::Window* window, bool value) {
  window->SetProperty(internal::kWindowTrackedByWorkspaceKey, value);
}

bool GetTrackedByWorkspace(aura::Window* window) {
  return window->GetProperty(internal::kWindowTrackedByWorkspaceKey);
}

void SetIgnoredByShelf(aura::Window* window, bool value) {
  window->SetProperty(internal::kIgnoredByShelfKey, value);
}

bool GetIgnoredByShelf(aura::Window* window) {
  return window->GetProperty(internal::kIgnoredByShelfKey);
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

internal::RootWindowController* GetRootWindowController(
    aura::RootWindow* root_window) {
  return root_window ?
      root_window->GetProperty(internal::kRootWindowControllerKey) : NULL;
}

void SetRootWindowController(aura::RootWindow* root_window,
                             internal::RootWindowController* controller) {
  root_window->SetProperty(internal::kRootWindowControllerKey, controller);
}

}  // namespace ash
