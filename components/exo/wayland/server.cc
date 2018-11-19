// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/server.h"

#include <alpha-compositing-unstable-v1-server-protocol.h>
#include <aura-shell-server-protocol.h>
#include <cursor-shapes-unstable-v1-server-protocol.h>
#include <gaming-input-unstable-v1-server-protocol.h>
#include <gaming-input-unstable-v2-server-protocol.h>
#include <grp.h>
#include <input-timestamps-unstable-v1-server-protocol.h>
#include <keyboard-configuration-unstable-v1-server-protocol.h>
#include <keyboard-extension-unstable-v1-server-protocol.h>
#include <linux/input.h>
#include <notification-shell-unstable-v1-server-protocol.h>
#include <pointer-gestures-unstable-v1-server-protocol.h>
#include <presentation-time-server-protocol.h>
#include <remote-shell-unstable-v1-server-protocol.h>
#include <secure-output-unstable-v1-server-protocol.h>
#include <stddef.h>
#include <stdint.h>
#include <stylus-tools-unstable-v1-server-protocol.h>
#include <stylus-unstable-v2-server-protocol.h>
#include <text-input-unstable-v1-server-protocol.h>
#include <viewporter-server-protocol.h>
#include <vsync-feedback-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>
#include <xdg-shell-unstable-v6-server-protocol.h>

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/display/screen_orientation_controller.h"
#include "ash/ime/ime_controller.h"
#include "ash/public/cpp/caption_buttons/caption_button_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/interfaces/window_pin_type.mojom.h"
#include "ash/public/interfaces/window_state_type.mojom.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_resizer.h"
#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/free_deleter.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/exo/buffer.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/data_device.h"
#include "components/exo/data_device_delegate.h"
#include "components/exo/data_offer.h"
#include "components/exo/data_offer_delegate.h"
#include "components/exo/data_source.h"
#include "components/exo/data_source_delegate.h"
#include "components/exo/display.h"
#include "components/exo/gamepad_delegate.h"
#include "components/exo/gaming_seat.h"
#include "components/exo/gaming_seat_delegate.h"
#include "components/exo/input_method_surface.h"
#include "components/exo/keyboard.h"
#include "components/exo/keyboard_delegate.h"
#include "components/exo/keyboard_device_configuration_delegate.h"
#include "components/exo/keyboard_observer.h"
#include "components/exo/notification.h"
#include "components/exo/notification_surface.h"
#include "components/exo/notification_surface_manager.h"
#include "components/exo/pointer.h"
#include "components/exo/pointer_delegate.h"
#include "components/exo/pointer_gesture_pinch_delegate.h"
#include "components/exo/shared_memory.h"
#include "components/exo/shell_surface.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/text_input.h"
#include "components/exo/touch.h"
#include "components/exo/touch_delegate.h"
#include "components/exo/touch_stylus_delegate.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wayland_input_delegate.h"
#include "components/exo/wayland/wayland_keyboard_delegate.h"
#include "components/exo/wayland/wayland_pointer_delegate.h"
#include "components/exo/wayland/wayland_touch_delegate.h"
#include "components/exo/wayland/wl_seat.h"
#include "components/exo/wayland/wl_shell.h"
#include "components/exo/wayland/zcr_cursor_shapes.h"
#include "components/exo/wayland/zcr_gaming_input.h"
#include "components/exo/wayland/zcr_keyboard_configuration.h"
#include "components/exo/wayland/zcr_keyboard_extension.h"
#include "components/exo/wayland/zcr_notification_shell.h"
#include "components/exo/wayland/zcr_remote_shell.h"
#include "components/exo/wayland/zcr_stylus_tools.h"
#include "components/exo/wayland/zwp_input_timestamps_manager.h"
#include "components/exo/wayland/zwp_pointer_gestures.h"
#include "components/exo/wayland/zwp_text_input_manager.h"
#include "components/exo/wayland/zxdg_shell.h"
#include "components/exo/wm_helper.h"
#include "components/exo/wm_helper_chromeos.h"
#include "components/exo/xdg_shell_surface.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_features.h"
#include "ui/compositor/compositor_vsync_manager.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_util.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/public/activation_change_observer.h"

#if defined(USE_OZONE)
#include <drm_fourcc.h>
#include <linux-dmabuf-unstable-v1-server-protocol.h>
#if defined(OS_CHROMEOS)
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#endif
#endif

#if defined(USE_FULLSCREEN_SHELL)
#include <fullscreen-shell-unstable-v1-server-protocol.h>
#include "components/exo/wayland/zwp_fullscreen_shell.h"
#endif

#if BUILDFLAG(USE_XKBCOMMON)
#include <xkbcommon/xkbcommon.h>
#include "ui/events/keycodes/scoped_xkb.h"  // nogncheck
#endif

DEFINE_UI_CLASS_PROPERTY_TYPE(wl_resource*);

namespace exo {
namespace wayland {
namespace switches {

// This flag can be used to override the default wayland socket name. It is
// useful when another wayland server is already running and using the
// default name.
constexpr char kWaylandServerSocket[] = "wayland-server-socket";
}

namespace {

// Default wayland socket name.
const base::FilePath::CharType kSocketName[] = FILE_PATH_LITERAL("wayland-0");

// Group used for wayland socket.
const char kWaylandSocketGroup[] = "wayland";

uint32_t WaylandDataDeviceManagerDndAction(DndAction action) {
  switch (action) {
    case DndAction::kNone:
      return WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
    case DndAction::kCopy:
      return WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
    case DndAction::kMove:
      return WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
    case DndAction::kAsk:
      return WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;
  }
  NOTREACHED();
}

uint32_t WaylandDataDeviceManagerDndActions(
    const base::flat_set<DndAction>& dnd_actions) {
  uint32_t actions = 0;
  for (DndAction action : dnd_actions)
    actions |= WaylandDataDeviceManagerDndAction(action);
  return actions;
}

DndAction DataDeviceManagerDndAction(uint32_t value) {
  switch (value) {
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE:
      return DndAction::kNone;
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY:
      return DndAction::kCopy;
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE:
      return DndAction::kMove;
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK:
      return DndAction::kAsk;
    default:
      NOTREACHED();
      return DndAction::kNone;
  }
}

base::flat_set<DndAction> DataDeviceManagerDndActions(uint32_t value) {
  base::flat_set<DndAction> actions;
  if (value & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
    actions.insert(DndAction::kCopy);
  if (value & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
    actions.insert(DndAction::kMove);
  if (value & WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK)
    actions.insert(DndAction::kAsk);
  return actions;
}

// A property key containing a boolean set to true if a viewport is associated
// with with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasViewportKey, false);

// A property key containing a boolean set to true if a security object is
// associated with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasSecurityKey, false);

// A property key containing a boolean set to true if a blending object is
// associated with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasBlendingKey, false);

// A property key containing a boolean set to true if na aura surface object is
// associated with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasAuraSurfaceKey, false);

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
    wl_callback_send_done(resource, TimeTicksToMilliseconds(frame_time));
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
  auto cancelable_callback =
      std::make_unique<base::CancelableCallback<void(base::TimeTicks)>>(
          base::Bind(&HandleSurfaceFrameCallback,
                     base::Unretained(callback_resource)));

  GetUserDataAs<Surface>(resource)
      ->RequestFrameCallback(cancelable_callback->callback());

  SetImplementation(callback_resource, nullptr, std::move(cancelable_callback));
}

void surface_set_opaque_region(wl_client* client,
                               wl_resource* resource,
                               wl_resource* region_resource) {
  SkRegion region = region_resource ? *GetUserDataAs<SkRegion>(region_resource)
                                    : SkRegion(SkIRect::MakeEmpty());
  GetUserDataAs<Surface>(resource)->SetOpaqueRegion(cc::Region(region));
}

void surface_set_input_region(wl_client* client,
                              wl_resource* resource,
                              wl_resource* region_resource) {
  Surface* surface = GetUserDataAs<Surface>(resource);
  if (region_resource) {
    surface->SetInputRegion(
        cc::Region(*GetUserDataAs<SkRegion>(region_resource)));
  } else
    surface->ResetInputRegion();
}

void surface_commit(wl_client* client, wl_resource* resource) {
  GetUserDataAs<Surface>(resource)->Commit();
}

void surface_set_buffer_transform(wl_client* client,
                                  wl_resource* resource,
                                  int32_t transform) {
  Transform buffer_transform;
  switch (transform) {
    case WL_OUTPUT_TRANSFORM_NORMAL:
      buffer_transform = Transform::NORMAL;
      break;
    case WL_OUTPUT_TRANSFORM_90:
      buffer_transform = Transform::ROTATE_90;
      break;
    case WL_OUTPUT_TRANSFORM_180:
      buffer_transform = Transform::ROTATE_180;
      break;
    case WL_OUTPUT_TRANSFORM_270:
      buffer_transform = Transform::ROTATE_270;
      break;
    case WL_OUTPUT_TRANSFORM_FLIPPED:
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
      NOTIMPLEMENTED();
      return;
    default:
      wl_resource_post_error(resource, WL_SURFACE_ERROR_INVALID_TRANSFORM,
                             "buffer transform must be one of the values from "
                             "the wl_output.transform enum ('%d' specified)",
                             transform);
      return;
  }

  GetUserDataAs<Surface>(resource)->SetBufferTransform(buffer_transform);
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
  SetSurfaceResource(surface.get(), surface_resource);

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
  static const auto kMode =
      base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe;
  auto fd_pair = base::subtle::ScopedFDPair(base::ScopedFD(fd),
                                            base::ScopedFD() /* readonly_fd */);
  auto guid = base::UnguessableToken::Create();
  auto platform_shared_memory = base::subtle::PlatformSharedMemoryRegion::Take(
      std::move(fd_pair), kMode, size, guid);
  std::unique_ptr<SharedMemory> shared_memory =
      GetUserDataAs<Display>(resource)->CreateSharedMemory(
          base::UnsafeSharedMemoryRegion::Deserialize(
              std::move(platform_shared_memory)));
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
// linux_buffer_params_interface:

const struct dmabuf_supported_format {
  uint32_t dmabuf_format;
  gfx::BufferFormat buffer_format;
} dmabuf_supported_formats[] = {
    {DRM_FORMAT_RGB565, gfx::BufferFormat::BGR_565},
    {DRM_FORMAT_XBGR8888, gfx::BufferFormat::RGBX_8888},
    {DRM_FORMAT_ABGR8888, gfx::BufferFormat::RGBA_8888},
    {DRM_FORMAT_XRGB8888, gfx::BufferFormat::BGRX_8888},
    {DRM_FORMAT_ARGB8888, gfx::BufferFormat::BGRA_8888},
    {DRM_FORMAT_NV12, gfx::BufferFormat::YUV_420_BIPLANAR},
    {DRM_FORMAT_YVU420, gfx::BufferFormat::YVU_420}};

struct LinuxBufferParams {
  struct Plane {
    base::ScopedFD fd;
    uint32_t stride;
    uint32_t offset;
  };

  explicit LinuxBufferParams(Display* display) : display(display) {}

  Display* const display;
  std::map<uint32_t, Plane> planes;
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
  LinuxBufferParams* linux_buffer_params =
      GetUserDataAs<LinuxBufferParams>(resource);

  LinuxBufferParams::Plane plane{base::ScopedFD(fd), stride, offset};

  const auto& inserted = linux_buffer_params->planes.insert(
      std::pair<uint32_t, LinuxBufferParams::Plane>(plane_idx,
                                                    std::move(plane)));
  if (!inserted.second) {  // The plane was already there.
    wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_SET,
                           "plane already set");
  }
}

bool ValidateLinuxBufferParams(wl_resource* resource,
                               int32_t width,
                               int32_t height,
                               gfx::BufferFormat format,
                               uint32_t flags) {
  if (width <= 0 || height <= 0) {
    wl_resource_post_error(resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_DIMENSIONS,
                           "invalid width or height");
    return false;
  }

  if (flags & ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_INTERLACED) {
    wl_resource_post_error(resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                           "flags not supported");
    return false;
  }

  LinuxBufferParams* linux_buffer_params =
      GetUserDataAs<LinuxBufferParams>(resource);
  size_t num_planes = gfx::NumberOfPlanesForBufferFormat(format);

  for (uint32_t i = 0; i < num_planes; ++i) {
    auto plane_it = linux_buffer_params->planes.find(i);
    if (plane_it == linux_buffer_params->planes.end()) {
      wl_resource_post_error(resource,
                             ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                             "missing a plane");
      return false;
    }
  }

  if (linux_buffer_params->planes.size() != num_planes) {
    wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX,
                           "plane idx out of bounds");
    return false;
  }

  return true;
}

void linux_buffer_params_create(wl_client* client,
                                wl_resource* resource,
                                int32_t width,
                                int32_t height,
                                uint32_t format,
                                uint32_t flags) {
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

  if (!ValidateLinuxBufferParams(resource, width, height,
                                 supported_format->buffer_format, flags))
    return;

  LinuxBufferParams* linux_buffer_params =
      GetUserDataAs<LinuxBufferParams>(resource);

  size_t num_planes =
      gfx::NumberOfPlanesForBufferFormat(supported_format->buffer_format);

  std::vector<gfx::NativePixmapPlane> planes;
  std::vector<base::ScopedFD> fds;

  for (uint32_t i = 0; i < num_planes; ++i) {
    auto plane_it = linux_buffer_params->planes.find(i);
    LinuxBufferParams::Plane& plane = plane_it->second;
    planes.emplace_back(plane.stride, plane.offset, 0);
    fds.push_back(std::move(plane.fd));
  }

  bool y_invert = (flags & ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_Y_INVERT) != 0;

  std::unique_ptr<Buffer> buffer =
      linux_buffer_params->display->CreateLinuxDMABufBuffer(
          gfx::Size(width, height), supported_format->buffer_format, planes,
          y_invert, std::move(fds));
  if (!buffer) {
    zwp_linux_buffer_params_v1_send_failed(resource);
    return;
  }

  wl_resource* buffer_resource =
      wl_resource_create(client, &wl_buffer_interface, 1, 0);

  buffer->set_release_callback(base::Bind(&HandleBufferReleaseCallback,
                                          base::Unretained(buffer_resource)));

  SetImplementation(buffer_resource, &buffer_implementation, std::move(buffer));

  zwp_linux_buffer_params_v1_send_created(resource, buffer_resource);
}

void linux_buffer_params_create_immed(wl_client* client,
                                      wl_resource* resource,
                                      uint32_t buffer_id,
                                      int32_t width,
                                      int32_t height,
                                      uint32_t format,
                                      uint32_t flags) {
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

  if (!ValidateLinuxBufferParams(resource, width, height,
                                 supported_format->buffer_format, flags))
    return;

  LinuxBufferParams* linux_buffer_params =
      GetUserDataAs<LinuxBufferParams>(resource);

  size_t num_planes =
      gfx::NumberOfPlanesForBufferFormat(supported_format->buffer_format);

  std::vector<gfx::NativePixmapPlane> planes;
  std::vector<base::ScopedFD> fds;

  for (uint32_t i = 0; i < num_planes; ++i) {
    auto plane_it = linux_buffer_params->planes.find(i);
    LinuxBufferParams::Plane& plane = plane_it->second;
    planes.emplace_back(plane.stride, plane.offset, 0);
    fds.push_back(std::move(plane.fd));
  }

  bool y_invert = flags & ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_Y_INVERT;

  std::unique_ptr<Buffer> buffer =
      linux_buffer_params->display->CreateLinuxDMABufBuffer(
          gfx::Size(width, height), supported_format->buffer_format, planes,
          y_invert, std::move(fds));
  if (!buffer) {
    // On import failure in case of a create_immed request, the protocol
    // allows us to raise a fatal error from zwp_linux_dmabuf_v1 version 2+.
    wl_resource_post_error(resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_WL_BUFFER,
                           "dmabuf import failed");
    return;
  }

  wl_resource* buffer_resource =
      wl_resource_create(client, &wl_buffer_interface, 1, buffer_id);

  buffer->set_release_callback(base::Bind(&HandleBufferReleaseCallback,
                                          base::Unretained(buffer_resource)));

  SetImplementation(buffer_resource, &buffer_implementation, std::move(buffer));
}

const struct zwp_linux_buffer_params_v1_interface
    linux_buffer_params_implementation = {
        linux_buffer_params_destroy, linux_buffer_params_add,
        linux_buffer_params_create, linux_buffer_params_create_immed};

////////////////////////////////////////////////////////////////////////////////
// linux_dmabuf_interface:

void linux_dmabuf_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void linux_dmabuf_create_params(wl_client* client,
                                wl_resource* resource,
                                uint32_t id) {
  std::unique_ptr<LinuxBufferParams> linux_buffer_params =
      std::make_unique<LinuxBufferParams>(GetUserDataAs<Display>(resource));

  wl_resource* linux_buffer_params_resource =
      wl_resource_create(client, &zwp_linux_buffer_params_v1_interface,
                         wl_resource_get_version(resource), id);

  SetImplementation(linux_buffer_params_resource,
                    &linux_buffer_params_implementation,
                    std::move(linux_buffer_params));
}

const struct zwp_linux_dmabuf_v1_interface linux_dmabuf_implementation = {
    linux_dmabuf_destroy, linux_dmabuf_create_params};

const uint32_t linux_dmabuf_version = 2;

void bind_linux_dmabuf(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zwp_linux_dmabuf_v1_interface,
                         std::min(version, linux_dmabuf_version), id);

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
// wl_output_interface:

// Returns the transform that a compositor will apply to a surface to
// compensate for the rotation of an output device.
wl_output_transform OutputTransform(display::Display::Rotation rotation) {
  // Note: |rotation| describes the counter clockwise rotation that a
  // display's output is currently adjusted for, which is the inverse
  // of what we need to return.
  switch (rotation) {
    case display::Display::ROTATE_0:
      return WL_OUTPUT_TRANSFORM_NORMAL;
    case display::Display::ROTATE_90:
      return WL_OUTPUT_TRANSFORM_270;
    case display::Display::ROTATE_180:
      return WL_OUTPUT_TRANSFORM_180;
    case display::Display::ROTATE_270:
      return WL_OUTPUT_TRANSFORM_90;
  }
  NOTREACHED();
  return WL_OUTPUT_TRANSFORM_NORMAL;
}

class WaylandDisplayObserver : public display::DisplayObserver {
 public:
  class ScaleObserver : public base::SupportsWeakPtr<ScaleObserver> {
   public:
    ScaleObserver() {}

    virtual void OnDisplayScalesChanged(const display::Display& display) = 0;

   protected:
    virtual ~ScaleObserver() {}
  };

  WaylandDisplayObserver(int64_t id, wl_resource* output_resource)
      : id_(id), output_resource_(output_resource) {
    display::Screen::GetScreen()->AddObserver(this);
    SendDisplayMetrics();
  }
  ~WaylandDisplayObserver() override {
    display::Screen::GetScreen()->RemoveObserver(this);
  }

  void SetScaleObserver(base::WeakPtr<ScaleObserver> scale_observer) {
    scale_observer_ = scale_observer;
    SendDisplayMetrics();
  }

  bool HasScaleObserver() const { return !!scale_observer_; }

  // Overridden from display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override {
    if (id_ != display.id())
      return;

    // There is no need to check DISPLAY_METRIC_PRIMARY because when primary
    // changes, bounds always changes. (new primary should have had non
    // 0,0 origin).
    // Only exception is when switching to newly connected primary with
    // the same bounds. This happens whenyou're in docked mode, suspend,
    // unplug the dislpay, then resume to the internal display which has
    // the same resolution. Since metrics does not change, there is no need
    // to notify clients.
    if (changed_metrics &
        (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
         DISPLAY_METRIC_ROTATION)) {
      SendDisplayMetrics();
    }
  }

 private:
  void SendDisplayMetrics() {
    display::Display display;
    bool rv =
        display::Screen::GetScreen()->GetDisplayWithDisplayId(id_, &display);
    DCHECK(rv);

    const display::ManagedDisplayInfo& info =
        WMHelper::GetInstance()->GetDisplayInfo(display.id());

    const float kInchInMm = 25.4f;
    const char* kUnknown = "unknown";

    const std::string& make = info.manufacturer_id();
    const std::string& model = info.product_id();

    gfx::Rect bounds = info.bounds_in_native();
    wl_output_send_geometry(
        output_resource_, bounds.x(), bounds.y(),
        static_cast<int>(kInchInMm * bounds.width() / info.device_dpi()),
        static_cast<int>(kInchInMm * bounds.height() / info.device_dpi()),
        WL_OUTPUT_SUBPIXEL_UNKNOWN, make.empty() ? kUnknown : make.c_str(),
        model.empty() ? kUnknown : model.c_str(),
        OutputTransform(display.rotation()));

    if (wl_resource_get_version(output_resource_) >=
        WL_OUTPUT_SCALE_SINCE_VERSION) {
      wl_output_send_scale(output_resource_, display.device_scale_factor());
    }

    // TODO(reveman): Send real list of modes.
    wl_output_send_mode(
        output_resource_, WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED,
        bounds.width(), bounds.height(), static_cast<int>(60000));

    if (HasScaleObserver())
      scale_observer_->OnDisplayScalesChanged(display);

    if (wl_resource_get_version(output_resource_) >=
        WL_OUTPUT_DONE_SINCE_VERSION) {
      wl_output_send_done(output_resource_);
    }

    wl_client_flush(wl_resource_get_client(output_resource_));
  }

  // The ID of the display being observed.
  const int64_t id_;

  // The output resource associated with the display.
  wl_resource* const output_resource_;

  base::WeakPtr<ScaleObserver> scale_observer_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDisplayObserver);
};

const uint32_t output_version = 2;

void bind_output(wl_client* client, void* data, uint32_t version, uint32_t id) {
  Server::Output* output = static_cast<Server::Output*>(data);

  wl_resource* resource = wl_resource_create(
      client, &wl_output_interface, std::min(version, output_version), id);

  SetImplementation(
      resource, nullptr,
      std::make_unique<WaylandDisplayObserver>(output->id(), resource));
}

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

void aura_surface_set_frame(wl_client* client, wl_resource* resource,
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

const struct zaura_surface_interface aura_surface_implementation = {
    aura_surface_set_frame, aura_surface_set_parent,
    aura_surface_set_frame_colors, aura_surface_set_startup_id,
    aura_surface_set_application_id};

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
      zaura_output_send_device_scale_factor(resource_,
                                            display_info.device_scale_factor() *
                                            1000);
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
        resource,
        ZAURA_SHELL_ERROR_AURA_SURFACE_EXISTS,
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

const uint32_t aura_shell_version = 6;

void bind_aura_shell(wl_client* client,
                     void* data,
                     uint32_t version,
                     uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zaura_shell_interface,
                         std::min(version, aura_shell_version), id);

  wl_resource_set_implementation(resource, &aura_shell_implementation,
                                 nullptr, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// vsync_timing_interface:

// Implements VSync timing interface by monitoring updates to VSync parameters.
class VSyncTiming final : public ui::CompositorVSyncManager::Observer {
 public:
  ~VSyncTiming() override {
    WMHelper::GetInstance()->RemoveVSyncObserver(this);
  }

  static std::unique_ptr<VSyncTiming> Create(wl_resource* timing_resource) {
    std::unique_ptr<VSyncTiming> vsync_timing(new VSyncTiming(timing_resource));
    // Note: AddObserver() will call OnUpdateVSyncParameters.
    WMHelper::GetInstance()->AddVSyncObserver(vsync_timing.get());
    return vsync_timing;
  }

  // Overridden from ui::CompositorVSyncManager::Observer:
  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval) override {
    uint64_t timebase_us = timebase.ToInternalValue();
    uint64_t interval_us = interval.ToInternalValue();

    // Ignore updates with interval 0.
    if (!interval_us)
      return;

    uint64_t offset_us = timebase_us % interval_us;

    // Avoid sending update events if interval did not change.
    if (interval_us == last_interval_us_) {
      int64_t offset_delta_us =
          static_cast<int64_t>(last_offset_us_ - offset_us);

      // Reduce the amount of events by only sending an update if the offset
      // changed compared to the last offset sent to the client by this amount.
      const int64_t kOffsetDeltaThresholdInMicroseconds = 25;

      if (std::abs(offset_delta_us) < kOffsetDeltaThresholdInMicroseconds)
        return;
    }

    zcr_vsync_timing_v1_send_update(timing_resource_, timebase_us & 0xffffffff,
                                    timebase_us >> 32, interval_us & 0xffffffff,
                                    interval_us >> 32);
    wl_client_flush(wl_resource_get_client(timing_resource_));

    last_interval_us_ = interval_us;
    last_offset_us_ = offset_us;
  }

 private:
  explicit VSyncTiming(wl_resource* timing_resource)
      : timing_resource_(timing_resource) {}

  // The VSync timing resource.
  wl_resource* const timing_resource_;

  uint64_t last_interval_us_{0};
  uint64_t last_offset_us_{0};

  DISALLOW_COPY_AND_ASSIGN(VSyncTiming);
};

void vsync_timing_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_vsync_timing_v1_interface vsync_timing_implementation = {
    vsync_timing_destroy};

////////////////////////////////////////////////////////////////////////////////
// vsync_feedback_interface:

void vsync_feedback_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void vsync_feedback_get_vsync_timing(wl_client* client,
                                     wl_resource* resource,
                                     uint32_t id,
                                     wl_resource* output) {
  wl_resource* timing_resource =
      wl_resource_create(client, &zcr_vsync_timing_v1_interface, 1, id);
  SetImplementation(timing_resource, &vsync_timing_implementation,
                    VSyncTiming::Create(timing_resource));
}

const struct zcr_vsync_feedback_v1_interface vsync_feedback_implementation = {
    vsync_feedback_destroy, vsync_feedback_get_vsync_timing};

void bind_vsync_feedback(wl_client* client,
                         void* data,
                         uint32_t version,
                         uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_vsync_feedback_v1_interface, 1, id);

  wl_resource_set_implementation(resource, &vsync_feedback_implementation,
                                 nullptr, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// wl_data_source_interface:

class WaylandDataSourceDelegate : public DataSourceDelegate {
 public:
  explicit WaylandDataSourceDelegate(wl_resource* source)
      : data_source_resource_(source) {}

  // Overridden from DataSourceDelegate:
  void OnDataSourceDestroying(DataSource* device) override { delete this; }
  void OnTarget(const std::string& mime_type) override {
    wl_data_source_send_target(data_source_resource_, mime_type.c_str());
    wl_client_flush(wl_resource_get_client(data_source_resource_));
  }
  void OnSend(const std::string& mime_type, base::ScopedFD fd) override {
    wl_data_source_send_send(data_source_resource_, mime_type.c_str(),
                             fd.get());
    wl_client_flush(wl_resource_get_client(data_source_resource_));
  }
  void OnCancelled() override {
    wl_data_source_send_cancelled(data_source_resource_);
    wl_client_flush(wl_resource_get_client(data_source_resource_));
  }
  void OnDndDropPerformed() override {
    if (wl_resource_get_version(data_source_resource_) >=
        WL_DATA_SOURCE_DND_DROP_PERFORMED_SINCE_VERSION) {
      wl_data_source_send_dnd_drop_performed(data_source_resource_);
      wl_client_flush(wl_resource_get_client(data_source_resource_));
    }
  }
  void OnDndFinished() override {
    if (wl_resource_get_version(data_source_resource_) >=
        WL_DATA_SOURCE_DND_FINISHED_SINCE_VERSION) {
      wl_data_source_send_dnd_finished(data_source_resource_);
      wl_client_flush(wl_resource_get_client(data_source_resource_));
    }
  }
  void OnAction(DndAction dnd_action) override {
    if (wl_resource_get_version(data_source_resource_) >=
        WL_DATA_SOURCE_ACTION_SINCE_VERSION) {
      wl_data_source_send_action(data_source_resource_,
                                 WaylandDataDeviceManagerDndAction(dnd_action));
      wl_client_flush(wl_resource_get_client(data_source_resource_));
    }
  }

 private:
  wl_resource* const data_source_resource_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDataSourceDelegate);
};

void data_source_offer(wl_client* client,
                       wl_resource* resource,
                       const char* mime_type) {
  GetUserDataAs<DataSource>(resource)->Offer(mime_type);
}

void data_source_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void data_source_set_actions(wl_client* client,
                             wl_resource* resource,
                             uint32_t dnd_actions) {
  GetUserDataAs<DataSource>(resource)->SetActions(
      DataDeviceManagerDndActions(dnd_actions));
}

const struct wl_data_source_interface data_source_implementation = {
    data_source_offer, data_source_destroy, data_source_set_actions};

////////////////////////////////////////////////////////////////////////////////
// wl_data_offer_interface:

class WaylandDataOfferDelegate : public DataOfferDelegate {
 public:
  explicit WaylandDataOfferDelegate(wl_resource* offer)
      : data_offer_resource_(offer) {}

  // Overridden from DataOfferDelegate:
  void OnDataOfferDestroying(DataOffer* device) override { delete this; }
  void OnOffer(const std::string& mime_type) override {
    wl_data_offer_send_offer(data_offer_resource_, mime_type.c_str());
    wl_client_flush(wl_resource_get_client(data_offer_resource_));
  }
  void OnSourceActions(
      const base::flat_set<DndAction>& source_actions) override {
    if (wl_resource_get_version(data_offer_resource_) >=
        WL_DATA_OFFER_SOURCE_ACTIONS_SINCE_VERSION) {
      wl_data_offer_send_source_actions(
          data_offer_resource_,
          WaylandDataDeviceManagerDndActions(source_actions));
      wl_client_flush(wl_resource_get_client(data_offer_resource_));
    }
  }
  void OnAction(DndAction action) override {
    if (wl_resource_get_version(data_offer_resource_) >=
        WL_DATA_OFFER_ACTION_SINCE_VERSION) {
      wl_data_offer_send_action(data_offer_resource_,
                                WaylandDataDeviceManagerDndAction(action));
      wl_client_flush(wl_resource_get_client(data_offer_resource_));
    }
  }

 private:
  wl_resource* const data_offer_resource_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDataOfferDelegate);
};

void data_offer_accept(wl_client* client,
                       wl_resource* resource,
                       uint32_t serial,
                       const char* mime_type) {
  GetUserDataAs<DataOffer>(resource)->Accept(mime_type);
}

void data_offer_receive(wl_client* client,
                        wl_resource* resource,
                        const char* mime_type,
                        int fd) {
  GetUserDataAs<DataOffer>(resource)->Receive(mime_type, base::ScopedFD(fd));
}

void data_offer_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void data_offer_finish(wl_client* client, wl_resource* resource) {
  GetUserDataAs<DataOffer>(resource)->Finish();
}

void data_offer_set_actions(wl_client* client,
                            wl_resource* resource,
                            uint32_t dnd_actions,
                            uint32_t preferred_action) {
  GetUserDataAs<DataOffer>(resource)->SetActions(
      DataDeviceManagerDndActions(dnd_actions),
      DataDeviceManagerDndAction(preferred_action));
}

const struct wl_data_offer_interface data_offer_implementation = {
    data_offer_accept, data_offer_receive, data_offer_finish,
    data_offer_destroy, data_offer_set_actions};

////////////////////////////////////////////////////////////////////////////////
// wl_data_device_interface:

class WaylandDataDeviceDelegate : public DataDeviceDelegate {
 public:
  WaylandDataDeviceDelegate(wl_client* client, wl_resource* device_resource)
      : client_(client), data_device_resource_(device_resource) {}

  // Overridden from DataDeviceDelegate:
  void OnDataDeviceDestroying(DataDevice* device) override { delete this; }
  bool CanAcceptDataEventsForSurface(Surface* surface) override {
    return surface &&
           wl_resource_get_client(GetSurfaceResource(surface)) == client_;
  }
  DataOffer* OnDataOffer() override {
    wl_resource* data_offer_resource =
        wl_resource_create(client_, &wl_data_offer_interface,
                           wl_resource_get_version(data_device_resource_), 0);
    std::unique_ptr<DataOffer> data_offer = std::make_unique<DataOffer>(
        new WaylandDataOfferDelegate(data_offer_resource));
    SetDataOfferResource(data_offer.get(), data_offer_resource);
    SetImplementation(data_offer_resource, &data_offer_implementation,
                      std::move(data_offer));

    wl_data_device_send_data_offer(data_device_resource_, data_offer_resource);
    wl_client_flush(client_);

    return GetUserDataAs<DataOffer>(data_offer_resource);
  }
  void OnEnter(Surface* surface,
               const gfx::PointF& point,
               const DataOffer& data_offer) override {
    wl_data_device_send_enter(
        data_device_resource_,
        wl_display_next_serial(wl_client_get_display(client_)),
        GetSurfaceResource(surface), wl_fixed_from_double(point.x()),
        wl_fixed_from_double(point.y()), GetDataOfferResource(&data_offer));
    wl_client_flush(client_);
  }
  void OnLeave() override {
    wl_data_device_send_leave(data_device_resource_);
    wl_client_flush(client_);
  }
  void OnMotion(base::TimeTicks time_stamp, const gfx::PointF& point) override {
    wl_data_device_send_motion(
        data_device_resource_, TimeTicksToMilliseconds(time_stamp),
        wl_fixed_from_double(point.x()), wl_fixed_from_double(point.y()));
    wl_client_flush(client_);
  }
  void OnDrop() override {
    wl_data_device_send_drop(data_device_resource_);
    wl_client_flush(client_);
  }
  void OnSelection(const DataOffer& data_offer) override {
    wl_data_device_send_selection(data_device_resource_,
                                  GetDataOfferResource(&data_offer));
    wl_client_flush(client_);
  }

 private:
  wl_client* const client_;
  wl_resource* const data_device_resource_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDataDeviceDelegate);
};

void data_device_start_drag(wl_client* client,
                            wl_resource* resource,
                            wl_resource* source_resource,
                            wl_resource* origin_resource,
                            wl_resource* icon_resource,
                            uint32_t serial) {
  GetUserDataAs<DataDevice>(resource)->StartDrag(
      source_resource ? GetUserDataAs<DataSource>(source_resource) : nullptr,
      GetUserDataAs<Surface>(origin_resource),
      icon_resource ? GetUserDataAs<Surface>(icon_resource) : nullptr, serial);
}

void data_device_set_selection(wl_client* client,
                               wl_resource* resource,
                               wl_resource* data_source,
                               uint32_t serial) {
  GetUserDataAs<DataDevice>(resource)->SetSelection(
      data_source ? GetUserDataAs<DataSource>(data_source) : nullptr, serial);
}

void data_device_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_data_device_interface data_device_implementation = {
    data_device_start_drag, data_device_set_selection, data_device_release};

////////////////////////////////////////////////////////////////////////////////
// wl_data_device_manager_interface:

void data_device_manager_create_data_source(wl_client* client,
                                            wl_resource* resource,
                                            uint32_t id) {
  wl_resource* data_source_resource = wl_resource_create(
      client, &wl_data_source_interface, wl_resource_get_version(resource), id);
  SetImplementation(data_source_resource, &data_source_implementation,
                    std::make_unique<DataSource>(
                        new WaylandDataSourceDelegate(data_source_resource)));
}

void data_device_manager_get_data_device(wl_client* client,
                                         wl_resource* resource,
                                         uint32_t id,
                                         wl_resource* seat_resource) {
  Display* display = GetUserDataAs<Display>(resource);
  wl_resource* data_device_resource = wl_resource_create(
      client, &wl_data_device_interface, wl_resource_get_version(resource), id);
  SetImplementation(data_device_resource, &data_device_implementation,
                    display->CreateDataDevice(new WaylandDataDeviceDelegate(
                        client, data_device_resource)));
}

const struct wl_data_device_manager_interface
    data_device_manager_implementation = {
        data_device_manager_create_data_source,
        data_device_manager_get_data_device};

const uint32_t data_device_manager_version = 3;

void bind_data_device_manager(wl_client* client,
                              void* data,
                              uint32_t version,
                              uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wl_data_device_manager_interface,
                         std::min(version, data_device_manager_version), id);
  wl_resource_set_implementation(resource, &data_device_manager_implementation,
                                 data, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// wp_viewport_interface:

// Implements the viewport interface to a Surface. The "viewport"-state is set
// to null upon destruction. A window property will be set during the lifetime
// of this class to prevent multiple instances from being created for the same
// Surface.
class Viewport : public SurfaceObserver {
 public:
  explicit Viewport(Surface* surface) : surface_(surface) {
    surface_->AddSurfaceObserver(this);
    surface_->SetProperty(kSurfaceHasViewportKey, true);
  }
  ~Viewport() override {
    if (surface_) {
      surface_->RemoveSurfaceObserver(this);
      surface_->SetCrop(gfx::RectF());
      surface_->SetViewport(gfx::Size());
      surface_->SetProperty(kSurfaceHasViewportKey, false);
    }
  }

  void SetSource(const gfx::RectF& rect) {
    if (surface_)
      surface_->SetCrop(rect);
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

void viewport_set_source(wl_client* client,
                         wl_resource* resource,
                         wl_fixed_t x,
                         wl_fixed_t y,
                         wl_fixed_t width,
                         wl_fixed_t height) {
  if (x == wl_fixed_from_int(-1) && y == wl_fixed_from_int(-1) &&
      width == wl_fixed_from_int(-1) && height == wl_fixed_from_int(-1)) {
    GetUserDataAs<Viewport>(resource)->SetSource(gfx::RectF());
    return;
  }

  if (x < 0 || y < 0 || width <= 0 || height <= 0) {
    wl_resource_post_error(resource, WP_VIEWPORT_ERROR_BAD_VALUE,
                           "source rectangle must be non-empty (%dx%d) and"
                           "have positive origin (%d,%d)",
                           width, height, x, y);
    return;
  }

  GetUserDataAs<Viewport>(resource)->SetSource(
      gfx::RectF(wl_fixed_to_double(x), wl_fixed_to_double(y),
                 wl_fixed_to_double(width), wl_fixed_to_double(height)));
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
    wl_resource_post_error(resource, WP_VIEWPORT_ERROR_BAD_VALUE,
                           "destination size must be positive (%dx%d)", width,
                           height);
    return;
  }

  GetUserDataAs<Viewport>(resource)->SetDestination(gfx::Size(width, height));
}

const struct wp_viewport_interface viewport_implementation = {
    viewport_destroy, viewport_set_source, viewport_set_destination};

////////////////////////////////////////////////////////////////////////////////
// wp_viewporter_interface:

void viewporter_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void viewporter_get_viewport(wl_client* client,
                             wl_resource* resource,
                             uint32_t id,
                             wl_resource* surface_resource) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  if (surface->GetProperty(kSurfaceHasViewportKey)) {
    wl_resource_post_error(resource, WP_VIEWPORTER_ERROR_VIEWPORT_EXISTS,
                           "a viewport for that surface already exists");
    return;
  }

  wl_resource* viewport_resource = wl_resource_create(
      client, &wp_viewport_interface, wl_resource_get_version(resource), id);

  SetImplementation(viewport_resource, &viewport_implementation,
                    std::make_unique<Viewport>(surface));
}

const struct wp_viewporter_interface viewporter_implementation = {
    viewporter_destroy, viewporter_get_viewport};

void bind_viewporter(wl_client* client,
                     void* data,
                     uint32_t version,
                     uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wp_viewporter_interface, 1, id);

  wl_resource_set_implementation(resource, &viewporter_implementation, data,
                                 nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// presentation_interface:

void HandleSurfacePresentationCallback(
    wl_resource* resource,
    const gfx::PresentationFeedback& feedback) {
  if (feedback.timestamp.is_null()) {
    wp_presentation_feedback_send_discarded(resource);
  } else {
    int64_t presentation_time_us = feedback.timestamp.ToInternalValue();
    int64_t seconds = presentation_time_us / base::Time::kMicrosecondsPerSecond;
    int64_t microseconds =
        presentation_time_us % base::Time::kMicrosecondsPerSecond;
    static_assert(
        static_cast<uint32_t>(gfx::PresentationFeedback::Flags::kVSync) ==
            static_cast<uint32_t>(WP_PRESENTATION_FEEDBACK_KIND_VSYNC),
        "gfx::PresentationFlags::VSync don't match!");
    static_assert(
        static_cast<uint32_t>(gfx::PresentationFeedback::Flags::kHWClock) ==
            static_cast<uint32_t>(WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK),
        "gfx::PresentationFlags::HWClock don't match!");
    static_assert(
        static_cast<uint32_t>(
            gfx::PresentationFeedback::Flags::kHWCompletion) ==
            static_cast<uint32_t>(WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION),
        "gfx::PresentationFlags::HWCompletion don't match!");
    static_assert(
        static_cast<uint32_t>(gfx::PresentationFeedback::Flags::kZeroCopy) ==
            static_cast<uint32_t>(WP_PRESENTATION_FEEDBACK_KIND_ZERO_COPY),
        "gfx::PresentationFlags::ZeroCopy don't match!");
    wp_presentation_feedback_send_presented(
        resource, seconds >> 32, seconds & 0xffffffff,
        microseconds * base::Time::kNanosecondsPerMicrosecond,
        feedback.interval.InMicroseconds() *
            base::Time::kNanosecondsPerMicrosecond,
        0, 0, feedback.flags);
  }
  wl_client_flush(wl_resource_get_client(resource));
}

void presentation_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void presentation_feedback(wl_client* client,
                           wl_resource* resource,
                           wl_resource* surface_resource,
                           uint32_t id) {
  wl_resource* presentation_feedback_resource =
      wl_resource_create(client, &wp_presentation_feedback_interface,
                         wl_resource_get_version(resource), id);

  // base::Unretained is safe as the resource owns the callback.
  auto cancelable_callback = std::make_unique<
      base::CancelableCallback<void(const gfx::PresentationFeedback&)>>(
      base::Bind(&HandleSurfacePresentationCallback,
                 base::Unretained(presentation_feedback_resource)));

  GetUserDataAs<Surface>(surface_resource)
      ->RequestPresentationCallback(cancelable_callback->callback());

  SetImplementation(presentation_feedback_resource, nullptr,
                    std::move(cancelable_callback));
}

const struct wp_presentation_interface presentation_implementation = {
    presentation_destroy, presentation_feedback};

void bind_presentation(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wp_presentation_interface, 1, id);

  wl_resource_set_implementation(resource, &presentation_implementation, data,
                                 nullptr);

  wp_presentation_send_clock_id(resource, CLOCK_MONOTONIC);
}

////////////////////////////////////////////////////////////////////////////////
// security_interface:

// Implements the security interface to a Surface. The "only visible on secure
// output"-state is set to false upon destruction. A window property will be set
// during the lifetime of this class to prevent multiple instances from being
// created for the same Surface.
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

const struct zcr_security_v1_interface security_implementation = {
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
    wl_resource_post_error(resource, ZCR_SECURE_OUTPUT_V1_ERROR_SECURITY_EXISTS,
                           "a security object for that surface already exists");
    return;
  }

  wl_resource* security_resource =
      wl_resource_create(client, &zcr_security_v1_interface, 1, id);

  SetImplementation(security_resource, &security_implementation,
                    std::make_unique<Security>(surface));
}

const struct zcr_secure_output_v1_interface secure_output_implementation = {
    secure_output_destroy, secure_output_get_security};

void bind_secure_output(wl_client* client,
                        void* data,
                        uint32_t version,
                        uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_secure_output_v1_interface, 1, id);

  wl_resource_set_implementation(resource, &secure_output_implementation, data,
                                 nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// blending_interface:

// Implements the blending interface to a Surface. The "blend mode" and
// "alpha"-state is set to SrcOver and 1 upon destruction. A window property
// will be set during the lifetime of this class to prevent multiple instances
// from being created for the same Surface.
class Blending : public SurfaceObserver {
 public:
  explicit Blending(Surface* surface) : surface_(surface) {
    surface_->AddSurfaceObserver(this);
    surface_->SetProperty(kSurfaceHasBlendingKey, true);
  }
  ~Blending() override {
    if (surface_) {
      surface_->RemoveSurfaceObserver(this);
      surface_->SetBlendMode(SkBlendMode::kSrcOver);
      surface_->SetAlpha(1.0f);
      surface_->SetProperty(kSurfaceHasBlendingKey, false);
    }
  }

  void SetBlendMode(SkBlendMode blend_mode) {
    if (surface_)
      surface_->SetBlendMode(blend_mode);
  }

  void SetAlpha(float value) {
    if (surface_)
      surface_->SetAlpha(value);
  }

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override {
    surface->RemoveSurfaceObserver(this);
    surface_ = nullptr;
  }

 private:
  Surface* surface_;

  DISALLOW_COPY_AND_ASSIGN(Blending);
};

void blending_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void blending_set_blending(wl_client* client,
                           wl_resource* resource,
                           uint32_t equation) {
  switch (equation) {
    case ZCR_BLENDING_V1_BLENDING_EQUATION_NONE:
      GetUserDataAs<Blending>(resource)->SetBlendMode(SkBlendMode::kSrc);
      break;
    case ZCR_BLENDING_V1_BLENDING_EQUATION_PREMULT:
      GetUserDataAs<Blending>(resource)->SetBlendMode(SkBlendMode::kSrcOver);
      break;
    case ZCR_BLENDING_V1_BLENDING_EQUATION_COVERAGE:
      NOTIMPLEMENTED();
      break;
    default:
      DLOG(WARNING) << "Unsupported blending equation: " << equation;
      break;
  }
}

void blending_set_alpha(wl_client* client,
                        wl_resource* resource,
                        wl_fixed_t alpha) {
  GetUserDataAs<Blending>(resource)->SetAlpha(wl_fixed_to_double(alpha));
}

const struct zcr_blending_v1_interface blending_implementation = {
    blending_destroy, blending_set_blending, blending_set_alpha};

////////////////////////////////////////////////////////////////////////////////
// alpha_compositing_interface:

void alpha_compositing_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void alpha_compositing_get_blending(wl_client* client,
                                    wl_resource* resource,
                                    uint32_t id,
                                    wl_resource* surface_resource) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  if (surface->GetProperty(kSurfaceHasBlendingKey)) {
    wl_resource_post_error(resource,
                           ZCR_ALPHA_COMPOSITING_V1_ERROR_BLENDING_EXISTS,
                           "a blending object for that surface already exists");
    return;
  }

  wl_resource* blending_resource =
      wl_resource_create(client, &zcr_blending_v1_interface, 1, id);

  SetImplementation(blending_resource, &blending_implementation,
                    std::make_unique<Blending>(surface));
}

const struct zcr_alpha_compositing_v1_interface
    alpha_compositing_implementation = {alpha_compositing_destroy,
                                        alpha_compositing_get_blending};

void bind_alpha_compositing(wl_client* client,
                            void* data,
                            uint32_t version,
                            uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_alpha_compositing_v1_interface, 1, id);

  wl_resource_set_implementation(resource, &alpha_compositing_implementation,
                                 data, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// touch_stylus interface:

class WaylandTouchStylusDelegate : public TouchStylusDelegate {
 public:
  WaylandTouchStylusDelegate(wl_resource* resource, Touch* touch)
      : resource_(resource), touch_(touch) {
    touch_->SetStylusDelegate(this);
  }
  ~WaylandTouchStylusDelegate() override {
    if (touch_ != nullptr)
      touch_->SetStylusDelegate(nullptr);
  }
  void OnTouchDestroying(Touch* touch) override { touch_ = nullptr; }
  void OnTouchTool(int touch_id, ui::EventPointerType type) override {
    uint wayland_type = ZCR_TOUCH_STYLUS_V2_TOOL_TYPE_TOUCH;
    if (type == ui::EventPointerType::POINTER_TYPE_PEN)
      wayland_type = ZCR_TOUCH_STYLUS_V2_TOOL_TYPE_PEN;
    else if (type == ui::EventPointerType::POINTER_TYPE_ERASER)
      wayland_type = ZCR_TOUCH_STYLUS_V2_TOOL_TYPE_ERASER;
    zcr_touch_stylus_v2_send_tool(resource_, touch_id, wayland_type);
  }
  void OnTouchForce(base::TimeTicks time_stamp,
                    int touch_id,
                    float force) override {
    zcr_touch_stylus_v2_send_force(resource_,
                                   TimeTicksToMilliseconds(time_stamp),
                                   touch_id, wl_fixed_from_double(force));
  }
  void OnTouchTilt(base::TimeTicks time_stamp,
                   int touch_id,
                   const gfx::Vector2dF& tilt) override {
    zcr_touch_stylus_v2_send_tilt(
        resource_, TimeTicksToMilliseconds(time_stamp), touch_id,
        wl_fixed_from_double(tilt.x()), wl_fixed_from_double(tilt.y()));
  }

 private:
  wl_resource* resource_;
  Touch* touch_;

  DISALLOW_COPY_AND_ASSIGN(WaylandTouchStylusDelegate);
};

void touch_stylus_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_touch_stylus_v2_interface touch_stylus_implementation = {
    touch_stylus_destroy};

////////////////////////////////////////////////////////////////////////////////
// stylus_v2 interface:

void stylus_get_touch_stylus(wl_client* client,
                             wl_resource* resource,
                             uint32_t id,
                             wl_resource* touch_resource) {
  Touch* touch = GetUserDataAs<Touch>(touch_resource);
  if (touch->HasStylusDelegate()) {
    wl_resource_post_error(
        resource, ZCR_STYLUS_V2_ERROR_TOUCH_STYLUS_EXISTS,
        "touch has already been associated with a stylus object");
    return;
  }

  wl_resource* stylus_resource =
      wl_resource_create(client, &zcr_touch_stylus_v2_interface, 1, id);

  SetImplementation(
      stylus_resource, &touch_stylus_implementation,
      std::make_unique<WaylandTouchStylusDelegate>(stylus_resource, touch));
}

const struct zcr_stylus_v2_interface stylus_v2_implementation = {
    stylus_get_touch_stylus};

void bind_stylus_v2(wl_client* client,
                    void* data,
                    uint32_t version,
                    uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_stylus_v2_interface, version, id);
  wl_resource_set_implementation(resource, &stylus_v2_implementation, data,
                                 nullptr);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Server::Output, public:

Server::Output::Output(int64_t id) : id_(id) {}

Server::Output::~Output() {
  if (global_)
    wl_global_destroy(global_);
}

////////////////////////////////////////////////////////////////////////////////
// Server, public:

Server::Server(Display* display)
    : display_(display), wl_display_(wl_display_create()) {
  wl_global_create(wl_display_.get(), &wl_compositor_interface,
                   compositor_version, display_, bind_compositor);
  wl_global_create(wl_display_.get(), &wl_shm_interface, 1, display_, bind_shm);
#if defined(USE_OZONE)
  wl_global_create(wl_display_.get(), &zwp_linux_dmabuf_v1_interface,
                   linux_dmabuf_version, display_, bind_linux_dmabuf);
#endif
  wl_global_create(wl_display_.get(), &wl_subcompositor_interface, 1, display_,
                   bind_subcompositor);
  wl_global_create(wl_display_.get(), &wl_shell_interface, 1, display_,
                   bind_shell);
  display::Screen::GetScreen()->AddObserver(this);
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays())
    OnDisplayAdded(display);
  wl_global_create(wl_display_.get(), &zxdg_shell_v6_interface, 1, display_,
                   bind_xdg_shell_v6);
  wl_global_create(wl_display_.get(), &zcr_vsync_feedback_v1_interface, 1,
                   display_, bind_vsync_feedback);
  wl_global_create(wl_display_.get(), &wl_data_device_manager_interface,
                   data_device_manager_version, display_,
                   bind_data_device_manager);
  wl_global_create(wl_display_.get(), &wl_seat_interface, kWlSeatVersion,
                   display_->seat(), bind_seat);
  wl_global_create(wl_display_.get(), &wp_viewporter_interface, 1, display_,
                   bind_viewporter);
  wl_global_create(wl_display_.get(), &wp_presentation_interface, 1, display_,
                   bind_presentation);
  wl_global_create(wl_display_.get(), &zcr_secure_output_v1_interface, 1,
                   display_, bind_secure_output);
  wl_global_create(wl_display_.get(), &zcr_alpha_compositing_v1_interface, 1,
                   display_, bind_alpha_compositing);
  wl_global_create(wl_display_.get(), &zcr_remote_shell_v1_interface,
                   kZcrRemoteShellVersion, display_, bind_remote_shell);
  wl_global_create(wl_display_.get(), &zaura_shell_interface,
                   aura_shell_version, display_, bind_aura_shell);
  wl_global_create(wl_display_.get(), &zcr_gaming_input_v2_interface, 1,
                   display_, bind_gaming_input);
  wl_global_create(wl_display_.get(), &zcr_stylus_v2_interface, 1, display_,
                   bind_stylus_v2);
  wl_global_create(wl_display_.get(), &zwp_pointer_gestures_v1_interface, 1,
                   display_, bind_pointer_gestures);
  wl_global_create(wl_display_.get(), &zcr_keyboard_configuration_v1_interface,
                   kZcrKeyboardConfigurationVersion, display_,
                   bind_keyboard_configuration);
  wl_global_create(wl_display_.get(), &zcr_stylus_tools_v1_interface, 1,
                   display_, bind_stylus_tools);
  wl_global_create(wl_display_.get(), &zcr_keyboard_extension_v1_interface, 1,
                   display_, bind_keyboard_extension);
  wl_global_create(wl_display_.get(), &zcr_cursor_shapes_v1_interface, 1,
                   display_, bind_cursor_shapes);
  wl_global_create(wl_display_.get(),
                   &zwp_input_timestamps_manager_v1_interface, 1, display_,
                   bind_input_timestamps_manager);
  wl_global_create(wl_display_.get(), &zwp_text_input_manager_v1_interface, 1,
                   display_, bind_text_input_manager);
  wl_global_create(wl_display_.get(), &zcr_notification_shell_v1_interface, 1,
                   display_, bind_notification_shell);

#if defined(USE_FULLSCREEN_SHELL)
  wl_global_create(wl_display_.get(), &zwp_fullscreen_shell_v1_interface, 1,
                   display_, bind_fullscreen_shell);
#endif
}

Server::~Server() {
  display::Screen::GetScreen()->RemoveObserver(this);
}

// static
std::unique_ptr<Server> Server::Create(Display* display) {
  std::unique_ptr<Server> server(new Server(display));

  char* runtime_dir = getenv("XDG_RUNTIME_DIR");
  if (!runtime_dir) {
    LOG(ERROR) << "XDG_RUNTIME_DIR not set in the environment";
    return nullptr;
  }

  std::string socket_name(kSocketName);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kWaylandServerSocket)) {
    socket_name =
        command_line->GetSwitchValueASCII(switches::kWaylandServerSocket);
  }

  if (!server->AddSocket(socket_name.c_str())) {
    LOG(ERROR) << "Failed to add socket: " << socket_name;
    return nullptr;
  }

  base::FilePath socket_path = base::FilePath(runtime_dir).Append(socket_name);

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

void Server::OnDisplayAdded(const display::Display& new_display) {
  auto output = std::make_unique<Output>(new_display.id());
  output->set_global(wl_global_create(wl_display_.get(), &wl_output_interface,
                                      output_version, output.get(),
                                      bind_output));
  DCHECK_EQ(outputs_.count(new_display.id()), 0u);
  outputs_.insert(std::make_pair(new_display.id(), std::move(output)));
}

void Server::OnDisplayRemoved(const display::Display& old_display) {
  DCHECK_EQ(outputs_.count(old_display.id()), 1u);
  outputs_.erase(old_display.id());
}

}  // namespace wayland
}  // namespace exo
