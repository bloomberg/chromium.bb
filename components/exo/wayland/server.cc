// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/server.h"

#include <linux/input.h>
#include <stddef.h>
#include <stdint.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>
#include <xdg-shell-unstable-v5-server-protocol.h>
#include <algorithm>
#include <utility>

#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/buffer.h"
#include "components/exo/display.h"
#include "components/exo/keyboard.h"
#include "components/exo/keyboard_delegate.h"
#include "components/exo/pointer.h"
#include "components/exo/pointer_delegate.h"
#include "components/exo/shared_memory.h"
#include "components/exo/shell_surface.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/touch.h"
#include "components/exo/touch_delegate.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/window_property.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

#if defined(USE_OZONE)
#include <wayland-drm-server-protocol.h>
#endif

#if defined(USE_XKBCOMMON)
#include <xkbcommon/xkbcommon.h>
#include "ui/events/keycodes/scoped_xkb.h"
#endif

DECLARE_WINDOW_PROPERTY_TYPE(wl_resource*);

namespace exo {
namespace wayland {
namespace {

template <class T>
T* GetUserDataAs(wl_resource* resource) {
  return static_cast<T*>(wl_resource_get_user_data(resource));
}

template <class T>
scoped_ptr<T> TakeUserDataAs(wl_resource* resource) {
  scoped_ptr<T> user_data = make_scoped_ptr(GetUserDataAs<T>(resource));
  wl_resource_set_user_data(resource, nullptr);
  return user_data;
}

template <class T>
void DestroyUserData(wl_resource* resource) {
  TakeUserDataAs<T>(resource);
}

template <class T>
void SetImplementation(wl_resource* resource,
                       const void* implementation,
                       scoped_ptr<T> user_data) {
  wl_resource_set_implementation(resource, implementation, user_data.release(),
                                 DestroyUserData<T>);
}

// A property key containing the surface resource that is associated with
// window. If unset, no surface resource is associated with window.
DEFINE_WINDOW_PROPERTY_KEY(wl_resource*, kSurfaceResourceKey, nullptr);

////////////////////////////////////////////////////////////////////////////////
// wl_buffer_interface:

void buffer_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_buffer_interface buffer_implementation = {buffer_destroy};

void HandleBufferReleaseCallback(wl_resource* resource) {
  wl_buffer_send_release(resource);
  wl_client_flush(wl_resource_get_client(resource));
}

////////////////////////////////////////////////////////////////////////////////
// wl_surface_interface:

void surface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void surface_attach(wl_client* client,
                    wl_resource* resource,
                    wl_resource* buffer,
                    int32_t x,
                    int32_t y) {
  // TODO(reveman): Implement buffer offset support.
  if (x || y) {
    wl_resource_post_no_memory(resource);
    return;
  }

  GetUserDataAs<Surface>(resource)
      ->Attach(buffer ? GetUserDataAs<Buffer>(buffer) : nullptr);
}

void surface_damage(wl_client* client,
                    wl_resource* resource,
                    int32_t x,
                    int32_t y,
                    int32_t width,
                    int32_t height) {
  GetUserDataAs<Surface>(resource)->Damage(gfx::Rect(x, y, width, height));
}

void HandleSurfaceFrameCallback(wl_resource* resource,
                                base::TimeTicks frame_time) {
  if (!frame_time.is_null()) {
    wl_callback_send_done(resource,
                          (frame_time - base::TimeTicks()).InMilliseconds());
    // TODO(reveman): Remove this potentially blocking flush and instead watch
    // the file descriptor to be ready for write without blocking.
    wl_client_flush(wl_resource_get_client(resource));
  }
  wl_resource_destroy(resource);
}

void surface_frame(wl_client* client,
                   wl_resource* resource,
                   uint32_t callback) {
  wl_resource* callback_resource =
      wl_resource_create(client, &wl_callback_interface, 1, callback);
  if (!callback_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  // base::Unretained is safe as the resource owns the callback.
  scoped_ptr<base::CancelableCallback<void(base::TimeTicks)>>
  cancelable_callback(
      new base::CancelableCallback<void(base::TimeTicks)>(base::Bind(
          &HandleSurfaceFrameCallback, base::Unretained(callback_resource))));

  GetUserDataAs<Surface>(resource)
      ->RequestFrameCallback(cancelable_callback->callback());

  SetImplementation(callback_resource, nullptr, std::move(cancelable_callback));
}

void surface_set_opaque_region(wl_client* client,
                               wl_resource* resource,
                               wl_resource* region_resource) {
  GetUserDataAs<Surface>(resource)->SetOpaqueRegion(
      region_resource ? *GetUserDataAs<SkRegion>(region_resource)
                      : SkRegion(SkIRect::MakeEmpty()));
}

void surface_set_input_region(wl_client* client,
                              wl_resource* resource,
                              wl_resource* region_resource) {
  GetUserDataAs<Surface>(resource)->SetInputRegion(
      region_resource ? *GetUserDataAs<SkRegion>(region_resource)
                      : SkRegion(SkIRect::MakeLargest()));
}

void surface_commit(wl_client* client, wl_resource* resource) {
  GetUserDataAs<Surface>(resource)->Commit();
}

void surface_set_buffer_transform(wl_client* client,
                                  wl_resource* resource,
                                  int transform) {
  NOTIMPLEMENTED();
}

void surface_set_buffer_scale(wl_client* client,
                              wl_resource* resource,
                              int32_t scale) {
  if (scale < 1) {
    wl_resource_post_error(resource, WL_SURFACE_ERROR_INVALID_SCALE,
                           "buffer scale must be at least one "
                           "('%d' specified)",
                           scale);
    return;
  }

  GetUserDataAs<Surface>(resource)->SetBufferScale(scale);
}

const struct wl_surface_interface surface_implementation = {
    surface_destroy,
    surface_attach,
    surface_damage,
    surface_frame,
    surface_set_opaque_region,
    surface_set_input_region,
    surface_commit,
    surface_set_buffer_transform,
    surface_set_buffer_scale};

////////////////////////////////////////////////////////////////////////////////
// wl_region_interface:

void region_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void region_add(wl_client* client,
                wl_resource* resource,
                int32_t x,
                int32_t y,
                int32_t width,
                int32_t height) {
  GetUserDataAs<SkRegion>(resource)
      ->op(SkIRect::MakeXYWH(x, y, width, height), SkRegion::kUnion_Op);
}

static void region_subtract(wl_client* client,
                            wl_resource* resource,
                            int32_t x,
                            int32_t y,
                            int32_t width,
                            int32_t height) {
  GetUserDataAs<SkRegion>(resource)
      ->op(SkIRect::MakeXYWH(x, y, width, height), SkRegion::kDifference_Op);
}

const struct wl_region_interface region_implementation = {
    region_destroy, region_add, region_subtract};

////////////////////////////////////////////////////////////////////////////////
// wl_compositor_interface:

void compositor_create_surface(wl_client* client,
                               wl_resource* resource,
                               uint32_t id) {
  scoped_ptr<Surface> surface =
      GetUserDataAs<Display>(resource)->CreateSurface();
  DCHECK(surface);

  wl_resource* surface_resource = wl_resource_create(
      client, &wl_surface_interface, wl_resource_get_version(resource), id);
  if (!surface_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  // Set the surface resource property for type-checking downcast support.
  surface->SetProperty(kSurfaceResourceKey, surface_resource);

  SetImplementation(surface_resource, &surface_implementation,
                    std::move(surface));
}

void compositor_create_region(wl_client* client,
                              wl_resource* resource,
                              uint32_t id) {
  scoped_ptr<SkRegion> region(new SkRegion);

  wl_resource* region_resource =
      wl_resource_create(client, &wl_region_interface, 1, id);
  if (!region_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  SetImplementation(region_resource, &region_implementation, std::move(region));
}

const struct wl_compositor_interface compositor_implementation = {
    compositor_create_surface, compositor_create_region};

const uint32_t compositor_version = 3;

void bind_compositor(wl_client* client,
                     void* data,
                     uint32_t version,
                     uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wl_compositor_interface,
                         std::min(version, compositor_version), id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &compositor_implementation, data,
                                 nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// wl_shm_pool_interface:

const struct shm_supported_format {
  uint32_t shm_format;
  gfx::BufferFormat buffer_format;
} shm_supported_formats[] = {
    {WL_SHM_FORMAT_XBGR8888, gfx::BufferFormat::RGBX_8888},
    {WL_SHM_FORMAT_ABGR8888, gfx::BufferFormat::RGBA_8888},
    {WL_SHM_FORMAT_XRGB8888, gfx::BufferFormat::BGRX_8888},
    {WL_SHM_FORMAT_ARGB8888, gfx::BufferFormat::BGRA_8888}};

void shm_pool_create_buffer(wl_client* client,
                            wl_resource* resource,
                            uint32_t id,
                            int32_t offset,
                            int32_t width,
                            int32_t height,
                            int32_t stride,
                            uint32_t format) {
  const auto* supported_format =
      std::find_if(shm_supported_formats,
                   shm_supported_formats + arraysize(shm_supported_formats),
                   [format](const shm_supported_format& supported_format) {
                     return supported_format.shm_format == format;
                   });
  if (supported_format ==
      (shm_supported_formats + arraysize(shm_supported_formats))) {
    wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FORMAT,
                           "invalid format 0x%x", format);
    return;
  }

  if (offset < 0) {
    wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FORMAT,
                           "invalid offset %d", offset);
    return;
  }

  scoped_ptr<Buffer> buffer =
      GetUserDataAs<SharedMemory>(resource)
          ->CreateBuffer(gfx::Size(width, height),
                         supported_format->buffer_format, offset, stride);
  if (!buffer) {
    wl_resource_post_no_memory(resource);
    return;
  }

  wl_resource* buffer_resource =
      wl_resource_create(client, &wl_buffer_interface, 1, id);
  if (!buffer_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  buffer->set_release_callback(base::Bind(&HandleBufferReleaseCallback,
                                          base::Unretained(buffer_resource)));

  SetImplementation(buffer_resource, &buffer_implementation, std::move(buffer));
}

void shm_pool_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void shm_pool_resize(wl_client* client, wl_resource* resource, int32_t size) {
  // Nothing to do here.
}

const struct wl_shm_pool_interface shm_pool_implementation = {
    shm_pool_create_buffer, shm_pool_destroy, shm_pool_resize};

////////////////////////////////////////////////////////////////////////////////
// wl_shm_interface:

void shm_create_pool(wl_client* client,
                     wl_resource* resource,
                     uint32_t id,
                     int fd,
                     int32_t size) {
  scoped_ptr<SharedMemory> shared_memory =
      GetUserDataAs<Display>(resource)
          ->CreateSharedMemory(base::FileDescriptor(fd, true), size);
  if (!shared_memory) {
    wl_resource_post_no_memory(resource);
    return;
  }

  wl_resource* shm_pool_resource =
      wl_resource_create(client, &wl_shm_pool_interface, 1, id);
  if (!shm_pool_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  SetImplementation(shm_pool_resource, &shm_pool_implementation,
                    std::move(shared_memory));
}

const struct wl_shm_interface shm_implementation = {shm_create_pool};

void bind_shm(wl_client* client, void* data, uint32_t version, uint32_t id) {
  wl_resource* resource = wl_resource_create(client, &wl_shm_interface, 1, id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &shm_implementation, data, nullptr);

  for (const auto& supported_format : shm_supported_formats)
    wl_shm_send_format(resource, supported_format.shm_format);
}

#if defined(USE_OZONE)

////////////////////////////////////////////////////////////////////////////////
// wl_drm_interface:

const struct drm_supported_format {
  uint32_t drm_format;
  gfx::BufferFormat buffer_format;
} drm_supported_formats[] = {
    {WL_DRM_FORMAT_XBGR8888, gfx::BufferFormat::RGBX_8888},
    {WL_DRM_FORMAT_ABGR8888, gfx::BufferFormat::RGBA_8888},
    {WL_DRM_FORMAT_XRGB8888, gfx::BufferFormat::BGRX_8888},
    {WL_DRM_FORMAT_ARGB8888, gfx::BufferFormat::BGRA_8888}};

void drm_authenticate(wl_client* client, wl_resource* resource, uint32_t id) {
  wl_drm_send_authenticated(resource);
}

void drm_create_buffer(wl_client* client,
                       wl_resource* resource,
                       uint32_t id,
                       uint32_t name,
                       int32_t width,
                       int32_t height,
                       uint32_t stride,
                       uint32_t format) {
  wl_resource_post_error(resource, WL_DRM_ERROR_INVALID_NAME,
                         "GEM names are not supported");
}

void drm_create_planar_buffer(wl_client* client,
                              wl_resource* resource,
                              uint32_t id,
                              uint32_t name,
                              int32_t width,
                              int32_t height,
                              uint32_t format,
                              int32_t offset0,
                              int32_t stride0,
                              int32_t offset1,
                              int32_t stride1,
                              int32_t offset2,
                              int32_t stride3) {
  wl_resource_post_error(resource, WL_DRM_ERROR_INVALID_NAME,
                         "GEM names are not supported");
}

void drm_create_prime_buffer(wl_client* client,
                             wl_resource* resource,
                             uint32_t id,
                             int32_t name,
                             int32_t width,
                             int32_t height,
                             uint32_t format,
                             int32_t offset0,
                             int32_t stride0,
                             int32_t offset1,
                             int32_t stride1,
                             int32_t offset2,
                             int32_t stride2) {
  const auto* supported_format =
      std::find_if(drm_supported_formats,
                   drm_supported_formats + arraysize(drm_supported_formats),
                   [format](const drm_supported_format& supported_format) {
                     return supported_format.drm_format == format;
                   });
  if (supported_format ==
      (drm_supported_formats + arraysize(drm_supported_formats))) {
    wl_resource_post_error(resource, WL_DRM_ERROR_INVALID_FORMAT,
                           "invalid format 0x%x", format);
    return;
  }

  scoped_ptr<Buffer> buffer =
      GetUserDataAs<Display>(resource)
          ->CreatePrimeBuffer(base::ScopedFD(name), gfx::Size(width, height),
                              supported_format->buffer_format, stride0);
  if (!buffer) {
    wl_resource_post_no_memory(resource);
    return;
  }

  wl_resource* buffer_resource =
      wl_resource_create(client, &wl_buffer_interface, 1, id);
  if (!buffer_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  buffer->set_release_callback(base::Bind(&HandleBufferReleaseCallback,
                                          base::Unretained(buffer_resource)));

  SetImplementation(buffer_resource, &buffer_implementation, std::move(buffer));
}

const struct wl_drm_interface drm_implementation = {
    drm_authenticate, drm_create_buffer, drm_create_planar_buffer,
    drm_create_prime_buffer};

const uint32_t drm_version = 2;

void bind_drm(wl_client* client, void* data, uint32_t version, uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &wl_drm_interface, std::min(version, drm_version), id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(resource, &drm_implementation, data, nullptr);

  if (version >= 2)
    wl_drm_send_capabilities(resource, WL_DRM_CAPABILITY_PRIME);

  for (const auto& supported_format : drm_supported_formats)
    wl_drm_send_format(resource, supported_format.drm_format);
}
#endif

////////////////////////////////////////////////////////////////////////////////
// wl_subsurface_interface:

void subsurface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void subsurface_set_position(wl_client* client,
                             wl_resource* resource,
                             int32_t x,
                             int32_t y) {
  GetUserDataAs<SubSurface>(resource)->SetPosition(gfx::Point(x, y));
}

void subsurface_place_above(wl_client* client,
                            wl_resource* resource,
                            wl_resource* reference_resource) {
  GetUserDataAs<SubSurface>(resource)
      ->PlaceAbove(GetUserDataAs<Surface>(reference_resource));
}

void subsurface_place_below(wl_client* client,
                            wl_resource* resource,
                            wl_resource* sibling_resource) {
  GetUserDataAs<SubSurface>(resource)
      ->PlaceBelow(GetUserDataAs<Surface>(sibling_resource));
}

void subsurface_set_sync(wl_client* client, wl_resource* resource) {
  GetUserDataAs<SubSurface>(resource)->SetCommitBehavior(true);
}

void subsurface_set_desync(wl_client* client, wl_resource* resource) {
  GetUserDataAs<SubSurface>(resource)->SetCommitBehavior(false);
}

const struct wl_subsurface_interface subsurface_implementation = {
    subsurface_destroy,     subsurface_set_position, subsurface_place_above,
    subsurface_place_below, subsurface_set_sync,     subsurface_set_desync};

////////////////////////////////////////////////////////////////////////////////
// wl_subcompositor_interface:

void subcompositor_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void subcompositor_get_subsurface(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t id,
                                  wl_resource* surface,
                                  wl_resource* parent) {
  scoped_ptr<SubSurface> subsurface =
      GetUserDataAs<Display>(resource)->CreateSubSurface(
          GetUserDataAs<Surface>(surface), GetUserDataAs<Surface>(parent));
  if (!subsurface) {
    wl_resource_post_no_memory(resource);
    return;
  }

  wl_resource* subsurface_resource =
      wl_resource_create(client, &wl_subsurface_interface, 1, id);
  if (!subsurface_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  SetImplementation(subsurface_resource, &subsurface_implementation,
                    std::move(subsurface));
}

const struct wl_subcompositor_interface subcompositor_implementation = {
    subcompositor_destroy, subcompositor_get_subsurface};

void bind_subcompositor(wl_client* client,
                        void* data,
                        uint32_t version,
                        uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wl_subcompositor_interface, 1, id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(resource, &subcompositor_implementation, data,
                                 nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// wl_shell_surface_interface:

void shell_surface_pong(wl_client* client,
                        wl_resource* resource,
                        uint32_t serial) {
  NOTIMPLEMENTED();
}

void shell_surface_move(wl_client* client,
                        wl_resource* resource,
                        wl_resource* seat_resource,
                        uint32_t serial) {
  GetUserDataAs<ShellSurface>(resource)->Move();
}

void shell_surface_resize(wl_client* client,
                          wl_resource* resource,
                          wl_resource* seat_resource,
                          uint32_t serial,
                          uint32_t edges) {
  NOTIMPLEMENTED();
}

void shell_surface_set_toplevel(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->SetEnabled(true);
}

void shell_surface_set_transient(wl_client* client,
                                 wl_resource* resource,
                                 wl_resource* parent_resource,
                                 int x,
                                 int y,
                                 uint32_t flags) {
  NOTIMPLEMENTED();
}

void shell_surface_set_fullscreen(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t method,
                                  uint32_t framerate,
                                  wl_resource* output_resource) {
  GetUserDataAs<ShellSurface>(resource)->SetEnabled(true);
  GetUserDataAs<ShellSurface>(resource)->SetFullscreen(true);
}

void shell_surface_set_popup(wl_client* client,
                             wl_resource* resource,
                             wl_resource* seat_resource,
                             uint32_t serial,
                             wl_resource* parent_resource,
                             int32_t x,
                             int32_t y,
                             uint32_t flags) {
  NOTIMPLEMENTED();
}

void shell_surface_set_maximized(wl_client* client,
                                 wl_resource* resource,
                                 wl_resource* output_resource) {
  GetUserDataAs<ShellSurface>(resource)->SetEnabled(true);
  GetUserDataAs<ShellSurface>(resource)->Maximize();
}

void shell_surface_set_title(wl_client* client,
                             wl_resource* resource,
                             const char* title) {
  GetUserDataAs<ShellSurface>(resource)
      ->SetTitle(base::string16(base::UTF8ToUTF16(title)));
}

void shell_surface_set_class(wl_client* client,
                             wl_resource* resource,
                             const char* clazz) {
  GetUserDataAs<ShellSurface>(resource)->SetApplicationId(clazz);
}

const struct wl_shell_surface_interface shell_surface_implementation = {
    shell_surface_pong,          shell_surface_move,
    shell_surface_resize,        shell_surface_set_toplevel,
    shell_surface_set_transient, shell_surface_set_fullscreen,
    shell_surface_set_popup,     shell_surface_set_maximized,
    shell_surface_set_title,     shell_surface_set_class};

////////////////////////////////////////////////////////////////////////////////
// wl_shell_interface:

void HandleShellSurfaceConfigureCallback(wl_resource* resource,
                                         const gfx::Size& size,
                                         ash::wm::WindowStateType state_type,
                                         bool activated) {
  wl_shell_surface_send_configure(resource, WL_SHELL_SURFACE_RESIZE_NONE,
                                  size.width(), size.height());
  wl_client_flush(wl_resource_get_client(resource));
}

void shell_get_shell_surface(wl_client* client,
                             wl_resource* resource,
                             uint32_t id,
                             wl_resource* surface) {
  scoped_ptr<ShellSurface> shell_surface =
      GetUserDataAs<Display>(resource)
          ->CreateShellSurface(GetUserDataAs<Surface>(surface));
  if (!shell_surface) {
    wl_resource_post_no_memory(resource);
    return;
  }

  wl_resource* shell_surface_resource =
      wl_resource_create(client, &wl_shell_surface_interface, 1, id);
  if (!shell_surface_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  // Shell surfaces are initially disabled and needs to be explicitly mapped
  // before they are enabled and can become visible.
  shell_surface->SetEnabled(false);

  shell_surface->set_surface_destroyed_callback(base::Bind(
      &wl_resource_destroy, base::Unretained(shell_surface_resource)));

  shell_surface->set_configure_callback(
      base::Bind(&HandleShellSurfaceConfigureCallback,
                 base::Unretained(shell_surface_resource)));

  SetImplementation(shell_surface_resource, &shell_surface_implementation,
                    std::move(shell_surface));
}

const struct wl_shell_interface shell_implementation = {
    shell_get_shell_surface};

void bind_shell(wl_client* client, void* data, uint32_t version, uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wl_shell_interface, 1, id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(resource, &shell_implementation, data,
                                 nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// wl_output_interface:

const uint32_t output_version = 2;

void bind_output(wl_client* client, void* data, uint32_t version, uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &wl_output_interface, std::min(version, output_version), id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  // TODO(reveman): Watch for display changes and report them.
  // TODO(reveman): Multi-display support.
  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();
  const gfx::Display& primary = display_manager->GetPrimaryDisplayCandidate();

  const ash::DisplayInfo& info = display_manager->GetDisplayInfo(primary.id());
  const float kInchInMm = 25.4f;
  const char* kUnknownMake = "unknown";
  const char* kUnknownModel = "unknown";
  gfx::Rect bounds = info.bounds_in_native();
  // TODO(reveman): Send the actual active device rotation.
  wl_output_send_geometry(
      resource, bounds.x(), bounds.y(),
      static_cast<int>(kInchInMm * bounds.width() / info.device_dpi()),
      static_cast<int>(kInchInMm * bounds.height() / info.device_dpi()),
      WL_OUTPUT_SUBPIXEL_UNKNOWN, kUnknownMake, kUnknownModel,
      WL_OUTPUT_TRANSFORM_NORMAL);

  // TODO(reveman): Send correct device scale factor when surface API respects
  // scale.
  if (version >= WL_OUTPUT_SCALE_SINCE_VERSION)
    wl_output_send_scale(resource, primary.device_scale_factor());

  // TODO(reveman): Send real list of modes after adding multi-display support.
  wl_output_send_mode(resource,
                      WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED,
                      bounds.width(), bounds.height(), static_cast<int>(60000));

  if (version >= WL_OUTPUT_DONE_SINCE_VERSION)
    wl_output_send_done(resource);
}

////////////////////////////////////////////////////////////////////////////////
// xdg_surface_interface:

void xdg_surface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_surface_set_parent(wl_client* client,
                            wl_resource* resource,
                            wl_resource* parent) {
  NOTIMPLEMENTED();
}

void xdg_surface_set_title(wl_client* client,
                           wl_resource* resource,
                           const char* title) {
  GetUserDataAs<ShellSurface>(resource)
      ->SetTitle(base::string16(base::UTF8ToUTF16(title)));
}

void xdg_surface_set_add_id(wl_client* client,
                            wl_resource* resource,
                            const char* app_id) {
  GetUserDataAs<ShellSurface>(resource)->SetApplicationId(app_id);
}

void xdg_surface_show_window_menu(wl_client* client,
                                  wl_resource* resource,
                                  wl_resource* seat,
                                  uint32_t serial,
                                  int32_t x,
                                  int32_t y) {
  NOTIMPLEMENTED();
}

void xdg_surface_move(wl_client* client,
                      wl_resource* resource,
                      wl_resource* seat,
                      uint32_t serial) {
  GetUserDataAs<ShellSurface>(resource)->Move();
}

void xdg_surface_resize(wl_client* client,
                        wl_resource* resource,
                        wl_resource* seat,
                        uint32_t serial,
                        uint32_t edges) {
  NOTIMPLEMENTED();
}

void xdg_surface_ack_configure(wl_client* client,
                               wl_resource* resource,
                               uint32_t serial) {
  NOTIMPLEMENTED();
}

void xdg_surface_set_window_geometry(wl_client* client,
                                     wl_resource* resource,
                                     int32_t x,
                                     int32_t y,
                                     int32_t width,
                                     int32_t height) {
  GetUserDataAs<ShellSurface>(resource)
      ->SetGeometry(gfx::Rect(x, y, width, height));
}

void xdg_surface_set_maximized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->Maximize();
}

void xdg_surface_unset_maximized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->Restore();
}

void xdg_surface_set_fullscreen(wl_client* client,
                                wl_resource* resource,
                                wl_resource* output) {
  GetUserDataAs<ShellSurface>(resource)->SetFullscreen(true);
}

void xdg_surface_unset_fullscreen(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->SetFullscreen(false);
}

void xdg_surface_set_minimized(wl_client* client, wl_resource* resource) {
  NOTIMPLEMENTED();
}

const struct xdg_surface_interface xdg_surface_implementation = {
    xdg_surface_destroy,
    xdg_surface_set_parent,
    xdg_surface_set_title,
    xdg_surface_set_add_id,
    xdg_surface_show_window_menu,
    xdg_surface_move,
    xdg_surface_resize,
    xdg_surface_ack_configure,
    xdg_surface_set_window_geometry,
    xdg_surface_set_maximized,
    xdg_surface_unset_maximized,
    xdg_surface_set_fullscreen,
    xdg_surface_unset_fullscreen,
    xdg_surface_set_minimized};

////////////////////////////////////////////////////////////////////////////////
// xdg_shell_interface:

void xdg_shell_destroy(wl_client* client, wl_resource* resource) {
  // Nothing to do here.
}

// Currently implemented version of the unstable xdg-shell interface.
#define XDG_SHELL_VERSION 5
static_assert(XDG_SHELL_VERSION == XDG_SHELL_VERSION_CURRENT,
              "Interface version doesn't match implementation version");

void xdg_shell_use_unstable_version(wl_client* client,
                                    wl_resource* resource,
                                    int32_t version) {
  if (version > XDG_SHELL_VERSION) {
    wl_resource_post_error(resource, 1,
                           "xdg-shell version not implemented yet.");
  }
}

void HandleXdgSurfaceCloseCallback(wl_resource* resource) {
  xdg_surface_send_close(resource);
  wl_client_flush(wl_resource_get_client(resource));
}

void HandleXdgSurfaceConfigureCallback(wl_resource* resource,
                                       const gfx::Size& size,
                                       ash::wm::WindowStateType state_type,
                                       bool activated) {
  // TODO(reveman): Implement XDG_SURFACE_STATE_RESIZING.
  wl_array states;
  wl_array_init(&states);
  if (state_type == ash::wm::WINDOW_STATE_TYPE_MAXIMIZED) {
    xdg_surface_state* value = static_cast<xdg_surface_state*>(
        wl_array_add(&states, sizeof(xdg_surface_state)));
    DCHECK(value);
    *value = XDG_SURFACE_STATE_MAXIMIZED;
  }
  if (state_type == ash::wm::WINDOW_STATE_TYPE_FULLSCREEN) {
    xdg_surface_state* value = static_cast<xdg_surface_state*>(
        wl_array_add(&states, sizeof(xdg_surface_state)));
    DCHECK(value);
    *value = XDG_SURFACE_STATE_FULLSCREEN;
  }
  if (activated) {
    xdg_surface_state* value = static_cast<xdg_surface_state*>(
        wl_array_add(&states, sizeof(xdg_surface_state)));
    DCHECK(value);
    *value = XDG_SURFACE_STATE_ACTIVATED;
  }
  xdg_surface_send_configure(resource, size.width(), size.height(), &states,
                             wl_display_next_serial(wl_client_get_display(
                                 wl_resource_get_client(resource))));
  wl_client_flush(wl_resource_get_client(resource));
  wl_array_release(&states);
}

void xdg_shell_get_xdg_surface(wl_client* client,
                               wl_resource* resource,
                               uint32_t id,
                               wl_resource* surface) {
  scoped_ptr<ShellSurface> shell_surface =
      GetUserDataAs<Display>(resource)
          ->CreateShellSurface(GetUserDataAs<Surface>(surface));
  if (!shell_surface) {
    wl_resource_post_no_memory(resource);
    return;
  }

  wl_resource* xdg_surface_resource =
      wl_resource_create(client, &xdg_surface_interface, 1, id);
  if (!xdg_surface_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  shell_surface->set_close_callback(base::Bind(
      &HandleXdgSurfaceCloseCallback, base::Unretained(xdg_surface_resource)));

  shell_surface->set_configure_callback(
      base::Bind(&HandleXdgSurfaceConfigureCallback,
                 base::Unretained(xdg_surface_resource)));

  SetImplementation(xdg_surface_resource, &xdg_surface_implementation,
                    std::move(shell_surface));
}

void xdg_shell_get_xdg_popup(wl_client* client,
                             wl_resource* resource,
                             uint32_t id,
                             wl_resource* surface,
                             wl_resource* parent,
                             wl_resource* seat,
                             uint32_t serial,
                             int32_t x,
                             int32_t y) {
  NOTIMPLEMENTED();
}

void xdg_shell_pong(wl_client* client, wl_resource* resource, uint32_t serial) {
  NOTIMPLEMENTED();
}

const struct xdg_shell_interface xdg_shell_implementation = {
    xdg_shell_destroy, xdg_shell_use_unstable_version,
    xdg_shell_get_xdg_surface, xdg_shell_get_xdg_popup, xdg_shell_pong};

void bind_xdg_shell(wl_client* client,
                    void* data,
                    uint32_t version,
                    uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &xdg_shell_interface, 1, id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(resource, &xdg_shell_implementation, data,
                                 nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// wl_data_device_interface:

void data_device_start_drag(wl_client* client,
                            wl_resource* resource,
                            wl_resource* source_resource,
                            wl_resource* origin_resource,
                            wl_resource* icon_resource,
                            uint32_t serial) {
  NOTIMPLEMENTED();
}

void data_device_set_selection(wl_client* client,
                               wl_resource* resource,
                               wl_resource* data_source,
                               uint32_t serial) {
  NOTIMPLEMENTED();
}

const struct wl_data_device_interface data_device_implementation = {
    data_device_start_drag, data_device_set_selection};

////////////////////////////////////////////////////////////////////////////////
// wl_data_device_manager_interface:

void data_device_manager_create_data_source(wl_client* client,
                                            wl_resource* resource,
                                            uint32_t id) {
  NOTIMPLEMENTED();
}

void data_device_manager_get_data_device(wl_client* client,
                                         wl_resource* resource,
                                         uint32_t id,
                                         wl_resource* seat_resource) {
  wl_resource* data_device_resource =
      wl_resource_create(client, &wl_data_device_interface, 1, id);
  if (!data_device_resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(data_device_resource,
                                 &data_device_implementation, nullptr, nullptr);
}

const struct wl_data_device_manager_interface
    data_device_manager_implementation = {
        data_device_manager_create_data_source,
        data_device_manager_get_data_device};

void bind_data_device_manager(wl_client* client,
                              void* data,
                              uint32_t version,
                              uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wl_data_device_manager_interface, 1, id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &data_device_manager_implementation,
                                 data, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// wl_pointer_interface:

// Pointer delegate class that accepts events for surfaces owned by the same
// client as a pointer resource.
class WaylandPointerDelegate : public PointerDelegate {
 public:
  explicit WaylandPointerDelegate(wl_resource* pointer_resource)
      : pointer_resource_(pointer_resource) {}

  // Overridden from PointerDelegate:
  void OnPointerDestroying(Pointer* pointer) override { delete this; }
  bool CanAcceptPointerEventsForSurface(Surface* surface) const override {
    wl_resource* surface_resource = surface->GetProperty(kSurfaceResourceKey);
    // We can accept events for this surface if the client is the same as the
    // pointer.
    return surface_resource &&
           wl_resource_get_client(surface_resource) == client();
  }
  void OnPointerEnter(Surface* surface,
                      const gfx::Point& location,
                      int button_flags) override {
    wl_resource* surface_resource = surface->GetProperty(kSurfaceResourceKey);
    DCHECK(surface_resource);
    // Should we be sending button events to the client before the enter event
    // if client's pressed button state is different from |button_flags|?
    wl_pointer_send_enter(pointer_resource_, next_serial(), surface_resource,
                          wl_fixed_from_int(location.x()),
                          wl_fixed_from_int(location.y()));
    wl_client_flush(client());
  }
  void OnPointerLeave(Surface* surface) override {
    wl_resource* surface_resource = surface->GetProperty(kSurfaceResourceKey);
    DCHECK(surface_resource);
    wl_pointer_send_leave(pointer_resource_, next_serial(), surface_resource);
    wl_client_flush(client());
  }
  void OnPointerMotion(base::TimeDelta time_stamp,
                       const gfx::Point& location) override {
    wl_pointer_send_motion(pointer_resource_, time_stamp.InMilliseconds(),
                           wl_fixed_from_int(location.x()),
                           wl_fixed_from_int(location.y()));
    wl_client_flush(client());
  }
  void OnPointerButton(base::TimeDelta time_stamp,
                       int button_flags,
                       bool pressed) override {
    struct {
      ui::EventFlags flag;
      uint32_t value;
    } buttons[] = {
        {ui::EF_LEFT_MOUSE_BUTTON, BTN_LEFT},
        {ui::EF_RIGHT_MOUSE_BUTTON, BTN_RIGHT},
        {ui::EF_MIDDLE_MOUSE_BUTTON, BTN_MIDDLE},
        {ui::EF_FORWARD_MOUSE_BUTTON, BTN_FORWARD},
        {ui::EF_BACK_MOUSE_BUTTON, BTN_BACK},
    };
    uint32_t serial = next_serial();
    for (auto button : buttons) {
      if (button_flags & button.flag) {
        wl_pointer_send_button(pointer_resource_, serial,
                               time_stamp.InMilliseconds(), button.value,
                               pressed ? WL_POINTER_BUTTON_STATE_PRESSED
                                       : WL_POINTER_BUTTON_STATE_RELEASED);
      }
    }
    wl_client_flush(client());
  }
  void OnPointerWheel(base::TimeDelta time_stamp,
                      const gfx::Vector2d& offset) override {
    // Same as Weston, the reference compositor.
    const double kAxisStepDistance = 10.0 / ui::MouseWheelEvent::kWheelDelta;

    double x_value = offset.x() * kAxisStepDistance;
    if (x_value) {
      wl_pointer_send_axis(pointer_resource_, time_stamp.InMilliseconds(),
                           WL_POINTER_AXIS_HORIZONTAL_SCROLL,
                           wl_fixed_from_double(-x_value));
    }
    double y_value = offset.y() * kAxisStepDistance;
    if (y_value) {
      wl_pointer_send_axis(pointer_resource_, time_stamp.InMilliseconds(),
                           WL_POINTER_AXIS_VERTICAL_SCROLL,
                           wl_fixed_from_double(-y_value));
    }
    wl_client_flush(client());
  }

 private:
  // The client who own this pointer instance.
  wl_client* client() const {
    return wl_resource_get_client(pointer_resource_);
  }

  // Returns the next serial to use for pointer events.
  uint32_t next_serial() const {
    return wl_display_next_serial(wl_client_get_display(client()));
  }

  // The pointer resource associated with the pointer.
  wl_resource* const pointer_resource_;

  DISALLOW_COPY_AND_ASSIGN(WaylandPointerDelegate);
};

void pointer_set_cursor(wl_client* client,
                        wl_resource* resource,
                        uint32_t serial,
                        wl_resource* surface_resource,
                        int32_t hotspot_x,
                        int32_t hotspot_y) {
  GetUserDataAs<Pointer>(resource)->SetCursor(
      surface_resource ? GetUserDataAs<Surface>(surface_resource) : nullptr,
      gfx::Point(hotspot_x, hotspot_y));
}

void pointer_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_pointer_interface pointer_implementation = {pointer_set_cursor,
                                                            pointer_release};

#if defined(USE_XKBCOMMON)

////////////////////////////////////////////////////////////////////////////////
// wl_keyboard_interface:

// Keyboard delegate class that accepts events for surfaces owned by the same
// client as a keyboard resource.
class WaylandKeyboardDelegate : public KeyboardDelegate {
 public:
  explicit WaylandKeyboardDelegate(wl_resource* keyboard_resource)
      : keyboard_resource_(keyboard_resource),
        xkb_context_(xkb_context_new(XKB_CONTEXT_NO_FLAGS)),
        // TODO(reveman): Keep keymap synchronized with the keymap used by
        // chromium and the host OS.
        xkb_keymap_(xkb_keymap_new_from_names(xkb_context_.get(),
                                              nullptr,
                                              XKB_KEYMAP_COMPILE_NO_FLAGS)),
        xkb_state_(xkb_state_new(xkb_keymap_.get())) {
    scoped_ptr<char, base::FreeDeleter> keymap_string(
        xkb_keymap_get_as_string(xkb_keymap_.get(), XKB_KEYMAP_FORMAT_TEXT_V1));
    DCHECK(keymap_string.get());
    size_t keymap_size = strlen(keymap_string.get()) + 1;
    base::SharedMemory shared_keymap;
    bool rv = shared_keymap.CreateAndMapAnonymous(keymap_size);
    DCHECK(rv);
    memcpy(shared_keymap.memory(), keymap_string.get(), keymap_size);
    wl_keyboard_send_keymap(keyboard_resource_,
                            WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                            shared_keymap.handle().fd, keymap_size);
  }

  // Overridden from KeyboardDelegate:
  void OnKeyboardDestroying(Keyboard* keyboard) override { delete this; }
  bool CanAcceptKeyboardEventsForSurface(Surface* surface) const override {
    wl_resource* surface_resource = surface->GetProperty(kSurfaceResourceKey);
    // We can accept events for this surface if the client is the same as the
    // keyboard.
    return surface_resource &&
           wl_resource_get_client(surface_resource) == client();
  }
  void OnKeyboardEnter(Surface* surface,
                       const std::vector<ui::DomCode>& pressed_keys) override {
    wl_resource* surface_resource = surface->GetProperty(kSurfaceResourceKey);
    DCHECK(surface_resource);
    wl_array keys;
    wl_array_init(&keys);
    for (auto key : pressed_keys) {
      uint32_t* value =
          static_cast<uint32_t*>(wl_array_add(&keys, sizeof(uint32_t)));
      DCHECK(value);
      *value = DomCodeToKey(key);
    }
    wl_keyboard_send_enter(keyboard_resource_, next_serial(), surface_resource,
                           &keys);
    wl_array_release(&keys);
    wl_client_flush(client());
  }
  void OnKeyboardLeave(Surface* surface) override {
    wl_resource* surface_resource = surface->GetProperty(kSurfaceResourceKey);
    DCHECK(surface_resource);
    wl_keyboard_send_leave(keyboard_resource_, next_serial(), surface_resource);
    wl_client_flush(client());
  }
  void OnKeyboardKey(base::TimeDelta time_stamp,
                     ui::DomCode key,
                     bool pressed) override {
    wl_keyboard_send_key(keyboard_resource_, next_serial(),
                         time_stamp.InMilliseconds(), DomCodeToKey(key),
                         pressed ? WL_KEYBOARD_KEY_STATE_PRESSED
                                 : WL_KEYBOARD_KEY_STATE_RELEASED);
    wl_client_flush(client());
  }
  void OnKeyboardModifiers(int modifier_flags) override {
    xkb_state_update_mask(xkb_state_.get(),
                          ModifierFlagsToXkbModifiers(modifier_flags), 0, 0, 0,
                          0, 0);
    wl_keyboard_send_modifiers(
        keyboard_resource_, next_serial(),
        xkb_state_serialize_mods(xkb_state_.get(), XKB_STATE_MODS_DEPRESSED),
        xkb_state_serialize_mods(xkb_state_.get(), XKB_STATE_MODS_LOCKED),
        xkb_state_serialize_mods(xkb_state_.get(), XKB_STATE_MODS_LATCHED),
        xkb_state_serialize_layout(xkb_state_.get(),
                                   XKB_STATE_LAYOUT_EFFECTIVE));
    wl_client_flush(client());
  }

 private:
  // Returns the corresponding key given a dom code.
  uint32_t DomCodeToKey(ui::DomCode code) const {
    // This assumes KeycodeConverter has been built with evdev/xkb codes.
    xkb_keycode_t xkb_keycode = static_cast<xkb_keycode_t>(
        ui::KeycodeConverter::DomCodeToNativeKeycode(code));

    // Keycodes are offset by 8 in Xkb.
    DCHECK_GE(xkb_keycode, 8u);
    return xkb_keycode - 8;
  }

  // Returns a set of Xkb modififers given a set of modifier flags.
  uint32_t ModifierFlagsToXkbModifiers(int modifier_flags) {
    struct {
      ui::EventFlags flag;
      const char* xkb_name;
    } modifiers[] = {
        {ui::EF_SHIFT_DOWN, XKB_MOD_NAME_SHIFT},
        {ui::EF_CONTROL_DOWN, XKB_MOD_NAME_CTRL},
        {ui::EF_ALT_DOWN, XKB_MOD_NAME_ALT},
        {ui::EF_COMMAND_DOWN, XKB_MOD_NAME_LOGO},
        {ui::EF_ALTGR_DOWN, "Mod5"},
        {ui::EF_MOD3_DOWN, "Mod3"},
        {ui::EF_NUM_LOCK_ON, XKB_MOD_NAME_NUM},
        {ui::EF_CAPS_LOCK_ON, XKB_MOD_NAME_CAPS},
    };
    uint32_t xkb_modifiers = 0;
    for (auto modifier : modifiers) {
      if (modifier_flags & modifier.flag) {
        xkb_modifiers |=
            1 << xkb_keymap_mod_get_index(xkb_keymap_.get(), modifier.xkb_name);
      }
    }
    return xkb_modifiers;
  }

  // The client who own this keyboard instance.
  wl_client* client() const {
    return wl_resource_get_client(keyboard_resource_);
  }

  // Returns the next serial to use for keyboard events.
  uint32_t next_serial() const {
    return wl_display_next_serial(wl_client_get_display(client()));
  }

  // The keyboard resource associated with the keyboard.
  wl_resource* const keyboard_resource_;

  // The Xkb state used for the keyboard.
  scoped_ptr<xkb_context, ui::XkbContextDeleter> xkb_context_;
  scoped_ptr<xkb_keymap, ui::XkbKeymapDeleter> xkb_keymap_;
  scoped_ptr<xkb_state, ui::XkbStateDeleter> xkb_state_;

  DISALLOW_COPY_AND_ASSIGN(WaylandKeyboardDelegate);
};

void keyboard_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_keyboard_interface keyboard_implementation = {keyboard_release};

#endif

////////////////////////////////////////////////////////////////////////////////
// wl_touch_interface:

// Touch delegate class that accepts events for surfaces owned by the same
// client as a touch resource.
class WaylandTouchDelegate : public TouchDelegate {
 public:
  explicit WaylandTouchDelegate(wl_resource* touch_resource)
      : touch_resource_(touch_resource) {}

  // Overridden from TouchDelegate:
  void OnTouchDestroying(Touch* touch) override { delete this; }
  bool CanAcceptTouchEventsForSurface(Surface* surface) const override {
    wl_resource* surface_resource = surface->GetProperty(kSurfaceResourceKey);
    // We can accept events for this surface if the client is the same as the
    // touch resource.
    return surface_resource &&
           wl_resource_get_client(surface_resource) == client();
  }
  void OnTouchDown(Surface* surface,
                   base::TimeDelta time_stamp,
                   int id,
                   const gfx::Point& location) override {
    wl_resource* surface_resource = surface->GetProperty(kSurfaceResourceKey);
    DCHECK(surface_resource);
    wl_touch_send_down(touch_resource_, next_serial(),
                       time_stamp.InMilliseconds(), surface_resource, id,
                       wl_fixed_from_int(location.x()),
                       wl_fixed_from_int(location.y()));
    wl_client_flush(client());
  }
  void OnTouchUp(base::TimeDelta time_stamp, int id) override {
    wl_touch_send_up(touch_resource_, next_serial(),
                     time_stamp.InMilliseconds(), id);
    wl_client_flush(client());
  }
  void OnTouchMotion(base::TimeDelta time_stamp,
                     int id,
                     const gfx::Point& location) override {
    wl_touch_send_motion(touch_resource_, time_stamp.InMilliseconds(), id,
                         wl_fixed_from_int(location.x()),
                         wl_fixed_from_int(location.y()));
    wl_client_flush(client());
  }
  void OnTouchCancel() override {
    wl_touch_send_cancel(touch_resource_);
    wl_client_flush(client());
  }

 private:
  // The client who own this touch instance.
  wl_client* client() const { return wl_resource_get_client(touch_resource_); }

  // Returns the next serial to use for keyboard events.
  uint32_t next_serial() const {
    return wl_display_next_serial(wl_client_get_display(client()));
  }

  // The touch resource associated with the touch.
  wl_resource* const touch_resource_;

  DISALLOW_COPY_AND_ASSIGN(WaylandTouchDelegate);
};

void touch_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_touch_interface touch_implementation = {touch_release};

////////////////////////////////////////////////////////////////////////////////
// wl_seat_interface:

void seat_get_pointer(wl_client* client, wl_resource* resource, uint32_t id) {
  wl_resource* pointer_resource = wl_resource_create(
      client, &wl_pointer_interface, wl_resource_get_version(resource), id);
  if (!pointer_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  SetImplementation(pointer_resource, &pointer_implementation,
                    make_scoped_ptr(new Pointer(
                        new WaylandPointerDelegate(pointer_resource))));
}

void seat_get_keyboard(wl_client* client, wl_resource* resource, uint32_t id) {
#if defined(USE_XKBCOMMON)
  uint32_t version = wl_resource_get_version(resource);
  wl_resource* keyboard_resource =
      wl_resource_create(client, &wl_keyboard_interface, version, id);
  if (!keyboard_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  SetImplementation(keyboard_resource, &keyboard_implementation,
                    make_scoped_ptr(new Keyboard(
                        new WaylandKeyboardDelegate(keyboard_resource))));

  // TODO(reveman): Keep repeat info synchronized with chromium and the host OS.
  if (version >= WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION)
    wl_keyboard_send_repeat_info(keyboard_resource, 40, 500);
#else
  NOTIMPLEMENTED();
#endif
}

void seat_get_touch(wl_client* client, wl_resource* resource, uint32_t id) {
  wl_resource* touch_resource = wl_resource_create(
      client, &wl_touch_interface, wl_resource_get_version(resource), id);
  if (!touch_resource) {
    wl_resource_post_no_memory(resource);
    return;
  }

  SetImplementation(
      touch_resource, &touch_implementation,
      make_scoped_ptr(new Touch(new WaylandTouchDelegate(touch_resource))));
}

const struct wl_seat_interface seat_implementation = {
    seat_get_pointer, seat_get_keyboard, seat_get_touch};

const uint32_t seat_version = 4;

void bind_seat(wl_client* client, void* data, uint32_t version, uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &wl_seat_interface, std::min(version, seat_version), id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }

  wl_resource_set_implementation(resource, &seat_implementation, data, nullptr);

  if (version >= WL_SEAT_NAME_SINCE_VERSION)
    wl_seat_send_name(resource, "default");

  uint32_t capabilities = WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_TOUCH;
#if defined(USE_XKBCOMMON)
  capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
#endif
  wl_seat_send_capabilities(resource, capabilities);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Server, public:

Server::Server(Display* display)
    : display_(display), wl_display_(wl_display_create()) {
  wl_global_create(wl_display_.get(), &wl_compositor_interface,
                   compositor_version, display_, bind_compositor);
  wl_global_create(wl_display_.get(), &wl_shm_interface, 1, display_, bind_shm);
#if defined(USE_OZONE)
  wl_global_create(wl_display_.get(), &wl_drm_interface, drm_version, display_,
                   bind_drm);
#endif
  wl_global_create(wl_display_.get(), &wl_subcompositor_interface, 1, display_,
                   bind_subcompositor);
  wl_global_create(wl_display_.get(), &wl_shell_interface, 1, display_,
                   bind_shell);
  wl_global_create(wl_display_.get(), &wl_output_interface, output_version,
                   display_, bind_output);
  wl_global_create(wl_display_.get(), &xdg_shell_interface, 1, display_,
                   bind_xdg_shell);
  wl_global_create(wl_display_.get(), &wl_data_device_manager_interface, 1,
                   display_, bind_data_device_manager);
  wl_global_create(wl_display_.get(), &wl_seat_interface, seat_version,
                   display_, bind_seat);
}

Server::~Server() {}

// static
scoped_ptr<Server> Server::Create(Display* display) {
  scoped_ptr<Server> server(new Server(display));
  int rv = wl_display_add_socket(server->wl_display_.get(), nullptr);
  DCHECK_EQ(rv, 0) << "wl_display_add_socket failed: " << rv;
  return server;
}

bool Server::AddSocket(const std::string name) {
  DCHECK(!name.empty());
  return !wl_display_add_socket(wl_display_.get(), name.c_str());
}

int Server::GetFileDescriptor() const {
  wl_event_loop* event_loop = wl_display_get_event_loop(wl_display_.get());
  DCHECK(event_loop);
  return wl_event_loop_get_fd(event_loop);
}

void Server::Dispatch(base::TimeDelta timeout) {
  wl_event_loop* event_loop = wl_display_get_event_loop(wl_display_.get());
  DCHECK(event_loop);
  wl_event_loop_dispatch(event_loop, timeout.InMilliseconds());
}

void Server::Flush() {
  wl_display_flush_clients(wl_display_.get());
}

}  // namespace wayland
}  // namespace exo
