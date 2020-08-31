// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/shell_surface_util.h"

#include <memory>

#include "ash/public/cpp/app_types.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "components/exo/permission.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(exo::Permission*)

namespace exo {

namespace {

DEFINE_UI_CLASS_PROPERTY_KEY(Surface*, kMainSurfaceKey, nullptr)

// Application Id set by the client.
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kApplicationIdKey, nullptr)

// Startup Id set by the client.
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kStartupIdKey, nullptr)

// Accessibility Id set by the client.
DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kClientAccessibilityIdKey, -1)

// Permission object allowing this window to activate itself.
DEFINE_UI_CLASS_PROPERTY_KEY(exo::Permission*, kPermissionKey, nullptr)

// Returns true if the component for a located event should be taken care of
// by the window system.
bool ShouldHTComponentBlocked(int component) {
  switch (component) {
    case HTCAPTION:
    case HTCLOSE:
    case HTMAXBUTTON:
    case HTMINBUTTON:
    case HTMENU:
    case HTSYSMENU:
      return true;
    default:
      return false;
  }
}

}  // namespace

void SetShellApplicationId(aura::Window* window,
                           const base::Optional<std::string>& id) {
  TRACE_EVENT1("exo", "SetApplicationId", "application_id", id ? *id : "null");

  if (id)
    window->SetProperty(kApplicationIdKey, *id);
  else
    window->ClearProperty(kApplicationIdKey);
}

const std::string* GetShellApplicationId(const aura::Window* window) {
  return window->GetProperty(kApplicationIdKey);
}

void SetArcAppType(aura::Window* window) {
  window->SetProperty(aura::client::kAppType,
                      static_cast<int>(ash::AppType::ARC_APP));
}

void SetShellStartupId(aura::Window* window,
                       const base::Optional<std::string>& id) {
  TRACE_EVENT1("exo", "SetStartupId", "startup_id", id ? *id : "null");

  if (id)
    window->SetProperty(kStartupIdKey, *id);
  else
    window->ClearProperty(kStartupIdKey);
}

const std::string* GetShellStartupId(aura::Window* window) {
  return window->GetProperty(kStartupIdKey);
}

void SetShellClientAccessibilityId(aura::Window* window,
                                   const base::Optional<int32_t>& id) {
  TRACE_EVENT1("exo", "SetClientAccessibilityId", "id",
               id ? base::NumberToString(*id) : "null");

  if (id)
    window->SetProperty(kClientAccessibilityIdKey, *id);
  else
    window->ClearProperty(kClientAccessibilityIdKey);
}

const base::Optional<int32_t> GetShellClientAccessibilityId(
    aura::Window* window) {
  auto id = window->GetProperty(kClientAccessibilityIdKey);
  if (id < 0)
    return base::nullopt;
  else
    return id;
}

void SetShellMainSurface(aura::Window* window, Surface* surface) {
  window->SetProperty(kMainSurfaceKey, surface);
}

Surface* GetShellMainSurface(const aura::Window* window) {
  return window->GetProperty(kMainSurfaceKey);
}

ShellSurfaceBase* GetShellSurfaceBaseForWindow(aura::Window* window) {
  // Only windows with a surface can have a shell surface.
  if (!GetShellMainSurface(window))
    return nullptr;
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (!widget)
    return nullptr;
  return static_cast<ShellSurfaceBase*>(widget->widget_delegate());
}

Surface* GetTargetSurfaceForLocatedEvent(ui::LocatedEvent* event) {
  aura::Window* window =
      WMHelper::GetInstance()->GetCaptureClient()->GetCaptureWindow();
  gfx::PointF location_in_target_f = event->location_f();

  if (!window)
    return Surface::AsSurface(static_cast<aura::Window*>(event->target()));

  Surface* main_surface = GetShellMainSurface(window);
  // Skip if the event is captured by non exo windows.
  if (!main_surface) {
    auto* widget = views::Widget::GetTopLevelWidgetForNativeView(window);
    if (!widget)
      return nullptr;
    main_surface = GetShellMainSurface(widget->GetNativeWindow());
    if (!main_surface)
      return nullptr;
  }

  while (true) {
    gfx::Point location_in_target = gfx::ToFlooredPoint(location_in_target_f);
    aura::Window* focused = window->GetEventHandlerForPoint(location_in_target);

    if (focused)
      return Surface::AsSurface(focused);

    // If the event falls into the place where the window system should care
    // about (i.e. window caption), do not check the transient parent but just
    // return nullptr. See b/149517682.
    if (window->delegate() &&
        ShouldHTComponentBlocked(
            window->delegate()->GetNonClientComponent(location_in_target))) {
      return nullptr;
    }

    aura::Window* parent_window = wm::GetTransientParent(window);

    if (!parent_window)
      return main_surface;

    aura::Window::ConvertPointToTarget(window, parent_window,
                                       &location_in_target_f);
    window = parent_window;
  }
}

namespace {

// An activation-permission object whose lifetime is tied to a window property.
class ScopedWindowActivationPermission : public Permission {
 public:
  ScopedWindowActivationPermission(aura::Window* window,
                                   base::TimeDelta timeout)
      : Permission(Permission::Capability::kActivate, timeout),
        window_(window) {
    Permission* other = window_->GetProperty(kPermissionKey);
    if (other) {
      other->Revoke();
    }
    window_->SetProperty(kPermissionKey, reinterpret_cast<Permission*>(this));
  }

  ~ScopedWindowActivationPermission() override {
    if (window_->GetProperty(kPermissionKey) == this)
      window_->ClearProperty(kPermissionKey);
  }

 private:
  aura::Window* window_;
};

}  // namespace

std::unique_ptr<Permission> GrantPermissionToActivate(aura::Window* window,
                                                      base::TimeDelta timeout) {
  return std::make_unique<ScopedWindowActivationPermission>(window, timeout);
}

bool HasPermissionToActivate(aura::Window* window) {
  Permission* permission = window->GetProperty(kPermissionKey);
  return permission && permission->Check(Permission::Capability::kActivate);
}

}  // namespace exo
