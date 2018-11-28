// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zaura_shell.h"

#include <aura-shell-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include "ash/shell.h"
#include "components/exo/surface_observer.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wayland_display_observer.h"
#include "components/exo/wayland/wl_output.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/display_util.h"

namespace exo {
namespace wayland {

namespace {

// A property key containing a boolean set to true if na aura surface object is
// associated with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasAuraSurfaceKey, false);

////////////////////////////////////////////////////////////////////////////////
// aura_surface_interface:

class AuraSurface : public SurfaceObserver {
 public:
  explicit AuraSurface(Surface* surface) : surface_(surface) {
    surface_->AddSurfaceObserver(this);
    surface_->SetProperty(kSurfaceHasAuraSurfaceKey, true);
  }
  ~AuraSurface() override {
    if (surface_) {
      surface_->RemoveSurfaceObserver(this);
      surface_->SetProperty(kSurfaceHasAuraSurfaceKey, false);
    }
  }

  void SetFrame(SurfaceFrameType type) {
    if (surface_)
      surface_->SetFrame(type);
  }

  void SetFrameColors(SkColor active_frame_color,
                      SkColor inactive_frame_color) {
    if (surface_)
      surface_->SetFrameColors(active_frame_color, inactive_frame_color);
  }

  void SetParent(AuraSurface* parent, const gfx::Point& position) {
    if (surface_)
      surface_->SetParent(parent ? parent->surface_ : nullptr, position);
  }

  void SetStartupId(const char* startup_id) {
    if (surface_)
      surface_->SetStartupId(startup_id);
  }

  void SetApplicationId(const char* application_id) {
    if (surface_)
      surface_->SetApplicationId(application_id);
  }

  void SetClientSurfaceId(int client_surface_id) {
    if (surface_)
      surface_->SetClientSurfaceId(client_surface_id);
  }

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override {
    surface->RemoveSurfaceObserver(this);
    surface_ = nullptr;
  }

 private:
  Surface* surface_;

  DISALLOW_COPY_AND_ASSIGN(AuraSurface);
};

SurfaceFrameType AuraSurfaceFrameType(uint32_t frame_type) {
  switch (frame_type) {
    case ZAURA_SURFACE_FRAME_TYPE_NONE:
      return SurfaceFrameType::NONE;
    case ZAURA_SURFACE_FRAME_TYPE_NORMAL:
      return SurfaceFrameType::NORMAL;
    case ZAURA_SURFACE_FRAME_TYPE_SHADOW:
      return SurfaceFrameType::SHADOW;
    default:
      VLOG(2) << "Unkonwn aura-shell frame type: " << frame_type;
      return SurfaceFrameType::NONE;
  }
}

void aura_surface_set_frame(wl_client* client,
                            wl_resource* resource,
                            uint32_t type) {
  GetUserDataAs<AuraSurface>(resource)->SetFrame(AuraSurfaceFrameType(type));
}

void aura_surface_set_parent(wl_client* client,
                             wl_resource* resource,
                             wl_resource* parent_resource,
                             int32_t x,
                             int32_t y) {
  GetUserDataAs<AuraSurface>(resource)->SetParent(
      parent_resource ? GetUserDataAs<AuraSurface>(parent_resource) : nullptr,
      gfx::Point(x, y));
}

void aura_surface_set_frame_colors(wl_client* client,
                                   wl_resource* resource,
                                   uint32_t active_color,
                                   uint32_t inactive_color) {
  GetUserDataAs<AuraSurface>(resource)->SetFrameColors(active_color,
                                                       inactive_color);
}

void aura_surface_set_startup_id(wl_client* client,
                                 wl_resource* resource,
                                 const char* startup_id) {
  GetUserDataAs<AuraSurface>(resource)->SetStartupId(startup_id);
}

void aura_surface_set_application_id(wl_client* client,
                                     wl_resource* resource,
                                     const char* application_id) {
  GetUserDataAs<AuraSurface>(resource)->SetApplicationId(application_id);
}

void aura_surface_set_client_surface_id(wl_client* client,
                                        wl_resource* resource,
                                        int client_surface_id) {
  GetUserDataAs<AuraSurface>(resource)->SetClientSurfaceId(client_surface_id);
}

const struct zaura_surface_interface aura_surface_implementation = {
    aura_surface_set_frame,          aura_surface_set_parent,
    aura_surface_set_frame_colors,   aura_surface_set_startup_id,
    aura_surface_set_application_id, aura_surface_set_client_surface_id};

////////////////////////////////////////////////////////////////////////////////
// aura_output_interface:

class AuraOutput : public WaylandDisplayObserver::ScaleObserver {
 public:
  explicit AuraOutput(wl_resource* resource) : resource_(resource) {}

  // Overridden from WaylandDisplayObserver::ScaleObserver:
  void OnDisplayScalesChanged(const display::Display& display) override {
    display::DisplayManager* display_manager =
        ash::Shell::Get()->display_manager();
    const display::ManagedDisplayInfo& display_info =
        display_manager->GetDisplayInfo(display.id());

    if (wl_resource_get_version(resource_) >=
        ZAURA_OUTPUT_SCALE_SINCE_VERSION) {
      display::ManagedDisplayMode active_mode;
      bool rv = display_manager->GetActiveModeForDisplayId(display.id(),
                                                           &active_mode);
      DCHECK(rv);
      const int32_t current_output_scale =
          std::round(display_info.zoom_factor() * 1000.f);
      std::vector<float> zoom_factors =
          display::GetDisplayZoomFactors(active_mode);

      // Ensure that the current zoom factor is a part of the list.
      auto it = std::find_if(
          zoom_factors.begin(), zoom_factors.end(),
          [&display_info](float zoom_factor) -> bool {
            return std::abs(display_info.zoom_factor() - zoom_factor) <=
                   std::numeric_limits<float>::epsilon();
          });
      if (it == zoom_factors.end())
        zoom_factors.push_back(display_info.zoom_factor());

      for (float zoom_factor : zoom_factors) {
        int32_t output_scale = std::round(zoom_factor * 1000.f);
        uint32_t flags = 0;
        if (output_scale == 1000)
          flags |= ZAURA_OUTPUT_SCALE_PROPERTY_PREFERRED;
        if (current_output_scale == output_scale)
          flags |= ZAURA_OUTPUT_SCALE_PROPERTY_CURRENT;

        // TODO(malaykeshav): This can be removed in the future when client
        // has been updated.
        if (wl_resource_get_version(resource_) < 6)
          output_scale = std::round(1000.f / zoom_factor);

        zaura_output_send_scale(resource_, flags, output_scale);
      }
    }

    if (wl_resource_get_version(resource_) >=
        ZAURA_OUTPUT_CONNECTION_SINCE_VERSION) {
      zaura_output_send_connection(resource_,
                                   display.IsInternal()
                                       ? ZAURA_OUTPUT_CONNECTION_TYPE_INTERNAL
                                       : ZAURA_OUTPUT_CONNECTION_TYPE_UNKNOWN);
    }

    if (wl_resource_get_version(resource_) >=
        ZAURA_OUTPUT_DEVICE_SCALE_FACTOR_SINCE_VERSION) {
      zaura_output_send_device_scale_factor(
          resource_, display_info.device_scale_factor() * 1000);
    }
  }

 private:
  wl_resource* const resource_;

  DISALLOW_COPY_AND_ASSIGN(AuraOutput);
};

////////////////////////////////////////////////////////////////////////////////
// aura_shell_interface:

void aura_shell_get_aura_surface(wl_client* client,
                                 wl_resource* resource,
                                 uint32_t id,
                                 wl_resource* surface_resource) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  if (surface->GetProperty(kSurfaceHasAuraSurfaceKey)) {
    wl_resource_post_error(
        resource, ZAURA_SHELL_ERROR_AURA_SURFACE_EXISTS,
        "an aura surface object for that surface already exists");
    return;
  }

  wl_resource* aura_surface_resource = wl_resource_create(
      client, &zaura_surface_interface, wl_resource_get_version(resource), id);

  SetImplementation(aura_surface_resource, &aura_surface_implementation,
                    std::make_unique<AuraSurface>(surface));
}

void aura_shell_get_aura_output(wl_client* client,
                                wl_resource* resource,
                                uint32_t id,
                                wl_resource* output_resource) {
  WaylandDisplayObserver* display_observer =
      GetUserDataAs<WaylandDisplayObserver>(output_resource);
  if (display_observer->HasScaleObserver()) {
    wl_resource_post_error(
        resource, ZAURA_SHELL_ERROR_AURA_OUTPUT_EXISTS,
        "an aura output object for that output already exists");
    return;
  }

  wl_resource* aura_output_resource = wl_resource_create(
      client, &zaura_output_interface, wl_resource_get_version(resource), id);

  auto aura_output = std::make_unique<AuraOutput>(aura_output_resource);
  display_observer->SetScaleObserver(aura_output->AsWeakPtr());

  SetImplementation(aura_output_resource, nullptr, std::move(aura_output));
}

const struct zaura_shell_interface aura_shell_implementation = {
    aura_shell_get_aura_surface, aura_shell_get_aura_output};

}  // namespace

void bind_aura_shell(wl_client* client,
                     void* data,
                     uint32_t version,
                     uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zaura_shell_interface,
                         std::min(version, kZAuraShellVersion), id);

  wl_resource_set_implementation(resource, &aura_shell_implementation, nullptr,
                                 nullptr);
}

}  // namespace wayland
}  // namespace exo
