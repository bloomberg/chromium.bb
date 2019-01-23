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
#include <linux-explicit-synchronization-unstable-v1-server-protocol.h>
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

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/free_deleter.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
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
#include "components/exo/notification.h"
#include "components/exo/surface.h"
#include "components/exo/touch.h"
#include "components/exo/touch_delegate.h"
#include "components/exo/touch_stylus_delegate.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wayland_display_output.h"
#include "components/exo/wayland/wayland_input_delegate.h"
#include "components/exo/wayland/wayland_touch_delegate.h"
#include "components/exo/wayland/wl_compositor.h"
#include "components/exo/wayland/wl_output.h"
#include "components/exo/wayland/wl_seat.h"
#include "components/exo/wayland/wl_shm.h"
#include "components/exo/wayland/wl_subcompositor.h"
#include "components/exo/wayland/zcr_vsync_feedback.h"
#include "components/exo/wm_helper.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/buildflags.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/compositor_vsync_manager.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_util.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/public/activation_change_observer.h"

#if defined(OS_CHROMEOS)
#include "components/exo/wayland/wayland_keyboard_delegate.h"
#include "components/exo/wayland/wayland_pointer_delegate.h"
#include "components/exo/wayland/wl_shell.h"
#include "components/exo/wayland/zaura_shell.h"
#include "components/exo/wayland/zcr_cursor_shapes.h"
#include "components/exo/wayland/zcr_gaming_input.h"
#include "components/exo/wayland/zcr_keyboard_configuration.h"
#include "components/exo/wayland/zcr_keyboard_extension.h"
#include "components/exo/wayland/zcr_notification_shell.h"
#include "components/exo/wayland/zcr_remote_shell.h"
#include "components/exo/wayland/zcr_stylus_tools.h"
#include "components/exo/wayland/zwp_input_timestamps_manager.h"
#include "components/exo/wayland/zwp_linux_explicit_synchronization.h"
#include "components/exo/wayland/zwp_pointer_gestures.h"
#include "components/exo/wayland/zwp_text_input_manager.h"
#include "components/exo/wayland/zxdg_shell.h"
#include "components/exo/wm_helper_chromeos.h"
#endif

#if defined(USE_OZONE)
#include <linux-dmabuf-unstable-v1-server-protocol.h>

#include "components/exo/wayland/zwp_linux_dmabuf.h"
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
// Server, public:

Server::Server(Display* display)
    : display_(display), wl_display_(wl_display_create()) {
  wl_global_create(wl_display_.get(), &wl_compositor_interface,
                   kWlCompositorVersion, display_, bind_compositor);
  wl_global_create(wl_display_.get(), &wl_shm_interface, 1, display_, bind_shm);
#if defined(USE_OZONE)
  wl_global_create(wl_display_.get(), &zwp_linux_dmabuf_v1_interface,
                   kZwpLinuxDmabufVersion, display_, bind_linux_dmabuf);
#endif
  wl_global_create(wl_display_.get(), &wl_subcompositor_interface, 1, display_,
                   bind_subcompositor);
  display::Screen::GetScreen()->AddObserver(this);
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays())
    OnDisplayAdded(display);
  wl_global_create(wl_display_.get(), &zcr_vsync_feedback_v1_interface, 1,
                   display_, bind_vsync_feedback);
  wl_global_create(wl_display_.get(), &wl_data_device_manager_interface,
                   data_device_manager_version, display_,
                   bind_data_device_manager);
  wl_global_create(wl_display_.get(), &wp_viewporter_interface, 1, display_,
                   bind_viewporter);
  wl_global_create(wl_display_.get(), &wp_presentation_interface, 1, display_,
                   bind_presentation);
  wl_global_create(wl_display_.get(), &zcr_secure_output_v1_interface, 1,
                   display_, bind_secure_output);
  wl_global_create(wl_display_.get(), &zcr_alpha_compositing_v1_interface, 1,
                   display_, bind_alpha_compositing);
  wl_global_create(wl_display_.get(), &zcr_stylus_v2_interface, 1, display_,
                   bind_stylus_v2);
  wl_global_create(wl_display_.get(), &wl_seat_interface, kWlSeatVersion,
                   display_->seat(), bind_seat);
#if defined(OS_CHROMEOS)
  wl_global_create(wl_display_.get(), &wl_shell_interface, 1, display_,
                   bind_shell);
  wl_global_create(wl_display_.get(), &zaura_shell_interface,
                   kZAuraShellVersion, display_, bind_aura_shell);
  wl_global_create(wl_display_.get(), &zcr_cursor_shapes_v1_interface, 1,
                   display_, bind_cursor_shapes);
  wl_global_create(wl_display_.get(), &zcr_gaming_input_v2_interface, 1,
                   display_, bind_gaming_input);
  wl_global_create(wl_display_.get(), &zcr_keyboard_configuration_v1_interface,
                   kZcrKeyboardConfigurationVersion, display_,
                   bind_keyboard_configuration);
  wl_global_create(wl_display_.get(), &zcr_keyboard_extension_v1_interface, 1,
                   display_, bind_keyboard_extension);
  wl_global_create(wl_display_.get(), &zcr_notification_shell_v1_interface, 1,
                   display_, bind_notification_shell);
  wl_global_create(wl_display_.get(), &zcr_remote_shell_v1_interface,
                   kZcrRemoteShellVersion, display_, bind_remote_shell);
  wl_global_create(wl_display_.get(), &zcr_stylus_tools_v1_interface, 1,
                   display_, bind_stylus_tools);
  wl_global_create(wl_display_.get(),
                   &zwp_input_timestamps_manager_v1_interface, 1, display_,
                   bind_input_timestamps_manager);
  wl_global_create(wl_display_.get(), &zwp_pointer_gestures_v1_interface, 1,
                   display_, bind_pointer_gestures);
  wl_global_create(wl_display_.get(), &zwp_text_input_manager_v1_interface, 1,
                   display_, bind_text_input_manager);
  wl_global_create(wl_display_.get(), &zxdg_shell_v6_interface, 1, display_,
                   bind_xdg_shell_v6);
  wl_global_create(wl_display_.get(),
                   &zwp_linux_explicit_synchronization_v1_interface, 1,
                   display_, bind_linux_explicit_synchronization);
#endif

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
  auto output = std::make_unique<WaylandDisplayOutput>(new_display.id());
  output->set_global(wl_global_create(wl_display_.get(), &wl_output_interface,
                                      kWlOutputVersion, output.get(),
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
