// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/server.h"

#include <grp.h>
#include <linux/input.h>
#include <scaler-server-protocol.h>
#include <secure-output-unstable-v1-server-protocol.h>
#include <stddef.h>
#include <stdint.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>
#include <xdg-shell-unstable-v5-server-protocol.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>

#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/free_deleter.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
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
#include "ipc/unix_domain_socket_util.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/window_property.h"
#include "ui/base/hit_test.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if defined(USE_OZONE)
#include <drm_fourcc.h>
#include <linux-dmabuf-unstable-v1-server-protocol.h>
#include <wayland-drm-server-protocol.h>
#endif

#if defined(USE_XKBCOMMON)
#include <xkbcommon/xkbcommon.h>
#include "ui/events/keycodes/scoped_xkb.h"  // nogncheck
#endif

DECLARE_WINDOW_PROPERTY_TYPE(wl_resource*);

namespace exo {
namespace wayland {
namespace {

// Default wayland socket name.
const base::FilePath::CharType kSocketName[] = FILE_PATH_LITERAL("wayland-0");

// Group used for wayland socket.
const char kWaylandSocketGroup[] = "wayland";

template <class T>
T* GetUserDataAs(wl_resource* resource) {
  return static_cast<T*>(wl_resource_get_user_data(resource));
}

template <class T>
std::unique_ptr<T> TakeUserDataAs(wl_resource* resource) {
  std::unique_ptr<T> user_data = base::WrapUnique(GetUserDataAs<T>(resource));
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
                       std::unique_ptr<T> user_data) {
  wl_resource_set_implementation(resource, implementation, user_data.release(),
                                 DestroyUserData<T>);
}

// A property key containing the surface resource that is associated with
// window. If unset, no surface resource is associated with window.
DEFINE_WINDOW_PROPERTY_KEY(wl_resource*, kSurfaceResourceKey, nullptr);

// A property key containing a boolean set to true if a viewport is associated
// with window.
DEFINE_WINDOW_PROPERTY_KEY(bool, kSurfaceHasViewportKey, false);

// A property key containing a boolean set to true if a security object is
// associated with window.
DEFINE_WINDOW_PROPERTY_KEY(bool, kSurfaceHasSecurityKey, false);

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
  DLOG_IF(WARNING, x || y) << "Unsupported buffer offset: "
                           << gfx::Point(x, y).ToString();

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

  // base::Unretained is safe as the resource owns the callback.
  std::unique_ptr<base::CancelableCallback<void(base::TimeTicks)>>
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
  std::unique_ptr<Surface> surface =
      GetUserDataAs<Display>(resource)->CreateSurface();

  wl_resource* surface_resource = wl_resource_create(
      client, &wl_surface_interface, wl_resource_get_version(resource), id);

  // Set the surface resource property for type-checking downcast support.
  surface->SetProperty(kSurfaceResourceKey, surface_resource);

  SetImplementation(surface_resource, &surface_implementation,
                    std::move(surface));
}

void compositor_create_region(wl_client* client,
                              wl_resource* resource,
                              uint32_t id) {
  wl_resource* region_resource =
      wl_resource_create(client, &wl_region_interface, 1, id);

  SetImplementation(region_resource, &region_implementation,
                    base::WrapUnique(new SkRegion));
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

  std::unique_ptr<Buffer> buffer =
      GetUserDataAs<SharedMemory>(resource)->CreateBuffer(
          gfx::Size(width, height), supported_format->buffer_format, offset,
          stride);
  if (!buffer) {
    wl_resource_post_no_memory(resource);
    return;
  }

  wl_resource* buffer_resource =
      wl_resource_create(client, &wl_buffer_interface, 1, id);

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
  std::unique_ptr<SharedMemory> shared_memory =
      GetUserDataAs<Display>(resource)->CreateSharedMemory(
          base::FileDescriptor(fd, true), size);
  if (!shared_memory) {
    wl_resource_post_no_memory(resource);
    return;
  }

  wl_resource* shm_pool_resource =
      wl_resource_create(client, &wl_shm_pool_interface, 1, id);

  SetImplementation(shm_pool_resource, &shm_pool_implementation,
                    std::move(shared_memory));
}

const struct wl_shm_interface shm_implementation = {shm_create_pool};

void bind_shm(wl_client* client, void* data, uint32_t version, uint32_t id) {
  wl_resource* resource = wl_resource_create(client, &wl_shm_interface, 1, id);

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

  std::unique_ptr<Buffer> buffer =
      GetUserDataAs<Display>(resource)->CreateLinuxDMABufBuffer(
          base::ScopedFD(name), gfx::Size(width, height),
          supported_format->buffer_format, stride0);
  if (!buffer) {
    wl_resource_post_no_memory(resource);
    return;
  }

  wl_resource* buffer_resource =
      wl_resource_create(client, &wl_buffer_interface, 1, id);

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

  wl_resource_set_implementation(resource, &drm_implementation, data, nullptr);

  if (version >= 2)
    wl_drm_send_capabilities(resource, WL_DRM_CAPABILITY_PRIME);

  for (const auto& supported_format : drm_supported_formats)
    wl_drm_send_format(resource, supported_format.drm_format);
}

////////////////////////////////////////////////////////////////////////////////
// linux_buffer_params_interface:

const struct dmabuf_supported_format {
  uint32_t dmabuf_format;
  gfx::BufferFormat buffer_format;
} dmabuf_supported_formats[] = {
    {DRM_FORMAT_XBGR8888, gfx::BufferFormat::RGBX_8888},
    {DRM_FORMAT_ABGR8888, gfx::BufferFormat::RGBA_8888},
    {DRM_FORMAT_XRGB8888, gfx::BufferFormat::BGRX_8888},
    {DRM_FORMAT_ARGB8888, gfx::BufferFormat::BGRA_8888}};

struct LinuxBufferParams {
  explicit LinuxBufferParams(Display* display)
      : display(display), stride(0), offset(0) {}

  Display* const display;
  base::ScopedFD fd;
  uint32_t stride;
  uint32_t offset;
};

void linux_buffer_params_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void linux_buffer_params_add(wl_client* client,
                             wl_resource* resource,
                             int32_t fd,
                             uint32_t plane_idx,
                             uint32_t offset,
                             uint32_t stride,
                             uint32_t modifier_hi,
                             uint32_t modifier_lo) {
  if (plane_idx) {
    wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX,
                           "plane_idx too large");
    return;
  }

  LinuxBufferParams* linux_buffer_params =
      GetUserDataAs<LinuxBufferParams>(resource);
  if (linux_buffer_params->fd.is_valid()) {
    wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_SET,
                           "plane already set");
    return;
  }

  linux_buffer_params->fd.reset(fd);
  linux_buffer_params->stride = stride;
  linux_buffer_params->offset = offset;
}

void linux_buffer_params_create(wl_client* client,
                                wl_resource* resource,
                                int32_t width,
                                int32_t height,
                                uint32_t format,
                                uint32_t flags) {
  if (width <= 0 || height <= 0) {
    wl_resource_post_error(resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_DIMENSIONS,
                           "invalid width or height");
    return;
  }

  const auto* supported_format = std::find_if(
      std::begin(dmabuf_supported_formats), std::end(dmabuf_supported_formats),
      [format](const dmabuf_supported_format& supported_format) {
        return supported_format.dmabuf_format == format;
      });
  if (supported_format == std::end(dmabuf_supported_formats)) {
    wl_resource_post_error(resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_FORMAT,
                           "format not supported");
    return;
  }

  if (flags & (ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_Y_INVERT |
               ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_INTERLACED)) {
    wl_resource_post_error(resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                           "flags not supported");
    return;
  }

  LinuxBufferParams* linux_buffer_params =
      GetUserDataAs<LinuxBufferParams>(resource);
  if (linux_buffer_params->offset) {
    wl_resource_post_error(resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                           "offset not supported");
    return;
  }

  std::unique_ptr<Buffer> buffer =
      linux_buffer_params->display->CreateLinuxDMABufBuffer(
          std::move(linux_buffer_params->fd), gfx::Size(width, height),
          supported_format->buffer_format, linux_buffer_params->stride);
  if (!buffer) {
    zwp_linux_buffer_params_v1_send_failed(resource);
    return;
  }

  wl_resource* buffer_resource =
      wl_resource_create(client, &wl_buffer_interface, 1, 0);

  buffer->set_release_callback(base::Bind(&HandleBufferReleaseCallback,
                                          base::Unretained(buffer_resource)));

  zwp_linux_buffer_params_v1_send_created(resource, buffer_resource);
}

const struct zwp_linux_buffer_params_v1_interface
    linux_buffer_params_implementation = {linux_buffer_params_destroy,
                                          linux_buffer_params_add,
                                          linux_buffer_params_create};

////////////////////////////////////////////////////////////////////////////////
// linux_dmabuf_interface:

void linux_dmabuf_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void linux_dmabuf_create_params(wl_client* client,
                                wl_resource* resource,
                                uint32_t id) {
  std::unique_ptr<LinuxBufferParams> linux_buffer_params =
      base::WrapUnique(new LinuxBufferParams(GetUserDataAs<Display>(resource)));

  wl_resource* linux_buffer_params_resource =
      wl_resource_create(client, &zwp_linux_buffer_params_v1_interface, 1, id);

  SetImplementation(linux_buffer_params_resource,
                    &linux_buffer_params_implementation,
                    std::move(linux_buffer_params));
}

const struct zwp_linux_dmabuf_v1_interface linux_dmabuf_implementation = {
    linux_dmabuf_destroy, linux_dmabuf_create_params};

void bind_linux_dmabuf(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zwp_linux_dmabuf_v1_interface, 1, id);

  wl_resource_set_implementation(resource, &linux_dmabuf_implementation, data,
                                 nullptr);

  for (const auto& supported_format : dmabuf_supported_formats)
    zwp_linux_dmabuf_v1_send_format(resource, supported_format.dmabuf_format);
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
  std::unique_ptr<SubSurface> subsurface =
      GetUserDataAs<Display>(resource)->CreateSubSurface(
          GetUserDataAs<Surface>(surface), GetUserDataAs<Surface>(parent));
  if (!subsurface) {
    wl_resource_post_error(resource, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                           "invalid surface");
    return;
  }

  wl_resource* subsurface_resource =
      wl_resource_create(client, &wl_subsurface_interface, 1, id);

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

  wl_resource_set_implementation(resource, &subcompositor_implementation, data,
                                 nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// wl_shell_surface_interface:

class ShellSurfaceSizeObserver : public views::WidgetObserver {
 public:
  ShellSurfaceSizeObserver(wl_resource* shell_surface_resource,
                           const gfx::Size& initial_size)
      : shell_surface_resource_(shell_surface_resource),
        old_size_(initial_size) {
    wl_shell_surface_send_configure(shell_surface_resource,
                                    WL_SHELL_SURFACE_RESIZE_NONE,
                                    old_size_.width(), old_size_.height());
  }

  // Overridden from view::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override { delete this; }
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override {
    if (old_size_ == new_bounds.size())
      return;

    wl_shell_surface_send_configure(shell_surface_resource_,
                                    WL_SHELL_SURFACE_RESIZE_NONE,
                                    new_bounds.width(), new_bounds.height());
    old_size_ = new_bounds.size();
  }

 private:
  wl_resource* shell_surface_resource_;
  gfx::Size old_size_;

  DISALLOW_COPY_AND_ASSIGN(ShellSurfaceSizeObserver);
};

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
  ShellSurface* shell_surface = GetUserDataAs<ShellSurface>(resource);
  if (shell_surface->enabled())
    return;

  shell_surface->SetEnabled(true);
  shell_surface->SetFullscreen(true);

  views::Widget* widget = shell_surface->GetWidget();
  widget->AddObserver(new ShellSurfaceSizeObserver(
      resource, widget->GetWindowBoundsInScreen().size()));
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
  ShellSurface* shell_surface = GetUserDataAs<ShellSurface>(resource);
  if (shell_surface->enabled())
    return;

  shell_surface->SetEnabled(true);
  shell_surface->Maximize();

  views::Widget* widget = shell_surface->GetWidget();
  widget->AddObserver(new ShellSurfaceSizeObserver(
      resource, widget->GetWindowBoundsInScreen().size()));
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

void shell_get_shell_surface(wl_client* client,
                             wl_resource* resource,
                             uint32_t id,
                             wl_resource* surface) {
  std::unique_ptr<ShellSurface> shell_surface =
      GetUserDataAs<Display>(resource)->CreateShellSurface(
          GetUserDataAs<Surface>(surface));
  if (!shell_surface) {
    wl_resource_post_error(resource, WL_SHELL_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  wl_resource* shell_surface_resource =
      wl_resource_create(client, &wl_shell_surface_interface, 1, id);

  // Shell surfaces are initially disabled and needs to be explicitly mapped
  // before they are enabled and can become visible.
  shell_surface->SetEnabled(false);

  shell_surface->set_surface_destroyed_callback(base::Bind(
      &wl_resource_destroy, base::Unretained(shell_surface_resource)));

  SetImplementation(shell_surface_resource, &shell_surface_implementation,
                    std::move(shell_surface));
}

const struct wl_shell_interface shell_implementation = {
    shell_get_shell_surface};

void bind_shell(wl_client* client, void* data, uint32_t version, uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wl_shell_interface, 1, id);

  wl_resource_set_implementation(resource, &shell_implementation, data,
                                 nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// wl_output_interface:

wl_output_transform OutputTransform(display::Display::Rotation rotation) {
  switch (rotation) {
    case display::Display::ROTATE_0:
      return WL_OUTPUT_TRANSFORM_NORMAL;
    case display::Display::ROTATE_90:
      return WL_OUTPUT_TRANSFORM_90;
    case display::Display::ROTATE_180:
      return WL_OUTPUT_TRANSFORM_180;
    case display::Display::ROTATE_270:
      return WL_OUTPUT_TRANSFORM_270;
  }
  NOTREACHED();
  return WL_OUTPUT_TRANSFORM_NORMAL;
}

class WaylandDisplayObserver : public display::DisplayObserver {
 public:
  WaylandDisplayObserver(const display::Display& display,
                         wl_resource* output_resource)
      : display_id_(display.id()), output_resource_(output_resource) {
    display::Screen::GetScreen()->AddObserver(this);
    SendDisplayMetrics(display);
  }
  ~WaylandDisplayObserver() override {
    display::Screen::GetScreen()->RemoveObserver(this);
  }

  // Overridden from display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override {}
  void OnDisplayRemoved(const display::Display& new_display) override {}
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override {
    if (display.id() != display_id_)
      return;

    if (changed_metrics &
        (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
         DISPLAY_METRIC_ROTATION)) {
      SendDisplayMetrics(display);
    }
  }

 private:
  void SendDisplayMetrics(const display::Display& display) {
    const ash::DisplayInfo& info =
        ash::Shell::GetInstance()->display_manager()->GetDisplayInfo(
            display.id());

    const float kInchInMm = 25.4f;
    const char* kUnknownMake = "unknown";
    const char* kUnknownModel = "unknown";

    gfx::Rect bounds = info.bounds_in_native();
    wl_output_send_geometry(
        output_resource_, bounds.x(), bounds.y(),
        static_cast<int>(kInchInMm * bounds.width() / info.device_dpi()),
        static_cast<int>(kInchInMm * bounds.height() / info.device_dpi()),
        WL_OUTPUT_SUBPIXEL_UNKNOWN, kUnknownMake, kUnknownModel,
        OutputTransform(display.rotation()));

    if (wl_resource_get_version(output_resource_) >=
        WL_OUTPUT_SCALE_SINCE_VERSION) {
      wl_output_send_scale(output_resource_, display.device_scale_factor());
    }

    // TODO(reveman): Send real list of modes.
    wl_output_send_mode(
        output_resource_, WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED,
        bounds.width(), bounds.height(), static_cast<int>(60000));

    if (wl_resource_get_version(output_resource_) >=
        WL_OUTPUT_DONE_SINCE_VERSION) {
      wl_output_send_done(output_resource_);
    }
  }

  // The identifier associated with the observed display.
  int64_t display_id_;

  // The output resource associated with the display.
  wl_resource* const output_resource_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDisplayObserver);
};

const uint32_t output_version = 2;

void bind_output(wl_client* client, void* data, uint32_t version, uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &wl_output_interface, std::min(version, output_version), id);

  // TODO(reveman): Multi-display support.
  const display::Display& display = ash::Shell::GetInstance()
                                        ->display_manager()
                                        ->GetPrimaryDisplayCandidate();

  SetImplementation(
      resource, nullptr,
      base::WrapUnique(new WaylandDisplayObserver(display, resource)));
}

////////////////////////////////////////////////////////////////////////////////
// xdg_surface_interface:

int XdgResizeComponent(uint32_t edges) {
  switch (edges) {
    case XDG_SURFACE_RESIZE_EDGE_TOP:
      return HTTOP;
    case XDG_SURFACE_RESIZE_EDGE_BOTTOM:
      return HTBOTTOM;
    case XDG_SURFACE_RESIZE_EDGE_LEFT:
      return HTLEFT;
    case XDG_SURFACE_RESIZE_EDGE_TOP_LEFT:
      return HTTOPLEFT;
    case XDG_SURFACE_RESIZE_EDGE_BOTTOM_LEFT:
      return HTBOTTOMLEFT;
    case XDG_SURFACE_RESIZE_EDGE_RIGHT:
      return HTRIGHT;
    case XDG_SURFACE_RESIZE_EDGE_TOP_RIGHT:
      return HTTOPRIGHT;
    case XDG_SURFACE_RESIZE_EDGE_BOTTOM_RIGHT:
      return HTBOTTOMRIGHT;
    default:
      return HTBOTTOMRIGHT;
  }
}

void xdg_surface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_surface_set_parent(wl_client* client,
                            wl_resource* resource,
                            wl_resource* parent) {
  if (!parent) {
    GetUserDataAs<ShellSurface>(resource)->SetParent(nullptr);
    return;
  }

  // This is a noop if parent has not been mapped.
  ShellSurface* shell_surface = GetUserDataAs<ShellSurface>(parent);
  if (shell_surface->GetWidget())
    GetUserDataAs<ShellSurface>(resource)->SetParent(shell_surface);
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
  int component = XdgResizeComponent(edges);
  if (component != HTNOWHERE)
    GetUserDataAs<ShellSurface>(resource)->Resize(component);
}

void xdg_surface_ack_configure(wl_client* client,
                               wl_resource* resource,
                               uint32_t serial) {
  GetUserDataAs<ShellSurface>(resource)->AcknowledgeConfigure(serial);
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
// xdg_popup_interface:

void xdg_popup_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct xdg_popup_interface xdg_popup_implementation = {xdg_popup_destroy};

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

void AddXdgSurfaceState(wl_array* states, xdg_surface_state state) {
  xdg_surface_state* value = static_cast<xdg_surface_state*>(
      wl_array_add(states, sizeof(xdg_surface_state)));
  DCHECK(value);
  *value = state;
}

uint32_t HandleXdgSurfaceConfigureCallback(wl_resource* resource,
                                           const gfx::Size& size,
                                           ash::wm::WindowStateType state_type,
                                           bool resizing,
                                           bool activated) {
  wl_array states;
  wl_array_init(&states);
  if (state_type == ash::wm::WINDOW_STATE_TYPE_MAXIMIZED)
    AddXdgSurfaceState(&states, XDG_SURFACE_STATE_MAXIMIZED);
  if (state_type == ash::wm::WINDOW_STATE_TYPE_FULLSCREEN)
    AddXdgSurfaceState(&states, XDG_SURFACE_STATE_FULLSCREEN);
  if (resizing)
    AddXdgSurfaceState(&states, XDG_SURFACE_STATE_RESIZING);
  if (activated)
    AddXdgSurfaceState(&states, XDG_SURFACE_STATE_ACTIVATED);
  uint32_t serial = wl_display_next_serial(
      wl_client_get_display(wl_resource_get_client(resource)));
  xdg_surface_send_configure(resource, size.width(), size.height(), &states,
                             serial);
  wl_client_flush(wl_resource_get_client(resource));
  wl_array_release(&states);
  return serial;
}

void xdg_shell_get_xdg_surface(wl_client* client,
                               wl_resource* resource,
                               uint32_t id,
                               wl_resource* surface) {
  std::unique_ptr<ShellSurface> shell_surface =
      GetUserDataAs<Display>(resource)->CreateShellSurface(
          GetUserDataAs<Surface>(surface));
  if (!shell_surface) {
    wl_resource_post_error(resource, XDG_SHELL_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  wl_resource* xdg_surface_resource =
      wl_resource_create(client, &xdg_surface_interface, 1, id);

  shell_surface->set_close_callback(base::Bind(
      &HandleXdgSurfaceCloseCallback, base::Unretained(xdg_surface_resource)));

  shell_surface->set_configure_callback(
      base::Bind(&HandleXdgSurfaceConfigureCallback,
                 base::Unretained(xdg_surface_resource)));

  SetImplementation(xdg_surface_resource, &xdg_surface_implementation,
                    std::move(shell_surface));
}

void HandleXdgPopupCloseCallback(wl_resource* resource) {
  xdg_popup_send_popup_done(resource);
  wl_client_flush(wl_resource_get_client(resource));
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
  // Parent widget can be found by locating the closest ancestor with a widget.
  views::Widget* parent_widget = nullptr;
  aura::Window* parent_window = GetUserDataAs<Surface>(parent);
  while (parent_window) {
    parent_widget = views::Widget::GetWidgetForNativeWindow(parent_window);
    if (parent_widget)
      break;
    parent_window = parent_window->parent();
  }

  // Early out if parent widget was not found or is not associated with a
  // shell surface.
  if (!parent_widget ||
      !ShellSurface::GetMainSurface(parent_widget->GetNativeWindow())) {
    wl_resource_post_error(resource, XDG_SHELL_ERROR_INVALID_POPUP_PARENT,
                           "invalid popup parent surface");
    return;
  }

  // TODO(reveman): Automatically close popup when clicking outside the
  // popup window.
  std::unique_ptr<ShellSurface> shell_surface =
      GetUserDataAs<Display>(resource)->CreatePopupShellSurface(
          GetUserDataAs<Surface>(surface),
          // Shell surface widget delegate implementation of GetContentsView()
          // returns a pointer to the shell surface instance.
          static_cast<ShellSurface*>(
              parent_widget->widget_delegate()->GetContentsView()),
          gfx::Point(x, y));
  if (!shell_surface) {
    wl_resource_post_error(resource, XDG_SHELL_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  wl_resource* xdg_popup_resource =
      wl_resource_create(client, &xdg_popup_interface, 1, id);

  shell_surface->set_close_callback(base::Bind(
      &HandleXdgPopupCloseCallback, base::Unretained(xdg_popup_resource)));

  SetImplementation(xdg_popup_resource, &xdg_popup_implementation,
                    std::move(shell_surface));
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
  }
  void OnPointerLeave(Surface* surface) override {
    wl_resource* surface_resource = surface->GetProperty(kSurfaceResourceKey);
    DCHECK(surface_resource);
    wl_pointer_send_leave(pointer_resource_, next_serial(), surface_resource);
  }
  void OnPointerMotion(base::TimeDelta time_stamp,
                       const gfx::Point& location) override {
    wl_pointer_send_motion(pointer_resource_, time_stamp.InMilliseconds(),
                           wl_fixed_from_int(location.x()),
                           wl_fixed_from_int(location.y()));
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
  }

  void OnPointerScroll(base::TimeDelta time_stamp,
                       const gfx::Vector2dF& offset,
                       bool discrete) override {
    // Same as Weston, the reference compositor.
    const double kAxisStepDistance = 10.0 / ui::MouseWheelEvent::kWheelDelta;

    if (wl_resource_get_version(pointer_resource_) >=
        WL_POINTER_AXIS_SOURCE_SINCE_VERSION) {
      int32_t axis_source = discrete ? WL_POINTER_AXIS_SOURCE_WHEEL
                                     : WL_POINTER_AXIS_SOURCE_FINGER;
      wl_pointer_send_axis_source(pointer_resource_, axis_source);
    }

    double x_value = offset.x() * kAxisStepDistance;
    wl_pointer_send_axis(pointer_resource_, time_stamp.InMilliseconds(),
                         WL_POINTER_AXIS_HORIZONTAL_SCROLL,
                         wl_fixed_from_double(-x_value));

    double y_value = offset.y() * kAxisStepDistance;
    wl_pointer_send_axis(pointer_resource_, time_stamp.InMilliseconds(),
                         WL_POINTER_AXIS_VERTICAL_SCROLL,
                         wl_fixed_from_double(-y_value));
  }

  void OnPointerScrollCancel(base::TimeDelta time_stamp) override {
    // Wayland doesn't know the concept of a canceling kinetic scrolling.
    // But we can send a 0 distance scroll to emulate this behavior.
    OnPointerScroll(time_stamp, gfx::Vector2dF(0, 0), false);
    OnPointerScrollStop(time_stamp);
  }

  void OnPointerScrollStop(base::TimeDelta time_stamp) override {
    if (wl_resource_get_version(pointer_resource_) >=
        WL_POINTER_AXIS_STOP_SINCE_VERSION) {
      wl_pointer_send_axis_stop(pointer_resource_, time_stamp.InMilliseconds(),
                                WL_POINTER_AXIS_HORIZONTAL_SCROLL);
      wl_pointer_send_axis_stop(pointer_resource_, time_stamp.InMilliseconds(),
                                WL_POINTER_AXIS_VERTICAL_SCROLL);
    }
  }

  void OnPointerFrame() override {
    if (wl_resource_get_version(pointer_resource_) >=
        WL_POINTER_FRAME_SINCE_VERSION) {
      wl_pointer_send_frame(pointer_resource_);
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
    std::unique_ptr<char, base::FreeDeleter> keymap_string(
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
  std::unique_ptr<xkb_context, ui::XkbContextDeleter> xkb_context_;
  std::unique_ptr<xkb_keymap, ui::XkbKeymapDeleter> xkb_keymap_;
  std::unique_ptr<xkb_state, ui::XkbStateDeleter> xkb_state_;

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

  SetImplementation(pointer_resource, &pointer_implementation,
                    base::WrapUnique(new Pointer(
                        new WaylandPointerDelegate(pointer_resource))));
}

void seat_get_keyboard(wl_client* client, wl_resource* resource, uint32_t id) {
#if defined(USE_XKBCOMMON)
  uint32_t version = wl_resource_get_version(resource);
  wl_resource* keyboard_resource =
      wl_resource_create(client, &wl_keyboard_interface, version, id);

  SetImplementation(keyboard_resource, &keyboard_implementation,
                    base::WrapUnique(new Keyboard(
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

  SetImplementation(
      touch_resource, &touch_implementation,
      base::WrapUnique(new Touch(new WaylandTouchDelegate(touch_resource))));
}

void seat_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_seat_interface seat_implementation = {
    seat_get_pointer, seat_get_keyboard, seat_get_touch, seat_release};

const uint32_t seat_version = 5;

void bind_seat(wl_client* client, void* data, uint32_t version, uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &wl_seat_interface, std::min(version, seat_version), id);

  wl_resource_set_implementation(resource, &seat_implementation, data, nullptr);

  if (version >= WL_SEAT_NAME_SINCE_VERSION)
    wl_seat_send_name(resource, "default");

  uint32_t capabilities = WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_TOUCH;
#if defined(USE_XKBCOMMON)
  capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
#endif
  wl_seat_send_capabilities(resource, capabilities);
}

////////////////////////////////////////////////////////////////////////////////
// wl_viewport_interface:

class Viewport : public SurfaceObserver {
 public:
  explicit Viewport(Surface* surface) : surface_(surface) {
    surface_->AddSurfaceObserver(this);
    surface_->SetProperty(kSurfaceHasViewportKey, true);
  }
  ~Viewport() override {
    if (surface_) {
      surface_->RemoveSurfaceObserver(this);
      surface_->SetViewport(gfx::Size());
      surface_->SetProperty(kSurfaceHasViewportKey, false);
    }
  }

  void SetDestination(const gfx::Size& size) {
    if (surface_)
      surface_->SetViewport(size);
  }

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override {
    surface->RemoveSurfaceObserver(this);
    surface_ = nullptr;
  }

 private:
  Surface* surface_;

  DISALLOW_COPY_AND_ASSIGN(Viewport);
};

void viewport_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void viewport_set(wl_client* client,
                  wl_resource* resource,
                  wl_fixed_t src_x,
                  wl_fixed_t src_y,
                  wl_fixed_t src_width,
                  wl_fixed_t src_height,
                  int32_t dst_width,
                  int32_t dst_height) {
  NOTIMPLEMENTED();
}

void viewport_set_source(wl_client* client,
                         wl_resource* resource,
                         wl_fixed_t x,
                         wl_fixed_t y,
                         wl_fixed_t width,
                         wl_fixed_t height) {
  NOTIMPLEMENTED();
}

void viewport_set_destination(wl_client* client,
                              wl_resource* resource,
                              int32_t width,
                              int32_t height) {
  if (width == -1 && height == -1) {
    GetUserDataAs<Viewport>(resource)->SetDestination(gfx::Size());
    return;
  }

  if (width <= 0 || height <= 0) {
    wl_resource_post_error(resource, WL_VIEWPORT_ERROR_BAD_VALUE,
                           "destination size must be positive (%dx%d)", width,
                           height);
    return;
  }

  GetUserDataAs<Viewport>(resource)->SetDestination(gfx::Size(width, height));
}

const struct wl_viewport_interface viewport_implementation = {
    viewport_destroy, viewport_set, viewport_set_source,
    viewport_set_destination};

////////////////////////////////////////////////////////////////////////////////
// wl_scaler_interface:

void scaler_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void scaler_get_viewport(wl_client* client,
                         wl_resource* resource,
                         uint32_t id,
                         wl_resource* surface_resource) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  if (surface->GetProperty(kSurfaceHasViewportKey)) {
    wl_resource_post_error(resource, WL_SCALER_ERROR_VIEWPORT_EXISTS,
                           "a viewport for that surface already exists");
    return;
  }

  wl_resource* viewport_resource = wl_resource_create(
      client, &wl_viewport_interface, wl_resource_get_version(resource), id);

  SetImplementation(viewport_resource, &viewport_implementation,
                    base::WrapUnique(new Viewport(surface)));
}

const struct wl_scaler_interface scaler_implementation = {scaler_destroy,
                                                          scaler_get_viewport};

const uint32_t scaler_version = 2;

void bind_scaler(wl_client* client, void* data, uint32_t version, uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &wl_scaler_interface, std::min(version, scaler_version), id);

  wl_resource_set_implementation(resource, &scaler_implementation, data,
                                 nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// security_interface:

class Security : public SurfaceObserver {
 public:
  explicit Security(Surface* surface) : surface_(surface) {
    surface_->AddSurfaceObserver(this);
    surface_->SetProperty(kSurfaceHasSecurityKey, true);
  }
  ~Security() override {
    if (surface_) {
      surface_->RemoveSurfaceObserver(this);
      surface_->SetOnlyVisibleOnSecureOutput(false);
      surface_->SetProperty(kSurfaceHasSecurityKey, false);
    }
  }

  void OnlyVisibleOnSecureOutput() {
    if (surface_)
      surface_->SetOnlyVisibleOnSecureOutput(true);
  }

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override {
    surface->RemoveSurfaceObserver(this);
    surface_ = nullptr;
  }

 private:
  Surface* surface_;

  DISALLOW_COPY_AND_ASSIGN(Security);
};

void security_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void security_only_visible_on_secure_output(wl_client* client,
                                            wl_resource* resource) {
  GetUserDataAs<Security>(resource)->OnlyVisibleOnSecureOutput();
}

const struct zwp_security_v1_interface security_implementation = {
    security_destroy, security_only_visible_on_secure_output};

////////////////////////////////////////////////////////////////////////////////
// secure_output_interface:

void secure_output_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void secure_output_get_security(wl_client* client,
                                wl_resource* resource,
                                uint32_t id,
                                wl_resource* surface_resource) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  if (surface->GetProperty(kSurfaceHasSecurityKey)) {
    wl_resource_post_error(resource, ZWP_SECURE_OUTPUT_V1_ERROR_SECURITY_EXISTS,
                           "a security object for that surface already exists");
    return;
  }

  wl_resource* security_resource =
      wl_resource_create(client, &zwp_security_v1_interface, 1, id);

  SetImplementation(security_resource, &security_implementation,
                    base::WrapUnique(new Security(surface)));
}

const struct zwp_secure_output_v1_interface secure_output_implementation = {
    secure_output_destroy, secure_output_get_security};

void bind_secure_output(wl_client* client,
                        void* data,
                        uint32_t version,
                        uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zwp_secure_output_v1_interface, 1, id);

  wl_resource_set_implementation(resource, &secure_output_implementation, data,
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
  wl_global_create(wl_display_.get(), &zwp_linux_dmabuf_v1_interface, 1,
                   display_, bind_linux_dmabuf);
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
  wl_global_create(wl_display_.get(), &wl_scaler_interface, scaler_version,
                   display_, bind_scaler);
  wl_global_create(wl_display_.get(), &zwp_secure_output_v1_interface, 1,
                   display_, bind_secure_output);
}

Server::~Server() {}

// static
std::unique_ptr<Server> Server::Create(Display* display) {
  std::unique_ptr<Server> server(new Server(display));

  char* runtime_dir = getenv("XDG_RUNTIME_DIR");
  if (!runtime_dir) {
    LOG(ERROR) << "XDG_RUNTIME_DIR not set in the environment";
    return nullptr;
  }

  if (!server->AddSocket(kSocketName)) {
    LOG(ERROR) << "Failed to add socket: " << kSocketName;
    return nullptr;
  }

  base::FilePath socket_path = base::FilePath(runtime_dir).Append(kSocketName);

  // Change permissions on the socket.
  struct group wayland_group;
  struct group* wayland_group_res = nullptr;
  char buf[10000];
  if (HANDLE_EINTR(getgrnam_r(kWaylandSocketGroup, &wayland_group, buf,
                              sizeof(buf), &wayland_group_res)) < 0) {
    PLOG(ERROR) << "getgrnam_r";
    return nullptr;
  }
  if (wayland_group_res) {
    if (HANDLE_EINTR(chown(socket_path.MaybeAsASCII().c_str(), -1,
                           wayland_group.gr_gid)) < 0) {
      PLOG(ERROR) << "chown";
      return nullptr;
    }
  } else {
    LOG(WARNING) << "Group '" << kWaylandSocketGroup << "' not found";
  }

  if (!base::SetPosixFilePermissions(socket_path, 0660)) {
    PLOG(ERROR) << "Could not set permissions: " << socket_path.value();
    return nullptr;
  }

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
