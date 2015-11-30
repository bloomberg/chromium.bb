// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/server.h"

#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include <algorithm>

#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/buffer.h"
#include "components/exo/display.h"
#include "components/exo/shared_memory.h"
#include "components/exo/shell_surface.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "third_party/skia/include/core/SkRegion.h"

#if defined(USE_OZONE)
#include <wayland-drm-server-protocol.h>
#endif

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
  return user_data.Pass();
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

////////////////////////////////////////////////////////////////////////////////
// wl_buffer_interface:

void buffer_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_buffer_interface buffer_implementation = {buffer_destroy};

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

void handle_surface_frame_callback(wl_resource* resource,
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
  cancelable_callback(new base::CancelableCallback<void(base::TimeTicks)>(
      base::Bind(&handle_surface_frame_callback,
                 base::Unretained(callback_resource))));

  GetUserDataAs<Surface>(resource)
      ->RequestFrameCallback(cancelable_callback->callback());

  SetImplementation(callback_resource, nullptr, cancelable_callback.Pass());
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
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
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

  SetImplementation(surface_resource, &surface_implementation, surface.Pass());
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

  SetImplementation(region_resource, &region_implementation, region.Pass());
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

  buffer->set_release_callback(
      base::Bind(&wl_buffer_send_release, base::Unretained(buffer_resource)));

  SetImplementation(buffer_resource, &buffer_implementation, buffer.Pass());
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
                    shared_memory.Pass());
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

////////////////////////////////////////////////////////////////////////////////
// wl_drm_interface:

#if defined(USE_OZONE)
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

  buffer->set_release_callback(
      base::Bind(&wl_buffer_send_release, base::Unretained(buffer_resource)));

  SetImplementation(buffer_resource, &buffer_implementation, buffer.Pass());
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
                    subsurface.Pass());
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
  NOTIMPLEMENTED();
}

void shell_surface_resize(wl_client* client,
                          wl_resource* resource,
                          wl_resource* seat_resource,
                          uint32_t serial,
                          uint32_t edges) {
  NOTIMPLEMENTED();
}

void shell_surface_set_toplevel(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->SetToplevel();
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
  GetUserDataAs<ShellSurface>(resource)->SetFullscreen();
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
  GetUserDataAs<ShellSurface>(resource)->SetMaximized();
}

void shell_surface_set_title(wl_client* client,
                             wl_resource* resource,
                             const char* title) {
  GetUserDataAs<ShellSurface>(resource)
      ->SetTitle(base::string16(base::ASCIIToUTF16(title)));
}

void shell_surface_set_class(wl_client* client,
                             wl_resource* resource,
                             const char* clazz) {
  NOTIMPLEMENTED();
}

const struct wl_shell_surface_interface shell_surface_implementation = {
    shell_surface_pong,          shell_surface_move,
    shell_surface_resize,        shell_surface_set_toplevel,
    shell_surface_set_transient, shell_surface_set_fullscreen,
    shell_surface_set_popup,     shell_surface_set_maximized,
    shell_surface_set_title,     shell_surface_set_class};

////////////////////////////////////////////////////////////////////////////////
// wl_shell_interface:

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

  SetImplementation(shell_surface_resource, &shell_surface_implementation,
                    shell_surface.Pass());
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
