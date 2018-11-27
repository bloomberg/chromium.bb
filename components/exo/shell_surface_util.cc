// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/shell_surface_util.h"

#include "base/trace_event/trace_event.h"
#include "components/exo/surface.h"
#include "ui/aura/window.h"

namespace exo {

namespace {

DEFINE_LOCAL_UI_CLASS_PROPERTY_KEY(Surface*, kMainSurfaceKey, nullptr)

// Application Id set by the client.
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kApplicationIdKey, nullptr);

// Application Id set by the client.
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kStartupIdKey, nullptr);

}  // namespace

void SetShellApplicationId(aura::Window* window,
                           const base::Optional<std::string>& id) {
  TRACE_EVENT1("exo", "SetApplicationId", "application_id", id ? *id : "null");

  if (id)
    window->SetProperty(kApplicationIdKey, new std::string(*id));
  else
    window->ClearProperty(kApplicationIdKey);
}

const std::string* GetShellApplicationId(const aura::Window* window) {
  return window->GetProperty(kApplicationIdKey);
}

void SetShellStartupId(aura::Window* window,
                       const base::Optional<std::string>& id) {
  TRACE_EVENT1("exo", "SetStartupId", "startup_id", id ? *id : "null");

  if (id)
    window->SetProperty(kStartupIdKey, new std::string(*id));
  else
    window->ClearProperty(kStartupIdKey);
}

const std::string* GetShellStartupId(aura::Window* window) {
  return window->GetProperty(kStartupIdKey);
}

void SetShellMainSurface(aura::Window* window, Surface* surface) {
  window->SetProperty(kMainSurfaceKey, surface);
}

Surface* GetShellMainSurface(const aura::Window* window) {
  return window->GetProperty(kMainSurfaceKey);
}

}  // namespace exo
