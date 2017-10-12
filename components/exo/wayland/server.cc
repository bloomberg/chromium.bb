// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/server.h"

#include <alpha-compositing-unstable-v1-server-protocol.h>
#include <aura-shell-server-protocol.h>
#include <gaming-input-unstable-v1-server-protocol.h>
#include <gaming-input-unstable-v2-server-protocol.h>
#include <grp.h>
#include <keyboard-configuration-unstable-v1-server-protocol.h>
#include <keyboard-extension-unstable-v1-server-protocol.h>
#include <linux/input.h>
#include <pointer-gestures-unstable-v1-server-protocol.h>
#include <presentation-time-server-protocol.h>
#include <remote-shell-unstable-v1-server-protocol.h>
#include <secure-output-unstable-v1-server-protocol.h>
#include <stddef.h>
#include <stdint.h>
#include <stylus-tools-unstable-v1-server-protocol.h>
#include <stylus-unstable-v1-server-protocol.h>
#include <stylus-unstable-v2-server-protocol.h>
#include <viewporter-server-protocol.h>
#include <vsync-feedback-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>
#include <xdg-shell-unstable-v5-server-protocol.h>
#include <xdg-shell-unstable-v6-server-protocol.h>

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/interfaces/window_pin_type.mojom.h"
#include "ash/public/interfaces/window_state_type.mojom.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/free_deleter.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/exo/buffer.h"
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
#include "components/exo/keyboard.h"
#include "components/exo/keyboard_delegate.h"
#include "components/exo/keyboard_device_configuration_delegate.h"
#include "components/exo/keyboard_observer.h"
#include "components/exo/notification_surface.h"
#include "components/exo/notification_surface_manager.h"
#include "components/exo/pointer.h"
#include "components/exo/pointer_delegate.h"
#include "components/exo/pointer_gesture_pinch_delegate.h"
#include "components/exo/shared_memory.h"
#include "components/exo/shell_surface.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/touch.h"
#include "components/exo/touch_delegate.h"
#include "components/exo/touch_stylus_delegate.h"
#include "components/exo/wm_helper.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_features.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_animations.h"

#if defined(USE_OZONE)
#include <drm_fourcc.h>
#include <linux-dmabuf-unstable-v1-server-protocol.h>
#if defined(OS_CHROMEOS)
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#endif
#endif

#if BUILDFLAG(USE_XKBCOMMON)
#include <xkbcommon/xkbcommon.h>
#include "ui/events/keycodes/scoped_xkb.h"  // nogncheck
#endif

DECLARE_UI_CLASS_PROPERTY_TYPE(wl_resource*);

namespace exo {
namespace wayland {
namespace switches {

constexpr char kForceRemoteShellScaleSwitch[] = "force-remote-shell-scale";
}

namespace {

// We don't send configure immediately after tablet mode switch
// because layout can change due to orientation lock state or accelerometer.
const int kConfigureDelayAfterLayoutSwitchMs = 300;

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

// Returns the scale factor to be used by remote shell clients.
double GetDefaultDeviceScaleFactor() {
  // A flag used by VM to emulate a device scale for a partiular board.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kForceRemoteShellScaleSwitch)) {
    std::string value = command_line->GetSwitchValueASCII(
        switches::kForceRemoteShellScaleSwitch);
    double scale = 1.0;
    if (base::StringToDouble(value, &scale))
      return std::max(1.0, scale);
  }
  return WMHelper::GetInstance()->GetDefaultDeviceScaleFactor();
}

// Convert a timestamp to a time value that can be used when interfacing
// with wayland. Note that we cast a int64_t value to uint32_t which can
// potentially overflow.
uint32_t TimeTicksToMilliseconds(base::TimeTicks ticks) {
  return (ticks - base::TimeTicks()).InMilliseconds();
}

uint32_t NowInMilliseconds() {
  return TimeTicksToMilliseconds(base::TimeTicks::Now());
}

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

// A property key containing the surface resource that is associated with
// window. If unset, no surface resource is associated with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(wl_resource*, kSurfaceResourceKey, nullptr);

// A property key containing a boolean set to true if a viewport is associated
// with with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasViewportKey, false);

// A property key containing a boolean set to true if a security object is
// associated with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasSecurityKey, false);

// A property key containing a boolean set to true if a blending object is
// associated with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasBlendingKey, false);

// A property key containing a boolean set to true if the stylus_tool
// object is associated with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasStylusToolKey, false);

// A property key containing the data offer resource that is associated with
// data offer object.
DEFINE_UI_CLASS_PROPERTY_KEY(wl_resource*, kDataOfferResourceKey, nullptr);

// A property key containing a boolean set to true if na aura surface object is
// associated with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasAuraSurfaceKey, false);

wl_resource* GetSurfaceResource(Surface* surface) {
  return surface->GetProperty(kSurfaceResourceKey);
}

wl_resource* GetDataOfferResource(const DataOffer* data_offer) {
  return data_offer->GetProperty(kDataOfferResourceKey);
}

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
          base::SharedMemoryHandle::ImportHandle(fd, size), size);
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
    return;
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

  if (flags & (ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_Y_INVERT |
               ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_INTERLACED)) {
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
    planes.emplace_back(plane.stride, plane.offset, 0, 0);
    fds.push_back(std::move(plane.fd));
  }

  std::unique_ptr<Buffer> buffer =
      linux_buffer_params->display->CreateLinuxDMABufBuffer(
          gfx::Size(width, height), supported_format->buffer_format, planes,
          std::move(fds));
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
    planes.emplace_back(plane.stride, plane.offset, 0, 0);
    fds.push_back(std::move(plane.fd));
  }

  std::unique_ptr<Buffer> buffer =
      linux_buffer_params->display->CreateLinuxDMABufBuffer(
          gfx::Size(width, height), supported_format->buffer_format, planes,
          std::move(fds));
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
      wl_resource_create(client, &zwp_linux_buffer_params_v1_interface, 2, id);

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
      wl_resource_create(client, &zwp_linux_dmabuf_v1_interface, version, id);

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
  ShellSurface* shell_surface = GetUserDataAs<ShellSurface>(resource);
  if (shell_surface->enabled())
    return;

  // Parent widget can be found by locating the closest ancestor with a widget.
  views::Widget* parent_widget = nullptr;
  aura::Window* parent_window =
      GetUserDataAs<Surface>(parent_resource)->window();
  while (parent_window) {
    parent_widget = views::Widget::GetWidgetForNativeWindow(parent_window);
    if (parent_widget)
      break;
    parent_window = parent_window->parent();
  }

  DLOG_IF(WARNING, parent_resource && !!parent_widget)
      << "Parent surface is not a visible shell surface";

  gfx::Point origin(x, y);
  ShellSurface* parent_shell_surface = nullptr;

  // Set parent if found and it is associated with a shell surface.
  if (parent_widget &&
      ShellSurface::GetMainSurface(parent_widget->GetNativeWindow())) {
    wm::ConvertPointToScreen(
        ShellSurface::GetMainSurface(parent_widget->GetNativeWindow())
            ->window(),
        &origin);
    // Shell surface widget delegate implementation of GetContentsView()
    // returns a pointer to the shell surface instance.
    parent_shell_surface = static_cast<ShellSurface*>(
        parent_widget->widget_delegate()->GetContentsView());
  }

  if (flags & WL_SHELL_SURFACE_TRANSIENT_INACTIVE) {
    shell_surface->SetOrigin(origin);
    shell_surface->SetContainer(ash::kShellWindowId_SystemModalContainer);
    shell_surface->SetActivatable(false);
  } else {
    shell_surface->SetParent(parent_shell_surface);
  }
  shell_surface->SetEnabled(true);
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

void HandleShellSurfaceCloseCallback(wl_resource* resource) {
  // Shell surface interface doesn't have a close event. Just send a ping event
  // for now.
  uint32_t serial = wl_display_next_serial(
      wl_client_get_display(wl_resource_get_client(resource)));
  wl_shell_surface_send_ping(resource, serial);
  wl_client_flush(wl_resource_get_client(resource));
}

uint32_t HandleShellSurfaceConfigureCallback(
    wl_resource* resource,
    const gfx::Size& size,
    ash::mojom::WindowStateType state_type,
    bool resizing,
    bool activated,
    const gfx::Vector2d& origin_offset) {
  wl_shell_surface_send_configure(resource, WL_SHELL_SURFACE_RESIZE_NONE,
                                  size.width(), size.height());
  wl_client_flush(wl_resource_get_client(resource));
  return 0;
}

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

  shell_surface->set_close_callback(
      base::Bind(&HandleShellSurfaceCloseCallback,
                 base::Unretained(shell_surface_resource)));

  shell_surface->set_configure_callback(
      base::Bind(&HandleShellSurfaceConfigureCallback,
                 base::Unretained(shell_surface_resource)));

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

class WaylandPrimaryDisplayObserver : public display::DisplayObserver {
 public:
  explicit WaylandPrimaryDisplayObserver(wl_resource* output_resource)
      : output_resource_(output_resource) {
    display::Screen::GetScreen()->AddObserver(this);
    SendDisplayMetrics();
  }
  ~WaylandPrimaryDisplayObserver() override {
    display::Screen::GetScreen()->RemoveObserver(this);
  }

  // Overridden from display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override {
    if (display::Screen::GetScreen()->GetPrimaryDisplay().id() != display.id())
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
    display::Display display =
        display::Screen::GetScreen()->GetPrimaryDisplay();

    const display::ManagedDisplayInfo& info =
        WMHelper::GetInstance()->GetDisplayInfo(display.id());

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

  // The output resource associated with the display.
  wl_resource* const output_resource_;

  DISALLOW_COPY_AND_ASSIGN(WaylandPrimaryDisplayObserver);
};

const uint32_t output_version = 2;

void bind_output(wl_client* client, void* data, uint32_t version, uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &wl_output_interface, std::min(version, output_version), id);

  SetImplementation(resource, nullptr,
                    std::make_unique<WaylandPrimaryDisplayObserver>(resource));
}

////////////////////////////////////////////////////////////////////////////////
// xdg_positioner_interface:

void xdg_positioner_v6_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_positioner_v6_set_size(wl_client* client,
                                wl_resource* resource,
                                int32_t width,
                                int32_t height) {
  NOTIMPLEMENTED();
}

void xdg_positioner_v6_set_anchor_rect(wl_client* client,
                                       wl_resource* resource,
                                       int32_t x,
                                       int32_t y,
                                       int32_t width,
                                       int32_t height) {
  NOTIMPLEMENTED();
}

void xdg_positioner_v6_set_anchor(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t anchor) {
  NOTIMPLEMENTED();
}

void xdg_positioner_v6_set_gravity(wl_client* client,
                                   wl_resource* resource,
                                   uint32_t gravity) {
  NOTIMPLEMENTED();
}

void xdg_positioner_v6_set_constraint_adjustment(
    wl_client* client,
    wl_resource* resource,
    uint32_t constraint_adjustment) {
  NOTIMPLEMENTED();
}

void xdg_positioner_v6_set_offset(wl_client* client,
                                  wl_resource* resource,
                                  int32_t x,
                                  int32_t y) {
  NOTIMPLEMENTED();
}

const struct zxdg_positioner_v6_interface xdg_positioner_v6_implementation = {
    xdg_positioner_v6_destroy,
    xdg_positioner_v6_set_size,
    xdg_positioner_v6_set_anchor_rect,
    xdg_positioner_v6_set_anchor,
    xdg_positioner_v6_set_gravity,
    xdg_positioner_v6_set_constraint_adjustment,
    xdg_positioner_v6_set_offset};

////////////////////////////////////////////////////////////////////////////////
// xdg_toplevel_interface:

int XdgToplevelV6ResizeComponent(uint32_t edges) {
  switch (edges) {
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP:
      return HTTOP;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM:
      return HTBOTTOM;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_LEFT:
      return HTLEFT;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP_LEFT:
      return HTTOPLEFT;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM_LEFT:
      return HTBOTTOMLEFT;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_RIGHT:
      return HTRIGHT;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP_RIGHT:
      return HTTOPRIGHT;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM_RIGHT:
      return HTBOTTOMRIGHT;
    default:
      return HTBOTTOMRIGHT;
  }
}

void xdg_toplevel_v6_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_toplevel_v6_set_parent(wl_client* client,
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

void xdg_toplevel_v6_set_title(wl_client* client,
                               wl_resource* resource,
                               const char* title) {
  GetUserDataAs<ShellSurface>(resource)->SetTitle(
      base::string16(base::UTF8ToUTF16(title)));
}

void xdg_toplevel_v6_set_add_id(wl_client* client,
                                wl_resource* resource,
                                const char* app_id) {
  GetUserDataAs<ShellSurface>(resource)->SetApplicationId(app_id);
}

void xdg_toplevel_v6_show_window_menu(wl_client* client,
                                      wl_resource* resource,
                                      wl_resource* seat,
                                      uint32_t serial,
                                      int32_t x,
                                      int32_t y) {
  NOTIMPLEMENTED();
}

void xdg_toplevel_v6_move(wl_client* client,
                          wl_resource* resource,
                          wl_resource* seat,
                          uint32_t serial) {
  GetUserDataAs<ShellSurface>(resource)->Move();
}

void xdg_toplevel_v6_resize(wl_client* client,
                            wl_resource* resource,
                            wl_resource* seat,
                            uint32_t serial,
                            uint32_t edges) {
  int component = XdgToplevelV6ResizeComponent(edges);
  if (component != HTNOWHERE)
    GetUserDataAs<ShellSurface>(resource)->Resize(component);
}

void xdg_toplevel_v6_set_max_size(wl_client* client,
                                  wl_resource* resource,
                                  int32_t width,
                                  int32_t height) {
  NOTIMPLEMENTED();
}

void xdg_toplevel_v6_set_min_size(wl_client* client,
                                  wl_resource* resource,
                                  int32_t width,
                                  int32_t height) {
  NOTIMPLEMENTED();
}

void xdg_toplevel_v6_set_maximized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->Maximize();
}

void xdg_toplevel_v6_unset_maximized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->Restore();
}

void xdg_toplevel_v6_set_fullscreen(wl_client* client,
                                    wl_resource* resource,
                                    wl_resource* output) {
  GetUserDataAs<ShellSurface>(resource)->SetFullscreen(true);
}

void xdg_toplevel_v6_unset_fullscreen(wl_client* client,
                                      wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->SetFullscreen(false);
}

void xdg_toplevel_v6_set_minimized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->Minimize();
}

const struct zxdg_toplevel_v6_interface xdg_toplevel_v6_implementation = {
    xdg_toplevel_v6_destroy,          xdg_toplevel_v6_set_parent,
    xdg_toplevel_v6_set_title,        xdg_toplevel_v6_set_add_id,
    xdg_toplevel_v6_show_window_menu, xdg_toplevel_v6_move,
    xdg_toplevel_v6_resize,           xdg_toplevel_v6_set_max_size,
    xdg_toplevel_v6_set_min_size,     xdg_toplevel_v6_set_maximized,
    xdg_toplevel_v6_unset_maximized,  xdg_toplevel_v6_set_fullscreen,
    xdg_toplevel_v6_unset_fullscreen, xdg_toplevel_v6_set_minimized};

////////////////////////////////////////////////////////////////////////////////
// xdg_popup_interface:

void xdg_popup_v5_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct xdg_popup_interface xdg_popup_v5_implementation = {
    xdg_popup_v5_destroy};

void xdg_popup_v6_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_popup_v6_grab(wl_client* client,
                       wl_resource* resource,
                       wl_resource* seat,
                       uint32_t serial) {
  NOTIMPLEMENTED();
}

const struct zxdg_popup_v6_interface xdg_popup_v6_implementation = {
    xdg_popup_v6_destroy, xdg_popup_v6_grab};

////////////////////////////////////////////////////////////////////////////////
// xdg_surface_interface:

int XdgSurfaceV5ResizeComponent(uint32_t edges) {
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

void xdg_surface_v5_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_surface_v5_set_parent(wl_client* client,
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

void xdg_surface_v5_set_title(wl_client* client,
                              wl_resource* resource,
                              const char* title) {
  GetUserDataAs<ShellSurface>(resource)
      ->SetTitle(base::string16(base::UTF8ToUTF16(title)));
}

void xdg_surface_v5_set_add_id(wl_client* client,
                               wl_resource* resource,
                               const char* app_id) {
  GetUserDataAs<ShellSurface>(resource)->SetApplicationId(app_id);
}

void xdg_surface_v5_show_window_menu(wl_client* client,
                                     wl_resource* resource,
                                     wl_resource* seat,
                                     uint32_t serial,
                                     int32_t x,
                                     int32_t y) {
  NOTIMPLEMENTED();
}

void xdg_surface_v5_move(wl_client* client,
                         wl_resource* resource,
                         wl_resource* seat,
                         uint32_t serial) {
  GetUserDataAs<ShellSurface>(resource)->Move();
}

void xdg_surface_v5_resize(wl_client* client,
                           wl_resource* resource,
                           wl_resource* seat,
                           uint32_t serial,
                           uint32_t edges) {
  int component = XdgSurfaceV5ResizeComponent(edges);
  if (component != HTNOWHERE)
    GetUserDataAs<ShellSurface>(resource)->Resize(component);
}

void xdg_surface_v5_ack_configure(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t serial) {
  GetUserDataAs<ShellSurface>(resource)->AcknowledgeConfigure(serial);
}

void xdg_surface_v5_set_window_geometry(wl_client* client,
                                        wl_resource* resource,
                                        int32_t x,
                                        int32_t y,
                                        int32_t width,
                                        int32_t height) {
  GetUserDataAs<ShellSurface>(resource)
      ->SetGeometry(gfx::Rect(x, y, width, height));
}

void xdg_surface_v5_set_maximized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->Maximize();
}

void xdg_surface_v5_unset_maximized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->Restore();
}

void xdg_surface_v5_set_fullscreen(wl_client* client,
                                   wl_resource* resource,
                                   wl_resource* output) {
  GetUserDataAs<ShellSurface>(resource)->SetFullscreen(true);
}

void xdg_surface_v5_unset_fullscreen(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->SetFullscreen(false);
}

void xdg_surface_v5_set_minimized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->Minimize();
}

const struct xdg_surface_interface xdg_surface_v5_implementation = {
    xdg_surface_v5_destroy,
    xdg_surface_v5_set_parent,
    xdg_surface_v5_set_title,
    xdg_surface_v5_set_add_id,
    xdg_surface_v5_show_window_menu,
    xdg_surface_v5_move,
    xdg_surface_v5_resize,
    xdg_surface_v5_ack_configure,
    xdg_surface_v5_set_window_geometry,
    xdg_surface_v5_set_maximized,
    xdg_surface_v5_unset_maximized,
    xdg_surface_v5_set_fullscreen,
    xdg_surface_v5_unset_fullscreen,
    xdg_surface_v5_set_minimized};

void xdg_surface_v6_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void HandleXdgToplevelV6CloseCallback(wl_resource* resource) {
  zxdg_toplevel_v6_send_close(resource);
  wl_client_flush(wl_resource_get_client(resource));
}

void AddXdgToplevelV6State(wl_array* states, zxdg_toplevel_v6_state state) {
  zxdg_toplevel_v6_state* value = static_cast<zxdg_toplevel_v6_state*>(
      wl_array_add(states, sizeof(zxdg_toplevel_v6_state)));
  DCHECK(value);
  *value = state;
}

uint32_t HandleXdgToplevelV6ConfigureCallback(
    wl_resource* resource,
    wl_resource* surface_resource,
    const gfx::Size& size,
    ash::mojom::WindowStateType state_type,
    bool resizing,
    bool activated,
    const gfx::Vector2d& origin_offset) {
  wl_array states;
  wl_array_init(&states);
  if (state_type == ash::mojom::WindowStateType::MAXIMIZED)
    AddXdgToplevelV6State(&states, ZXDG_TOPLEVEL_V6_STATE_MAXIMIZED);
  if (state_type == ash::mojom::WindowStateType::FULLSCREEN)
    AddXdgToplevelV6State(&states, ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN);
  if (resizing)
    AddXdgToplevelV6State(&states, ZXDG_TOPLEVEL_V6_STATE_RESIZING);
  if (activated)
    AddXdgToplevelV6State(&states, ZXDG_TOPLEVEL_V6_STATE_ACTIVATED);
  zxdg_toplevel_v6_send_configure(resource, size.width(), size.height(),
                                  &states);
  uint32_t serial = wl_display_next_serial(
      wl_client_get_display(wl_resource_get_client(surface_resource)));
  zxdg_surface_v6_send_configure(surface_resource, serial);
  wl_client_flush(wl_resource_get_client(resource));
  wl_array_release(&states);
  return serial;
}

void xdg_surface_v6_get_toplevel(wl_client* client,
                                 wl_resource* resource,
                                 uint32_t id) {
  ShellSurface* shell_surface = GetUserDataAs<ShellSurface>(resource);
  if (shell_surface->enabled()) {
    wl_resource_post_error(resource, ZXDG_SURFACE_V6_ERROR_ALREADY_CONSTRUCTED,
                           "surface has already been constructed");
    return;
  }

  shell_surface->SetEnabled(true);

  wl_resource* xdg_toplevel_resource =
      wl_resource_create(client, &zxdg_toplevel_v6_interface, 1, id);

  shell_surface->set_close_callback(
      base::Bind(&HandleXdgToplevelV6CloseCallback,
                 base::Unretained(xdg_toplevel_resource)));

  shell_surface->set_configure_callback(base::Bind(
      &HandleXdgToplevelV6ConfigureCallback,
      base::Unretained(xdg_toplevel_resource), base::Unretained(resource)));

  wl_resource_set_implementation(xdg_toplevel_resource,
                                 &xdg_toplevel_v6_implementation, shell_surface,
                                 nullptr);
}

void HandleXdgPopupV6CloseCallback(wl_resource* resource) {
  zxdg_popup_v6_send_popup_done(resource);
  wl_client_flush(wl_resource_get_client(resource));
}

void xdg_surface_v6_get_popup(wl_client* client,
                              wl_resource* resource,
                              uint32_t id,
                              wl_resource* parent,
                              wl_resource* positioner) {
  ShellSurface* shell_surface = GetUserDataAs<ShellSurface>(resource);
  if (shell_surface->enabled()) {
    wl_resource_post_error(resource, ZXDG_SURFACE_V6_ERROR_ALREADY_CONSTRUCTED,
                           "surface has already been constructed");
    return;
  }

  shell_surface->SetEnabled(true);

  wl_resource* xdg_popup_resource =
      wl_resource_create(client, &zxdg_popup_v6_interface, 1, id);

  shell_surface->set_close_callback(base::Bind(
      &HandleXdgPopupV6CloseCallback, base::Unretained(xdg_popup_resource)));

  wl_resource_set_implementation(
      xdg_popup_resource, &xdg_popup_v6_implementation, shell_surface, nullptr);
}

void xdg_surface_v6_set_window_geometry(wl_client* client,
                                        wl_resource* resource,
                                        int32_t x,
                                        int32_t y,
                                        int32_t width,
                                        int32_t height) {
  GetUserDataAs<ShellSurface>(resource)->SetGeometry(
      gfx::Rect(x, y, width, height));
}

void xdg_surface_v6_ack_configure(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t serial) {
  GetUserDataAs<ShellSurface>(resource)->AcknowledgeConfigure(serial);
}

const struct zxdg_surface_v6_interface xdg_surface_v6_implementation = {
    xdg_surface_v6_destroy, xdg_surface_v6_get_toplevel,
    xdg_surface_v6_get_popup, xdg_surface_v6_set_window_geometry,
    xdg_surface_v6_ack_configure};

////////////////////////////////////////////////////////////////////////////////
// xdg_shell_interface:

void xdg_shell_v5_destroy(wl_client* client, wl_resource* resource) {
  // Nothing to do here.
}

// Currently implemented version of the unstable xdg-shell interface.
#define XDG_SHELL_VERSION 5
static_assert(XDG_SHELL_VERSION == XDG_SHELL_VERSION_CURRENT,
              "Interface version doesn't match implementation version");

void xdg_shell_v5_use_unstable_version(wl_client* client,
                                       wl_resource* resource,
                                       int32_t version) {
  if (version > XDG_SHELL_VERSION) {
    wl_resource_post_error(resource, 1,
                           "xdg-shell version not implemented yet.");
  }
}

void HandleXdgSurfaceV5CloseCallback(wl_resource* resource) {
  xdg_surface_send_close(resource);
  wl_client_flush(wl_resource_get_client(resource));
}

void AddXdgSurfaceV5State(wl_array* states, xdg_surface_state state) {
  xdg_surface_state* value = static_cast<xdg_surface_state*>(
      wl_array_add(states, sizeof(xdg_surface_state)));
  DCHECK(value);
  *value = state;
}

uint32_t HandleXdgSurfaceV5ConfigureCallback(
    wl_resource* resource,
    const gfx::Size& size,
    ash::mojom::WindowStateType state_type,
    bool resizing,
    bool activated,
    const gfx::Vector2d& origin_offset) {
  wl_array states;
  wl_array_init(&states);
  if (state_type == ash::mojom::WindowStateType::MAXIMIZED)
    AddXdgSurfaceV5State(&states, XDG_SURFACE_STATE_MAXIMIZED);
  if (state_type == ash::mojom::WindowStateType::FULLSCREEN)
    AddXdgSurfaceV5State(&states, XDG_SURFACE_STATE_FULLSCREEN);
  if (resizing)
    AddXdgSurfaceV5State(&states, XDG_SURFACE_STATE_RESIZING);
  if (activated)
    AddXdgSurfaceV5State(&states, XDG_SURFACE_STATE_ACTIVATED);
  uint32_t serial = wl_display_next_serial(
      wl_client_get_display(wl_resource_get_client(resource)));
  xdg_surface_send_configure(resource, size.width(), size.height(), &states,
                             serial);
  wl_client_flush(wl_resource_get_client(resource));
  wl_array_release(&states);
  return serial;
}

void xdg_shell_v5_get_xdg_surface(wl_client* client,
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

  shell_surface->set_close_callback(
      base::Bind(&HandleXdgSurfaceV5CloseCallback,
                 base::Unretained(xdg_surface_resource)));

  shell_surface->set_configure_callback(
      base::Bind(&HandleXdgSurfaceV5ConfigureCallback,
                 base::Unretained(xdg_surface_resource)));

  SetImplementation(xdg_surface_resource, &xdg_surface_v5_implementation,
                    std::move(shell_surface));
}

void HandleXdgPopupV5CloseCallback(wl_resource* resource) {
  xdg_popup_send_popup_done(resource);
  wl_client_flush(wl_resource_get_client(resource));
}

void xdg_shell_v5_get_xdg_popup(wl_client* client,
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
  aura::Window* parent_window = GetUserDataAs<Surface>(parent)->window();
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
      &HandleXdgPopupV5CloseCallback, base::Unretained(xdg_popup_resource)));

  SetImplementation(xdg_popup_resource, &xdg_popup_v5_implementation,
                    std::move(shell_surface));
}

void xdg_shell_v5_pong(wl_client* client,
                       wl_resource* resource,
                       uint32_t serial) {
  NOTIMPLEMENTED();
}

const struct xdg_shell_interface xdg_shell_v5_implementation = {
    xdg_shell_v5_destroy, xdg_shell_v5_use_unstable_version,
    xdg_shell_v5_get_xdg_surface, xdg_shell_v5_get_xdg_popup,
    xdg_shell_v5_pong};

void bind_xdg_shell_v5(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &xdg_shell_interface, 1, id);

  wl_resource_set_implementation(resource, &xdg_shell_v5_implementation, data,
                                 nullptr);
}

void xdg_shell_v6_destroy(wl_client* client, wl_resource* resource) {
  // Nothing to do here.
}

void xdg_shell_v6_create_positioner(wl_client* client,
                                    wl_resource* resource,
                                    uint32_t id) {
  wl_resource* xdg_positioner_resource =
      wl_resource_create(client, &zxdg_positioner_v6_interface, 1, id);

  wl_resource_set_implementation(xdg_positioner_resource,
                                 &xdg_positioner_v6_implementation, nullptr,
                                 nullptr);
}

void xdg_shell_v6_get_xdg_surface(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t id,
                                  wl_resource* surface) {
  std::unique_ptr<ShellSurface> shell_surface =
      GetUserDataAs<Display>(resource)->CreateShellSurface(
          GetUserDataAs<Surface>(surface));
  if (!shell_surface) {
    wl_resource_post_error(resource, ZXDG_SHELL_V6_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  // Xdg shell v6 surfaces are initially disabled and needs to be explicitly
  // mapped before they are enabled and can become visible.
  shell_surface->SetEnabled(false);

  wl_resource* xdg_surface_resource =
      wl_resource_create(client, &zxdg_surface_v6_interface, 1, id);

  SetImplementation(xdg_surface_resource, &xdg_surface_v6_implementation,
                    std::move(shell_surface));
}

void xdg_shell_v6_pong(wl_client* client,
                       wl_resource* resource,
                       uint32_t serial) {
  NOTIMPLEMENTED();
}

const struct zxdg_shell_v6_interface xdg_shell_v6_implementation = {
    xdg_shell_v6_destroy, xdg_shell_v6_create_positioner,
    xdg_shell_v6_get_xdg_surface, xdg_shell_v6_pong};

void bind_xdg_shell_v6(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zxdg_shell_v6_interface, 1, id);

  wl_resource_set_implementation(resource, &xdg_shell_v6_implementation, data,
                                 nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// remote_surface_interface:

void remote_surface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void remote_surface_set_app_id(wl_client* client,
                               wl_resource* resource,
                               const char* app_id) {
  GetUserDataAs<ShellSurface>(resource)->SetApplicationId(app_id);
}

void remote_surface_set_window_geometry(wl_client* client,
                                        wl_resource* resource,
                                        int32_t x,
                                        int32_t y,
                                        int32_t width,
                                        int32_t height) {
  GetUserDataAs<ShellSurface>(resource)->SetGeometry(
      gfx::Rect(x, y, width, height));
}

void remote_surface_set_orientation(wl_client* client,
                                    wl_resource* resource,
                                    int32_t orientation) {
  GetUserDataAs<ShellSurface>(resource)->SetOrientation(
      orientation == ZCR_REMOTE_SURFACE_V1_ORIENTATION_PORTRAIT
          ? Orientation::PORTRAIT
          : Orientation::LANDSCAPE);
}

void remote_surface_set_scale(wl_client* client,
                              wl_resource* resource,
                              wl_fixed_t scale) {
  GetUserDataAs<ShellSurface>(resource)->SetScale(wl_fixed_to_double(scale));
}

void remote_surface_set_rectangular_shadow_DEPRECATED(wl_client* client,
                                                      wl_resource* resource,
                                                      int32_t x,
                                                      int32_t y,
                                                      int32_t width,
                                                      int32_t height) {
  NOTIMPLEMENTED();
}

void remote_surface_set_rectangular_shadow_background_opacity(
    wl_client* client,
    wl_resource* resource,
    wl_fixed_t opacity) {
  GetUserDataAs<ShellSurface>(resource)->SetRectangularShadowBackgroundOpacity(
      wl_fixed_to_double(opacity));
}

void remote_surface_set_title(wl_client* client,
                              wl_resource* resource,
                              const char* title) {
  GetUserDataAs<ShellSurface>(resource)->SetTitle(
      base::string16(base::UTF8ToUTF16(title)));
}

void remote_surface_set_top_inset(wl_client* client,
                                  wl_resource* resource,
                                  int32_t height) {
  GetUserDataAs<ShellSurface>(resource)->SetTopInset(height);
}

void remote_surface_activate(wl_client* client,
                             wl_resource* resource,
                             uint32_t serial) {
  ShellSurface* shell_surface = GetUserDataAs<ShellSurface>(resource);
  shell_surface->Activate();
}

void remote_surface_maximize(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->Maximize();
}

void remote_surface_minimize(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->Minimize();
}

void remote_surface_restore(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->Restore();
}

void remote_surface_fullscreen(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->SetFullscreen(true);
}

void remote_surface_unfullscreen(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->SetFullscreen(false);
}

void remote_surface_pin(wl_client* client,
                        wl_resource* resource,
                        int32_t trusted) {
  GetUserDataAs<ShellSurface>(resource)->SetPinned(
      trusted ? ash::mojom::WindowPinType::TRUSTED_PINNED
              : ash::mojom::WindowPinType::PINNED);
}

void remote_surface_unpin(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->SetPinned(
      ash::mojom::WindowPinType::NONE);
}

void remote_surface_set_system_modal(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->SetSystemModal(true);
}

void remote_surface_unset_system_modal(wl_client* client,
                                       wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->SetSystemModal(false);
}

void remote_surface_set_rectangular_surface_shadow(wl_client* client,
                                                   wl_resource* resource,
                                                   int32_t x,
                                                   int32_t y,
                                                   int32_t width,
                                                   int32_t height) {
  ShellSurface* shell_surface = GetUserDataAs<ShellSurface>(resource);
  shell_surface->SetShadowBounds(gfx::Rect(x, y, width, height));
}

void remote_surface_set_systemui_visibility(wl_client* client,
                                            wl_resource* resource,
                                            uint32_t visibility) {
  GetUserDataAs<ShellSurface>(resource)->SetSystemUiVisibility(
      visibility != ZCR_REMOTE_SURFACE_V1_SYSTEMUI_VISIBILITY_STATE_VISIBLE);
}

void remote_surface_set_always_on_top(wl_client* client,
                                      wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->SetAlwaysOnTop(true);
}

void remote_surface_unset_always_on_top(wl_client* client,
                                        wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->SetAlwaysOnTop(false);
}

void remote_surface_ack_configure(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t serial) {
  GetUserDataAs<ShellSurface>(resource)->AcknowledgeConfigure(serial);
}

void remote_surface_move(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->Move();
}

void remote_surface_set_window_type(wl_client* client,
                                    wl_resource* resource,
                                    uint32_t type) {
  if (type == ZCR_REMOTE_SURFACE_V1_WINDOW_TYPE_SYSTEM_UI) {
    auto* widget = GetUserDataAs<ShellSurface>(resource)->GetWidget();
    if (widget) {
      widget->GetNativeWindow()->SetProperty(ash::kShowInOverviewKey, false);

      wm::SetWindowVisibilityAnimationType(
          widget->GetNativeWindow(), wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
    }
  }
}

const struct zcr_remote_surface_v1_interface remote_surface_implementation = {
    remote_surface_destroy,
    remote_surface_set_app_id,
    remote_surface_set_window_geometry,
    remote_surface_set_scale,
    remote_surface_set_rectangular_shadow_DEPRECATED,
    remote_surface_set_rectangular_shadow_background_opacity,
    remote_surface_set_title,
    remote_surface_set_top_inset,
    remote_surface_activate,
    remote_surface_maximize,
    remote_surface_minimize,
    remote_surface_restore,
    remote_surface_fullscreen,
    remote_surface_unfullscreen,
    remote_surface_pin,
    remote_surface_unpin,
    remote_surface_set_system_modal,
    remote_surface_unset_system_modal,
    remote_surface_set_rectangular_surface_shadow,
    remote_surface_set_systemui_visibility,
    remote_surface_set_always_on_top,
    remote_surface_unset_always_on_top,
    remote_surface_ack_configure,
    remote_surface_move,
    remote_surface_set_orientation,
    remote_surface_set_window_type};

////////////////////////////////////////////////////////////////////////////////
// notification_surface_interface:

void notification_surface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_notification_surface_v1_interface
    notification_surface_implementation = {notification_surface_destroy};

////////////////////////////////////////////////////////////////////////////////
// remote_shell_interface:

// Implements remote shell interface and monitors workspace state needed
// for the remote shell interface.
class WaylandRemoteShell : public WMHelper::TabletModeObserver,
                           public WMHelper::ActivationObserver,
                           public display::DisplayObserver {
 public:
  WaylandRemoteShell(Display* display, wl_resource* remote_shell_resource)
      : display_(display),
        remote_shell_resource_(remote_shell_resource),
        weak_ptr_factory_(this) {
    auto* helper = WMHelper::GetInstance();
    helper->AddTabletModeObserver(this);
    helper->AddActivationObserver(this);
    display::Screen::GetScreen()->AddObserver(this);

    layout_mode_ = helper->IsTabletModeWindowManagerEnabled()
                       ? ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_TABLET
                       : ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_WINDOWED;

    if (wl_resource_get_version(remote_shell_resource_) >= 8) {
      double scale_factor = GetDefaultDeviceScaleFactor();
      // Send using 16.16 fixed point.
      const int kDecimalBits = 24;
      int32_t fixed_scale =
          static_cast<int32_t>(scale_factor * (1 << kDecimalBits));
      zcr_remote_shell_v1_send_default_device_scale_factor(
          remote_shell_resource_, fixed_scale);
    }

    SendDisplayMetrics();
    SendActivated(helper->GetActiveWindow(), nullptr);
  }
  ~WaylandRemoteShell() override {
    auto* helper = WMHelper::GetInstance();
    helper->RemoveTabletModeObserver(this);
    helper->RemoveActivationObserver(this);
    display::Screen::GetScreen()->RemoveObserver(this);
  }

  bool IsMultiDisplaySupported() const {
    return wl_resource_get_version(remote_shell_resource_) >= 5;
  }

  std::unique_ptr<ShellSurface> CreateShellSurface(
      Surface* surface,
      int container,
      double default_device_scale_factor) {
    return display_->CreateRemoteShellSurface(surface, container,
                                              default_device_scale_factor);
  }

  std::unique_ptr<NotificationSurface> CreateNotificationSurface(
      Surface* surface,
      const std::string& notification_key) {
    return display_->CreateNotificationSurface(surface, notification_key);
  }

  // Overridden from display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override {
    if (IsMultiDisplaySupported())
      ScheduleSendDisplayMetrics(0);
  }

  void OnDisplayRemoved(const display::Display& old_display) override {
    if (IsMultiDisplaySupported())
      ScheduleSendDisplayMetrics(0);
  }

  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override {
    if (!IsMultiDisplaySupported() &&
        display::Screen::GetScreen()->GetPrimaryDisplay().id() != display.id())
      return;

    // No need to update when a primary display has changed without bounds
    // change. See WaylandPrimaryDisplayObserver::OnDisplayMetricsChanged
    // for more details.
    if (changed_metrics &
        (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
         DISPLAY_METRIC_ROTATION | DISPLAY_METRIC_WORK_AREA)) {
      ScheduleSendDisplayMetrics(0);
    }
  }

  // Overridden from WMHelper::TabletModeObserver:
  void OnTabletModeStarted() override {
    layout_mode_ = ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_TABLET;
    ScheduleSendDisplayMetrics(kConfigureDelayAfterLayoutSwitchMs);
  }
  void OnTabletModeEnding() override {
    layout_mode_ = ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_WINDOWED;
    ScheduleSendDisplayMetrics(kConfigureDelayAfterLayoutSwitchMs);
  }
  void OnTabletModeEnded() override {}

  // Overridden from WMHelper::ActivationObserver:
  void OnWindowActivated(aura::Window* gained_active,
                         aura::Window* lost_active) override {
    SendActivated(gained_active, lost_active);
  }

 private:
  void ScheduleSendDisplayMetrics(int delay_ms) {
    needs_send_display_metrics_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&WaylandRemoteShell::SendDisplayMetrics,
                              weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(delay_ms));
  }

  // Returns the transform that a display's output is currently adjusted for.
  wl_output_transform DisplayTransform(display::Display::Rotation rotation) {
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

  void SendDisplayMetrics() {
    if (!needs_send_display_metrics_)
      return;
    needs_send_display_metrics_ = false;

    const display::Screen* screen = display::Screen::GetScreen();

    if (IsMultiDisplaySupported()) {
      for (const auto& display : screen->GetAllDisplays()) {
        const gfx::Rect& bounds = display.bounds();
        const gfx::Insets& insets = display.GetWorkAreaInsets();

        double device_scale_factor =
            WMHelper::GetInstance()->GetDisplayInfo(display.id())
                .device_scale_factor();

        zcr_remote_shell_v1_send_workspace(
            remote_shell_resource_, static_cast<uint32_t>(display.id() >> 32),
            static_cast<uint32_t>(display.id()), bounds.x(), bounds.y(),
            bounds.width(), bounds.height(), insets.left(), insets.top(),
            insets.right(), insets.bottom(),
            DisplayTransform(display.rotation()),
            wl_fixed_from_double(device_scale_factor), display.IsInternal());
      }

      zcr_remote_shell_v1_send_configure(remote_shell_resource_, layout_mode_);
    }

    display::Display primary_display = screen->GetPrimaryDisplay();
    const gfx::Insets& insets = primary_display.GetWorkAreaInsets();

    zcr_remote_shell_v1_send_configuration_changed(
        remote_shell_resource_, primary_display.size().width(),
        primary_display.size().height(),
        DisplayTransform(primary_display.rotation()),
        wl_fixed_from_double(primary_display.device_scale_factor()),
        insets.left(), insets.top(), insets.right(), insets.bottom(),
        layout_mode_);
    wl_client_flush(wl_resource_get_client(remote_shell_resource_));
  }

  void SendActivated(aura::Window* gained_active, aura::Window* lost_active) {
    Surface* gained_active_surface =
        gained_active ? ShellSurface::GetMainSurface(gained_active) : nullptr;
    Surface* lost_active_surface =
        lost_active ? ShellSurface::GetMainSurface(lost_active) : nullptr;
    wl_resource* gained_active_surface_resource =
        gained_active_surface ? GetSurfaceResource(gained_active_surface)
                              : nullptr;
    wl_resource* lost_active_surface_resource =
        lost_active_surface ? GetSurfaceResource(lost_active_surface) : nullptr;

    wl_client* client = wl_resource_get_client(remote_shell_resource_);

    // If surface that gained active is not owned by remote shell client then
    // set it to null.
    if (gained_active_surface_resource &&
        wl_resource_get_client(gained_active_surface_resource) != client) {
      gained_active_surface_resource = nullptr;
    }

    // If surface that lost active is not owned by remote shell client then
    // set it to null.
    if (lost_active_surface_resource &&
        wl_resource_get_client(lost_active_surface_resource) != client) {
      lost_active_surface_resource = nullptr;
    }

    zcr_remote_shell_v1_send_activated(remote_shell_resource_,
                                       gained_active_surface_resource,
                                       lost_active_surface_resource);
    wl_client_flush(client);
  }

  // The exo display instance. Not owned.
  Display* const display_;

  // The remote shell resource associated with observer.
  wl_resource* const remote_shell_resource_;

  bool needs_send_display_metrics_ = true;

  int layout_mode_ = ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_WINDOWED;

  base::WeakPtrFactory<WaylandRemoteShell> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WaylandRemoteShell);
};

void remote_shell_destroy(wl_client* client, wl_resource* resource) {
  // Nothing to do here.
}

int RemoteSurfaceContainer(uint32_t container) {
  switch (container) {
    case ZCR_REMOTE_SHELL_V1_CONTAINER_DEFAULT:
      return ash::kShellWindowId_DefaultContainer;
    case ZCR_REMOTE_SHELL_V1_CONTAINER_OVERLAY:
      return ash::kShellWindowId_SystemModalContainer;
    default:
      DLOG(WARNING) << "Unsupported container: " << container;
      return ash::kShellWindowId_DefaultContainer;
  }
}

void HandleRemoteSurfaceCloseCallback(wl_resource* resource) {
  zcr_remote_surface_v1_send_close(resource);
  wl_client_flush(wl_resource_get_client(resource));
}

void HandleRemoteSurfaceStateChangedCallback(
    wl_resource* resource,
    ash::mojom::WindowStateType old_state_type,
    ash::mojom::WindowStateType new_state_type) {
  DCHECK_NE(old_state_type, new_state_type);

  uint32_t state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_NORMAL;
  switch (new_state_type) {
    case ash::mojom::WindowStateType::MINIMIZED:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_MINIMIZED;
      break;
    case ash::mojom::WindowStateType::MAXIMIZED:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_MAXIMIZED;
      break;
    case ash::mojom::WindowStateType::FULLSCREEN:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_FULLSCREEN;
      break;
    case ash::mojom::WindowStateType::PINNED:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_PINNED;
      break;
    case ash::mojom::WindowStateType::TRUSTED_PINNED:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_TRUSTED_PINNED;
      break;
    default:
      break;
  }

  zcr_remote_surface_v1_send_state_type_changed(resource, state_type);
  wl_client_flush(wl_resource_get_client(resource));
}

uint32_t HandleRemoteSurfaceConfigureCallback(
    wl_resource* resource,
    const gfx::Size& size,
    ash::mojom::WindowStateType state_type,
    bool resizing,
    bool activated,
    const gfx::Vector2d& origin_offset) {
  wl_array states;
  wl_array_init(&states);
  uint32_t serial = wl_display_next_serial(
      wl_client_get_display(wl_resource_get_client(resource)));
  zcr_remote_surface_v1_send_configure(resource,
                                       origin_offset.x(),
                                       origin_offset.y(),
                                       &states, serial);
  wl_client_flush(wl_resource_get_client(resource));
  wl_array_release(&states);
  return serial;
}

void remote_shell_get_remote_surface(wl_client* client,
                                     wl_resource* resource,
                                     uint32_t id,
                                     wl_resource* surface,
                                     uint32_t container) {
  WaylandRemoteShell* shell = GetUserDataAs<WaylandRemoteShell>(resource);
  double default_scale_factor = wl_resource_get_version(resource) >= 8
                                    ? GetDefaultDeviceScaleFactor()
                                    : 1.0;

  std::unique_ptr<ShellSurface> shell_surface = shell->CreateShellSurface(
      GetUserDataAs<Surface>(surface), RemoteSurfaceContainer(container),
      default_scale_factor);
  if (!shell_surface) {
    wl_resource_post_error(resource, ZCR_REMOTE_SHELL_V1_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  wl_resource* remote_surface_resource =
      wl_resource_create(client, &zcr_remote_surface_v1_interface,
                         wl_resource_get_version(resource), id);

  shell_surface->set_close_callback(
      base::Bind(&HandleRemoteSurfaceCloseCallback,
                 base::Unretained(remote_surface_resource)));
  shell_surface->set_state_changed_callback(
      base::Bind(&HandleRemoteSurfaceStateChangedCallback,
                 base::Unretained(remote_surface_resource)));
  if (shell->IsMultiDisplaySupported()) {
    shell_surface->set_configure_callback(
        base::Bind(&HandleRemoteSurfaceConfigureCallback,
                   base::Unretained(remote_surface_resource)));
  }

  SetImplementation(remote_surface_resource, &remote_surface_implementation,
                    std::move(shell_surface));
}

void remote_shell_get_notification_surface(wl_client* client,
                                           wl_resource* resource,
                                           uint32_t id,
                                           wl_resource* surface,
                                           const char* notification_key) {
  if (GetUserDataAs<Surface>(surface)->HasSurfaceDelegate()) {
    wl_resource_post_error(resource, ZCR_REMOTE_SHELL_V1_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  std::unique_ptr<NotificationSurface> notification_surface =
      GetUserDataAs<WaylandRemoteShell>(resource)->CreateNotificationSurface(
          GetUserDataAs<Surface>(surface), std::string(notification_key));
  if (!notification_surface) {
    wl_resource_post_error(resource,
                           ZCR_REMOTE_SHELL_V1_ERROR_INVALID_NOTIFICATION_KEY,
                           "invalid notification key");
    return;
  }

  wl_resource* notification_surface_resource =
      wl_resource_create(client, &zcr_notification_surface_v1_interface,
                         wl_resource_get_version(resource), id);
  SetImplementation(notification_surface_resource,
                    &notification_surface_implementation,
                    std::move(notification_surface));
}

const struct zcr_remote_shell_v1_interface remote_shell_implementation = {
    remote_shell_destroy, remote_shell_get_remote_surface,
    remote_shell_get_notification_surface};

const uint32_t remote_shell_version = 8;

void bind_remote_shell(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_remote_shell_v1_interface,
                         std::min(version, remote_shell_version), id);

  SetImplementation(resource, &remote_shell_implementation,
                    std::make_unique<WaylandRemoteShell>(
                        static_cast<Display*>(data), resource));
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

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override {
    surface->RemoveSurfaceObserver(this);
    surface_ = nullptr;
  }

 private:
  Surface* surface_;

  DISALLOW_COPY_AND_ASSIGN(AuraSurface);
};

SurfaceFrameType ToSurfaceFrameType(uint32_t frame_type) {
  switch (frame_type) {
    case ZAURA_SURFACE_FRAME_TYPE_NONE:
      return SurfaceFrameType::NONE;
    case ZAURA_SURFACE_FRAME_TYPE_NORMAL:
      return SurfaceFrameType::NORMAL;
    case ZAURA_SURFACE_FRAME_TYPE_SHADOW:
      return SurfaceFrameType::SHADOW;
    default:
      DLOG(WARNING) << "Unsupported frame type: " << frame_type;
      return SurfaceFrameType::NONE;
  }
}

void aura_surface_set_frame(wl_client* client, wl_resource* resource,
                            uint32_t type) {
  GetUserDataAs<AuraSurface>(resource)->SetFrame(ToSurfaceFrameType(type));
}

const struct zaura_surface_interface aura_surface_implementation = {
    aura_surface_set_frame};

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

  wl_resource* aura_surface_resource =
      wl_resource_create(client, &zaura_surface_interface, 1, id);

  SetImplementation(aura_surface_resource, &aura_surface_implementation,
                    std::make_unique<AuraSurface>(surface));
}

const struct zaura_shell_interface aura_shell_implementation = {
    aura_shell_get_aura_surface};

void bind_aura_shell(wl_client* client,
                     void* data,
                     uint32_t version,
                     uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zaura_shell_interface, 1, id);

  wl_resource_set_implementation(resource, &aura_shell_implementation,
                                 nullptr, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// vsync_timing_interface:

// Implements VSync timing interface by monitoring updates to VSync parameters.
class VSyncTiming final : public WMHelper::VSyncObserver {
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

  // Overridden from WMHelper::VSyncObserver:
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
  }
  void OnSend(const std::string& mime_type, base::ScopedFD fd) override {
    wl_data_source_send_send(data_source_resource_, mime_type.c_str(),
                             fd.get());
  }
  void OnCancelled() override {
    wl_data_source_send_cancelled(data_source_resource_);
  }
  void OnDndDropPerformed() override {
    wl_data_source_send_dnd_drop_performed(data_source_resource_);
  }
  void OnDndFinished() override {
    wl_data_source_send_dnd_finished(data_source_resource_);
  }
  void OnAction(DndAction dnd_action) override {
    wl_data_source_send_action(data_source_resource_,
                               WaylandDataDeviceManagerDndAction(dnd_action));
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
  }
  void OnSourceActions(
      const base::flat_set<DndAction>& source_actions) override {
    wl_data_offer_send_source_actions(
        data_offer_resource_,
        WaylandDataDeviceManagerDndActions(source_actions));
  }
  void OnAction(DndAction action) override {
    wl_data_offer_send_action(data_offer_resource_,
                              WaylandDataDeviceManagerDndAction(action));
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
        wl_resource_create(client_, &wl_data_offer_interface, 1, 0);
    std::unique_ptr<DataOffer> data_offer = std::make_unique<DataOffer>(
        new WaylandDataOfferDelegate(data_offer_resource));
    data_offer->SetProperty(kDataOfferResourceKey, data_offer_resource);
    SetImplementation(data_offer_resource, &data_offer_implementation,
                      std::move(data_offer));

    wl_data_device_send_data_offer(data_device_resource_, data_offer_resource);

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
  }
  void OnLeave() override { wl_data_device_send_leave(data_device_resource_); }
  void OnMotion(base::TimeTicks time_stamp, const gfx::PointF& point) override {
    wl_data_device_send_motion(
        data_device_resource_, TimeTicksToMilliseconds(time_stamp),
        wl_fixed_from_double(point.x()), wl_fixed_from_double(point.y()));
  }
  void OnDrop() override { wl_data_device_send_drop(data_device_resource_); }
  void OnSelection(const DataOffer& data_offer) override {
    wl_data_device_send_selection(data_device_resource_,
                                  GetDataOfferResource(&data_offer));
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
      GetUserDataAs<DataSource>(data_source), serial);
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
  wl_resource* data_source_resource =
      wl_resource_create(client, &wl_data_device_interface, 1, id);
  SetImplementation(data_source_resource, &data_source_implementation,
                    std::make_unique<DataSource>(
                        new WaylandDataSourceDelegate(data_source_resource)));
}

void data_device_manager_get_data_device(wl_client* client,
                                         wl_resource* resource,
                                         uint32_t id,
                                         wl_resource* seat_resource) {
  Display* display = GetUserDataAs<Display>(resource);
  wl_resource* data_device_resource =
      wl_resource_create(client, &wl_data_device_interface, 1, id);
  SetImplementation(data_device_resource, &data_device_implementation,
                    display->CreateDataDevice(new WaylandDataDeviceDelegate(
                        client, data_device_resource)));
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
    wl_resource* surface_resource = GetSurfaceResource(surface);
    // We can accept events for this surface if the client is the same as the
    // pointer.
    return surface_resource &&
           wl_resource_get_client(surface_resource) == client();
  }
  void OnPointerEnter(Surface* surface,
                      const gfx::PointF& location,
                      int button_flags) override {
    wl_resource* surface_resource = GetSurfaceResource(surface);
    DCHECK(surface_resource);
    // Should we be sending button events to the client before the enter event
    // if client's pressed button state is different from |button_flags|?
    wl_pointer_send_enter(pointer_resource_, next_serial(), surface_resource,
                          wl_fixed_from_double(location.x()),
                          wl_fixed_from_double(location.y()));
  }
  void OnPointerLeave(Surface* surface) override {
    wl_resource* surface_resource = GetSurfaceResource(surface);
    DCHECK(surface_resource);
    wl_pointer_send_leave(pointer_resource_, next_serial(), surface_resource);
  }
  void OnPointerMotion(base::TimeTicks time_stamp,
                       const gfx::PointF& location) override {
    wl_pointer_send_motion(
        pointer_resource_, TimeTicksToMilliseconds(time_stamp),
        wl_fixed_from_double(location.x()), wl_fixed_from_double(location.y()));
  }
  void OnPointerButton(base::TimeTicks time_stamp,
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
        wl_pointer_send_button(
            pointer_resource_, serial, TimeTicksToMilliseconds(time_stamp),
            button.value, pressed ? WL_POINTER_BUTTON_STATE_PRESSED
                                  : WL_POINTER_BUTTON_STATE_RELEASED);
      }
    }
  }
  void OnPointerScroll(base::TimeTicks time_stamp,
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
    wl_pointer_send_axis(pointer_resource_, TimeTicksToMilliseconds(time_stamp),
                         WL_POINTER_AXIS_HORIZONTAL_SCROLL,
                         wl_fixed_from_double(-x_value));

    double y_value = offset.y() * kAxisStepDistance;
    wl_pointer_send_axis(pointer_resource_, TimeTicksToMilliseconds(time_stamp),
                         WL_POINTER_AXIS_VERTICAL_SCROLL,
                         wl_fixed_from_double(-y_value));
  }
  void OnPointerScrollStop(base::TimeTicks time_stamp) override {
    if (wl_resource_get_version(pointer_resource_) >=
        WL_POINTER_AXIS_STOP_SINCE_VERSION) {
      wl_pointer_send_axis_stop(pointer_resource_,
                                TimeTicksToMilliseconds(time_stamp),
                                WL_POINTER_AXIS_HORIZONTAL_SCROLL);
      wl_pointer_send_axis_stop(pointer_resource_,
                                TimeTicksToMilliseconds(time_stamp),
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

#if BUILDFLAG(USE_XKBCOMMON)

////////////////////////////////////////////////////////////////////////////////
// wl_keyboard_interface:

// Keyboard delegate class that accepts events for surfaces owned by the same
// client as a keyboard resource.
class WaylandKeyboardDelegate
    : public KeyboardDelegate,
      public KeyboardObserver
#if defined(OS_CHROMEOS)
    ,
      public chromeos::input_method::ImeKeyboard::Observer
#endif
{
 public:
  explicit WaylandKeyboardDelegate(wl_resource* keyboard_resource)
      : keyboard_resource_(keyboard_resource),
        xkb_context_(xkb_context_new(XKB_CONTEXT_NO_FLAGS)) {
#if defined(OS_CHROMEOS)
    chromeos::input_method::ImeKeyboard* keyboard =
        chromeos::input_method::InputMethodManager::Get()->GetImeKeyboard();
    if (keyboard) {
      keyboard->AddObserver(this);
      SendNamedLayout(keyboard->GetCurrentKeyboardLayoutName());
    }
#else
    SendLayout(nullptr);
#endif
  }
#if defined(OS_CHROMEOS)
  ~WaylandKeyboardDelegate() override {
    chromeos::input_method::ImeKeyboard* keyboard =
        chromeos::input_method::InputMethodManager::Get()->GetImeKeyboard();
    if (keyboard)
      keyboard->RemoveObserver(this);
  }
#endif

  // Overridden from KeyboardDelegate:
  void OnKeyboardDestroying(Keyboard* keyboard) override { delete this; }
  bool CanAcceptKeyboardEventsForSurface(Surface* surface) const override {
    wl_resource* surface_resource = GetSurfaceResource(surface);
    // We can accept events for this surface if the client is the same as the
    // keyboard.
    return surface_resource &&
           wl_resource_get_client(surface_resource) == client();
  }
  void OnKeyboardEnter(Surface* surface,
                       const std::vector<ui::DomCode>& pressed_keys) override {
    wl_resource* surface_resource = GetSurfaceResource(surface);
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
    wl_resource* surface_resource = GetSurfaceResource(surface);
    DCHECK(surface_resource);
    wl_keyboard_send_leave(keyboard_resource_, next_serial(), surface_resource);
    wl_client_flush(client());
  }
  uint32_t OnKeyboardKey(base::TimeTicks time_stamp,
                         ui::DomCode key,
                         bool pressed) override {
    uint32_t serial = next_serial();
    wl_keyboard_send_key(keyboard_resource_, serial,
                         TimeTicksToMilliseconds(time_stamp), DomCodeToKey(key),
                         pressed ? WL_KEYBOARD_KEY_STATE_PRESSED
                                 : WL_KEYBOARD_KEY_STATE_RELEASED);
    wl_client_flush(client());
    return serial;
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

#if defined(OS_CHROMEOS)
  // Overridden from input_method::ImeKeyboard::Observer:
  void OnCapsLockChanged(bool enabled) override {}
  void OnLayoutChanging(const std::string& layout_name) override {
    SendNamedLayout(layout_name);
  }
#endif

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

#if defined(OS_CHROMEOS)
  // Send the named keyboard layout to the client.
  void SendNamedLayout(const std::string& layout_name) {
    std::string layout_id, layout_variant;
    ui::XkbKeyboardLayoutEngine::ParseLayoutName(layout_name, &layout_id,
                                                 &layout_variant);
    xkb_rule_names names = {.rules = nullptr,
                            .model = "pc101",
                            .layout = layout_id.c_str(),
                            .variant = layout_variant.c_str(),
                            .options = ""};
    SendLayout(&names);
  }
#endif

  // Send the keyboard layout named by XKB rules to the client.
  void SendLayout(const xkb_rule_names* names) {
    xkb_keymap_.reset(xkb_keymap_new_from_names(xkb_context_.get(), names,
                                                XKB_KEYMAP_COMPILE_NO_FLAGS));
    xkb_state_.reset(xkb_state_new(xkb_keymap_.get()));
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
                            shared_keymap.handle().GetHandle(), keymap_size);
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
    wl_resource* surface_resource = GetSurfaceResource(surface);
    // We can accept events for this surface if the client is the same as the
    // touch resource.
    return surface_resource &&
           wl_resource_get_client(surface_resource) == client();
  }
  void OnTouchDown(Surface* surface,
                   base::TimeTicks time_stamp,
                   int id,
                   const gfx::PointF& location) override {
    wl_resource* surface_resource = GetSurfaceResource(surface);
    DCHECK(surface_resource);
    wl_touch_send_down(touch_resource_, next_serial(),
                       TimeTicksToMilliseconds(time_stamp), surface_resource,
                       id, wl_fixed_from_double(location.x()),
                       wl_fixed_from_double(location.y()));
  }
  void OnTouchUp(base::TimeTicks time_stamp, int id) override {
    wl_touch_send_up(touch_resource_, next_serial(),
                     TimeTicksToMilliseconds(time_stamp), id);
  }
  void OnTouchMotion(base::TimeTicks time_stamp,
                     int id,
                     const gfx::PointF& location) override {
    wl_touch_send_motion(touch_resource_, TimeTicksToMilliseconds(time_stamp),
                         id, wl_fixed_from_double(location.x()),
                         wl_fixed_from_double(location.y()));
  }
  void OnTouchShape(int id, float major, float minor) override {
    if (wl_resource_get_version(touch_resource_) >=
        WL_TOUCH_SHAPE_SINCE_VERSION) {
      wl_touch_send_shape(touch_resource_, id, wl_fixed_from_double(major),
                          wl_fixed_from_double(minor));
    }
  }
  void OnTouchFrame() override {
    if (wl_resource_get_version(touch_resource_) >=
        WL_TOUCH_FRAME_SINCE_VERSION) {
      wl_touch_send_frame(touch_resource_);
    }
    wl_client_flush(client());
  }
  void OnTouchCancel() override {
    wl_touch_send_cancel(touch_resource_);
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

  SetImplementation(
      pointer_resource, &pointer_implementation,
      std::make_unique<Pointer>(new WaylandPointerDelegate(pointer_resource)));
}

void seat_get_keyboard(wl_client* client, wl_resource* resource, uint32_t id) {
#if BUILDFLAG(USE_XKBCOMMON)
  uint32_t version = wl_resource_get_version(resource);
  wl_resource* keyboard_resource =
      wl_resource_create(client, &wl_keyboard_interface, version, id);

  WaylandKeyboardDelegate* delegate =
      new WaylandKeyboardDelegate(keyboard_resource);
  std::unique_ptr<Keyboard> keyboard = std::make_unique<Keyboard>(delegate);
  keyboard->AddObserver(delegate);
  SetImplementation(keyboard_resource, &keyboard_implementation,
                    std::move(keyboard));

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
      std::make_unique<Touch>(new WaylandTouchDelegate(touch_resource)));
}

void seat_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_seat_interface seat_implementation = {
    seat_get_pointer, seat_get_keyboard, seat_get_touch, seat_release};

const uint32_t seat_version = 6;

void bind_seat(wl_client* client, void* data, uint32_t version, uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &wl_seat_interface, std::min(version, seat_version), id);

  wl_resource_set_implementation(resource, &seat_implementation, data, nullptr);

  if (version >= WL_SEAT_NAME_SINCE_VERSION)
    wl_seat_send_name(resource, "default");

  uint32_t capabilities = WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_TOUCH;
#if BUILDFLAG(USE_XKBCOMMON)
  capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
#endif
  wl_seat_send_capabilities(resource, capabilities);
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

void HandleSurfacePresentationCallback(wl_resource* resource,
                                       base::TimeTicks presentation_time,
                                       base::TimeDelta refresh) {
  if (presentation_time.is_null()) {
    wp_presentation_feedback_send_discarded(resource);
  } else {
    int64_t presentation_time_us = presentation_time.ToInternalValue();
    int64_t seconds = presentation_time_us / base::Time::kMicrosecondsPerSecond;
    int64_t microseconds =
        presentation_time_us % base::Time::kMicrosecondsPerSecond;
    wp_presentation_feedback_send_presented(
        resource, seconds >> 32, seconds & 0xffffffff,
        microseconds * base::Time::kNanosecondsPerMicrosecond,
        refresh.InMicroseconds() * base::Time::kNanosecondsPerMicrosecond, 0, 0,
        WP_PRESENTATION_FEEDBACK_KIND_VSYNC |
            WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK |
            WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION);
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
      base::CancelableCallback<void(base::TimeTicks, base::TimeDelta)>>(
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
// gaming_input_interface:

// Gamepad delegate class that forwards gamepad events to the client resource.
class WaylandGamepadDelegate : public GamepadDelegate {
 public:
  explicit WaylandGamepadDelegate(wl_resource* gamepad_resource)
      : gamepad_resource_(gamepad_resource) {}

  // If gamepad_resource_ is destroyed first, ResetGamepadResource will
  // be called to remove the resource from delegate, and delegate won't
  // do anything after that. If delegate is destructed first, it will
  // set the data to null in the gamepad_resource_, then the resource
  // destroy won't reset the delegate (cause it's gone).
  static void ResetGamepadResource(wl_resource* resource) {
    WaylandGamepadDelegate* delegate =
        GetUserDataAs<WaylandGamepadDelegate>(resource);
    if (delegate) {
      delegate->gamepad_resource_ = nullptr;
    }
  }

  // Override from GamepadDelegate:
  void OnRemoved() override {
    if (!gamepad_resource_) {
      return;
    }
    zcr_gamepad_v2_send_removed(gamepad_resource_);
    wl_client_flush(client());
    // Reset the user data in gamepad_resource.
    wl_resource_set_user_data(gamepad_resource_, nullptr);
    delete this;
  }
  void OnAxis(int axis, double value) override {
    if (!gamepad_resource_) {
      return;
    }
    zcr_gamepad_v2_send_axis(gamepad_resource_, NowInMilliseconds(), axis,
                             wl_fixed_from_double(value));
  }
  void OnButton(int button, bool pressed, double value) override {
    if (!gamepad_resource_) {
      return;
    }
    uint32_t state = pressed ? ZCR_GAMEPAD_V2_BUTTON_STATE_PRESSED
                             : ZCR_GAMEPAD_V2_BUTTON_STATE_RELEASED;
    zcr_gamepad_v2_send_button(gamepad_resource_, NowInMilliseconds(), button,
                               state, wl_fixed_from_double(value));
  }
  void OnFrame() override {
    if (!gamepad_resource_) {
      return;
    }
    zcr_gamepad_v2_send_frame(gamepad_resource_, NowInMilliseconds());
    wl_client_flush(client());
  }

 private:
  // The object should be deleted by OnRemoved().
  ~WaylandGamepadDelegate() override {}

  // The client who own this gamepad instance.
  wl_client* client() const {
    return wl_resource_get_client(gamepad_resource_);
  }

  // The gamepad resource associated with the gamepad.
  wl_resource* gamepad_resource_;

  DISALLOW_COPY_AND_ASSIGN(WaylandGamepadDelegate);
};

void gamepad_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_gamepad_v2_interface gamepad_implementation = {
    gamepad_destroy};

// GamingSeat delegate that provide gamepad added.
class WaylandGamingSeatDelegate : public GamingSeatDelegate {
 public:
  explicit WaylandGamingSeatDelegate(wl_resource* gaming_seat_resource)
      : gaming_seat_resource_{gaming_seat_resource} {}

  // Override from GamingSeatDelegate:
  void OnGamingSeatDestroying(GamingSeat*) override { delete this; }
  bool CanAcceptGamepadEventsForSurface(Surface* surface) const override {
    wl_resource* surface_resource = GetSurfaceResource(surface);
    return surface_resource &&
           wl_resource_get_client(surface_resource) ==
               wl_resource_get_client(gaming_seat_resource_);
  }
  GamepadDelegate* GamepadAdded() override {
    wl_resource* gamepad_resource =
        wl_resource_create(wl_resource_get_client(gaming_seat_resource_),
                           &zcr_gamepad_v2_interface,
                           wl_resource_get_version(gaming_seat_resource_), 0);

    GamepadDelegate* gamepad_delegate =
        new WaylandGamepadDelegate(gamepad_resource);

    wl_resource_set_implementation(
        gamepad_resource, &gamepad_implementation, gamepad_delegate,
        &WaylandGamepadDelegate::ResetGamepadResource);

    zcr_gaming_seat_v2_send_gamepad_added(gaming_seat_resource_,
                                          gamepad_resource);

    return gamepad_delegate;
  }

 private:
  // The gaming seat resource associated with the gaming seat.
  wl_resource* const gaming_seat_resource_;

  DISALLOW_COPY_AND_ASSIGN(WaylandGamingSeatDelegate);
};

void gaming_seat_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_gaming_seat_v2_interface gaming_seat_implementation = {
    gaming_seat_destroy};

void gaming_input_get_gaming_seat(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t id,
                                  wl_resource* seat) {
  wl_resource* gaming_seat_resource =
      wl_resource_create(client, &zcr_gaming_seat_v2_interface,
                         wl_resource_get_version(resource), id);

  SetImplementation(gaming_seat_resource, &gaming_seat_implementation,
                    std::make_unique<GamingSeat>(
                        new WaylandGamingSeatDelegate(gaming_seat_resource)));
}

void gaming_input_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_gaming_input_v2_interface gaming_input_implementation = {
    gaming_input_get_gaming_seat, gaming_input_destroy};

void bind_gaming_input(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_gaming_input_v2_interface, version, id);

  wl_resource_set_implementation(resource, &gaming_input_implementation,
                                 nullptr, nullptr);
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

////////////////////////////////////////////////////////////////////////////////
// pointer_gesture_swipe_v1 interface:

void pointer_gestures_get_swipe_gesture(wl_client* client,
                                        wl_resource* resource,
                                        uint32_t id,
                                        wl_resource* pointer_resource) {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// pointer_gesture_pinch_v1 interface:

class WaylandPointerGesturePinchDelegate : public PointerGesturePinchDelegate {
 public:
  WaylandPointerGesturePinchDelegate(wl_resource* resource, Pointer* pointer)
      : resource_(resource), pointer_(pointer) {
    pointer_->SetGesturePinchDelegate(this);
  }

  ~WaylandPointerGesturePinchDelegate() override {
    if (pointer_)
      pointer_->SetGesturePinchDelegate(nullptr);
  }
  void OnPointerDestroying(Pointer* pointer) override { pointer_ = nullptr; }
  void OnPointerPinchBegin(uint32_t unique_touch_event_id,
                           base::TimeTicks time_stamp,
                           Surface* surface) override {
    wl_resource* surface_resource = GetSurfaceResource(surface);
    DCHECK(surface_resource);
    zwp_pointer_gesture_pinch_v1_send_begin(resource_, unique_touch_event_id,
                                            TimeTicksToMilliseconds(time_stamp),
                                            surface_resource, 2);
  }
  void OnPointerPinchUpdate(base::TimeTicks time_stamp, float scale) override {
    zwp_pointer_gesture_pinch_v1_send_update(
        resource_, TimeTicksToMilliseconds(time_stamp), 0, 0,
        wl_fixed_from_double(scale), 0);
  }
  void OnPointerPinchEnd(uint32_t unique_touch_event_id,
                         base::TimeTicks time_stamp) override {
    zwp_pointer_gesture_pinch_v1_send_end(resource_, unique_touch_event_id,
                                          TimeTicksToMilliseconds(time_stamp),
                                          0);
  }

 private:
  wl_resource* const resource_;
  Pointer* pointer_;

  DISALLOW_COPY_AND_ASSIGN(WaylandPointerGesturePinchDelegate);
};

void pointer_gesture_pinch_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zwp_pointer_gesture_pinch_v1_interface
    pointer_gesture_pinch_implementation = {pointer_gesture_pinch_destroy};

void pointer_gestures_get_pinch_gesture(wl_client* client,
                                        wl_resource* resource,
                                        uint32_t id,
                                        wl_resource* pointer_resource) {
  Pointer* pointer = GetUserDataAs<Pointer>(pointer_resource);
  wl_resource* pointer_gesture_pinch_resource = wl_resource_create(
      client, &zwp_pointer_gesture_pinch_v1_interface, 1, id);
  SetImplementation(pointer_gesture_pinch_resource,
                    &pointer_gesture_pinch_implementation,
                    base::MakeUnique<WaylandPointerGesturePinchDelegate>(
                        pointer_gesture_pinch_resource, pointer));
}

////////////////////////////////////////////////////////////////////////////////
// pointer_gestures_v1 interface:

const struct zwp_pointer_gestures_v1_interface pointer_gestures_implementation =
    {pointer_gestures_get_swipe_gesture, pointer_gestures_get_pinch_gesture};

void bind_pointer_gestures(wl_client* client,
                           void* data,
                           uint32_t version,
                           uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zwp_pointer_gestures_v1_interface, version, id);
  wl_resource_set_implementation(resource, &pointer_gestures_implementation,
                                 data, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// pointer_stylus interface (deprecated)
// TODO(denniskempin): Remove once client no longer depends on this interface.

void pointer_stylus_destroy_DEPRECATED(wl_client* client,
                                       wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_pointer_stylus_v1_interface
    pointer_stylus_implementation_DEPRECATED = {
        pointer_stylus_destroy_DEPRECATED};

////////////////////////////////////////////////////////////////////////////////
// stylus_v1 interface (deprecated):
// TODO(denniskempin): Remove once client no longer depends on this interface.

void stylus_get_pointer_stylus_DEPRECATED(wl_client* client,
                                          wl_resource* resource,
                                          uint32_t id,
                                          wl_resource* pointer_resource) {
  wl_resource* stylus_resource =
      wl_resource_create(client, &zcr_pointer_stylus_v1_interface, 1, id);
  wl_resource_set_implementation(stylus_resource,
                                 &pointer_stylus_implementation_DEPRECATED,
                                 nullptr, nullptr);
}

const struct zcr_stylus_v1_interface stylus_v1_implementation_DEPRECATED = {
    stylus_get_pointer_stylus_DEPRECATED};

void bind_stylus_v1_DEPRECATED(wl_client* client,
                               void* data,
                               uint32_t version,
                               uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_stylus_v1_interface, version, id);
  wl_resource_set_implementation(resource, &stylus_v1_implementation_DEPRECATED,
                                 data, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// keyboard_device_configuration interface:

class WaylandKeyboardDeviceConfigurationDelegate
    : public KeyboardDeviceConfigurationDelegate,
      public KeyboardObserver {
 public:
  WaylandKeyboardDeviceConfigurationDelegate(wl_resource* resource,
                                             Keyboard* keyboard)
      : resource_(resource), keyboard_(keyboard) {
    keyboard_->SetDeviceConfigurationDelegate(this);
    keyboard_->AddObserver(this);
  }
  ~WaylandKeyboardDeviceConfigurationDelegate() override {
    if (keyboard_) {
      keyboard_->SetDeviceConfigurationDelegate(nullptr);
      keyboard_->RemoveObserver(this);
    }
  }

  void OnKeyboardDestroying(Keyboard* keyboard) override {
    keyboard_ = nullptr;
  }
  void OnKeyboardTypeChanged(bool is_physical) override {
    zcr_keyboard_device_configuration_v1_send_type_change(
        resource_,
        is_physical
            ? ZCR_KEYBOARD_DEVICE_CONFIGURATION_V1_KEYBOARD_TYPE_PHYSICAL
            : ZCR_KEYBOARD_DEVICE_CONFIGURATION_V1_KEYBOARD_TYPE_VIRTUAL);
  }

 private:
  wl_resource* resource_;
  Keyboard* keyboard_;

  DISALLOW_COPY_AND_ASSIGN(WaylandKeyboardDeviceConfigurationDelegate);
};

void keyboard_device_configuration_destroy(wl_client* client,
                                           wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_keyboard_device_configuration_v1_interface
    keyboard_device_configuration_implementation = {
        keyboard_device_configuration_destroy};

////////////////////////////////////////////////////////////////////////////////
// keyboard_configuration interface:

void keyboard_configuration_get_keyboard_device_configuration(
    wl_client* client,
    wl_resource* resource,
    uint32_t id,
    wl_resource* keyboard_resource) {
  Keyboard* keyboard = GetUserDataAs<Keyboard>(keyboard_resource);
  if (keyboard->HasDeviceConfigurationDelegate()) {
    wl_resource_post_error(
        resource,
        ZCR_KEYBOARD_CONFIGURATION_V1_ERROR_DEVICE_CONFIGURATION_EXISTS,
        "keyboard has already been associated with a device configuration "
        "object");
    return;
  }

  wl_resource* keyboard_device_configuration_resource = wl_resource_create(
      client, &zcr_keyboard_device_configuration_v1_interface, 1, id);

  SetImplementation(
      keyboard_device_configuration_resource,
      &keyboard_device_configuration_implementation,
      std::make_unique<WaylandKeyboardDeviceConfigurationDelegate>(
          keyboard_device_configuration_resource, keyboard));
}

const struct zcr_keyboard_configuration_v1_interface
    keyboard_configuration_implementation = {
        keyboard_configuration_get_keyboard_device_configuration};

void bind_keyboard_configuration(wl_client* client,
                                 void* data,
                                 uint32_t version,
                                 uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zcr_keyboard_configuration_v1_interface, version, id);
  wl_resource_set_implementation(
      resource, &keyboard_configuration_implementation, data, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// stylus_tool interface:

class StylusTool : public SurfaceObserver {
 public:
  explicit StylusTool(Surface* surface) : surface_(surface) {
    surface_->AddSurfaceObserver(this);
    surface_->SetProperty(kSurfaceHasStylusToolKey, true);
  }
  ~StylusTool() override {
    if (surface_) {
      surface_->RemoveSurfaceObserver(this);
      surface_->SetProperty(kSurfaceHasStylusToolKey, false);
    }
  }

  void SetStylusOnly() { surface_->SetStylusOnly(); }

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override {
    surface->RemoveSurfaceObserver(this);
    surface_ = nullptr;
  }

 private:
  Surface* surface_;

  DISALLOW_COPY_AND_ASSIGN(StylusTool);
};

void stylus_tool_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void stylus_tool_set_stylus_only(wl_client* client, wl_resource* resource) {
  GetUserDataAs<StylusTool>(resource)->SetStylusOnly();
}

const struct zcr_stylus_tool_v1_interface stylus_tool_implementation = {
    stylus_tool_destroy, stylus_tool_set_stylus_only};

////////////////////////////////////////////////////////////////////////////////
// stylus_tools interface:

void stylus_tools_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void stylus_tools_get_stylus_tool(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t id,
                                  wl_resource* surface_resource) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  if (surface->GetProperty(kSurfaceHasStylusToolKey)) {
    wl_resource_post_error(
        resource, ZCR_STYLUS_TOOLS_V1_ERROR_STYLUS_TOOL_EXISTS,
        "a stylus_tool object for that surface already exists");
    return;
  }

  wl_resource* stylus_tool_resource =
      wl_resource_create(client, &zcr_stylus_tool_v1_interface, 1, id);

  SetImplementation(stylus_tool_resource, &stylus_tool_implementation,
                    std::make_unique<StylusTool>(surface));
}

const struct zcr_stylus_tools_v1_interface stylus_tools_implementation = {
    stylus_tools_destroy, stylus_tools_get_stylus_tool};

void bind_stylus_tools(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_stylus_tools_v1_interface, 1, id);

  wl_resource_set_implementation(resource, &stylus_tools_implementation, data,
                                 nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// extended_keyboard interface:

class WaylandExtendedKeyboardImpl : public KeyboardObserver {
 public:
  explicit WaylandExtendedKeyboardImpl(Keyboard* keyboard)
      : keyboard_(keyboard) {
    keyboard_->AddObserver(this);
    keyboard_->SetNeedKeyboardKeyAcks(true);
  }
  ~WaylandExtendedKeyboardImpl() override {
    if (keyboard_) {
      keyboard_->RemoveObserver(this);
      keyboard_->SetNeedKeyboardKeyAcks(false);
    }
  }

  // Overridden from KeyboardObserver:
  void OnKeyboardDestroying(Keyboard* keyboard) override {
    DCHECK(keyboard_ == keyboard);
    keyboard_ = nullptr;
  }

  void AckKeyboardKey(uint32_t serial, bool handled) {
    if (keyboard_)
      keyboard_->AckKeyboardKey(serial, handled);
  }

 private:
  Keyboard* keyboard_;

  DISALLOW_COPY_AND_ASSIGN(WaylandExtendedKeyboardImpl);
};

void extended_keyboard_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void extended_keyboard_ack_key(wl_client* client,
                               wl_resource* resource,
                               uint32_t serial,
                               uint32_t handled_state) {
  GetUserDataAs<WaylandExtendedKeyboardImpl>(resource)->AckKeyboardKey(
      serial, handled_state == ZCR_EXTENDED_KEYBOARD_V1_HANDLED_STATE_HANDLED);
}

const struct zcr_extended_keyboard_v1_interface
    extended_keyboard_implementation = {extended_keyboard_destroy,
                                        extended_keyboard_ack_key};

////////////////////////////////////////////////////////////////////////////////
// keyboard_extension interface:

void keyboard_extension_get_extended_keyboard(wl_client* client,
                                              wl_resource* resource,
                                              uint32_t id,
                                              wl_resource* keyboard_resource) {
  Keyboard* keyboard = GetUserDataAs<Keyboard>(keyboard_resource);
  if (keyboard->AreKeyboardKeyAcksNeeded()) {
    wl_resource_post_error(
        resource, ZCR_KEYBOARD_EXTENSION_V1_ERROR_EXTENDED_KEYBOARD_EXISTS,
        "keyboard has already been associated with a extended_keyboard object");
    return;
  }

  wl_resource* extended_keyboard_resource =
      wl_resource_create(client, &zcr_extended_keyboard_v1_interface, 1, id);

  SetImplementation(extended_keyboard_resource,
                    &extended_keyboard_implementation,
                    std::make_unique<WaylandExtendedKeyboardImpl>(keyboard));
}

const struct zcr_keyboard_extension_v1_interface
    keyboard_extension_implementation = {
        keyboard_extension_get_extended_keyboard};

void bind_keyboard_extension(wl_client* client,
                             void* data,
                             uint32_t version,
                             uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zcr_keyboard_extension_v1_interface, version, id);

  wl_resource_set_implementation(resource, &keyboard_extension_implementation,
                                 data, nullptr);
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
  wl_global_create(wl_display_.get(), &zwp_linux_dmabuf_v1_interface, 2,
                   display_, bind_linux_dmabuf);
#endif
  wl_global_create(wl_display_.get(), &wl_subcompositor_interface, 1, display_,
                   bind_subcompositor);
  wl_global_create(wl_display_.get(), &wl_shell_interface, 1, display_,
                   bind_shell);
  wl_global_create(wl_display_.get(), &wl_output_interface, output_version,
                   display_, bind_output);
  wl_global_create(wl_display_.get(), &xdg_shell_interface, 1, display_,
                   bind_xdg_shell_v5);
  wl_global_create(wl_display_.get(), &zxdg_shell_v6_interface, 1, display_,
                   bind_xdg_shell_v6);
  wl_global_create(wl_display_.get(), &zcr_vsync_feedback_v1_interface, 1,
                   display_, bind_vsync_feedback);
  wl_global_create(wl_display_.get(), &wl_data_device_manager_interface, 1,
                   display_, bind_data_device_manager);
  wl_global_create(wl_display_.get(), &wl_seat_interface, seat_version,
                   display_, bind_seat);
  wl_global_create(wl_display_.get(), &wp_viewporter_interface, 1, display_,
                   bind_viewporter);
  wl_global_create(wl_display_.get(), &wp_presentation_interface, 1, display_,
                   bind_presentation);
  wl_global_create(wl_display_.get(), &zcr_secure_output_v1_interface, 1,
                   display_, bind_secure_output);
  wl_global_create(wl_display_.get(), &zcr_alpha_compositing_v1_interface, 1,
                   display_, bind_alpha_compositing);
  wl_global_create(wl_display_.get(), &zcr_remote_shell_v1_interface,
                   remote_shell_version, display_, bind_remote_shell);
  wl_global_create(wl_display_.get(), &zaura_shell_interface, 1, display_,
                   bind_aura_shell);
  wl_global_create(wl_display_.get(), &zcr_gaming_input_v2_interface, 1,
                   display_, bind_gaming_input);
  wl_global_create(wl_display_.get(), &zcr_stylus_v1_interface, 2, display_,
                   bind_stylus_v1_DEPRECATED);
  wl_global_create(wl_display_.get(), &zcr_stylus_v2_interface, 1, display_,
                   bind_stylus_v2);
  wl_global_create(wl_display_.get(), &zwp_pointer_gestures_v1_interface, 1,
                   display_, bind_pointer_gestures);
  wl_global_create(wl_display_.get(), &zcr_keyboard_configuration_v1_interface,
                   2, display_, bind_keyboard_configuration);
  wl_global_create(wl_display_.get(), &zcr_stylus_tools_v1_interface, 1,
                   display_, bind_stylus_tools);
  wl_global_create(wl_display_.get(), &zcr_keyboard_extension_v1_interface, 1,
                   display_, bind_keyboard_extension);
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
