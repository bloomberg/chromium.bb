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
#include <relative-pointer-unstable-v1-server-protocol.h>
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
#include "components/exo/wayland/wl_data_device_manager.h"
#include "components/exo/wayland/wl_output.h"
#include "components/exo/wayland/wl_seat.h"
#include "components/exo/wayland/wl_shm.h"
#include "components/exo/wayland/wl_subcompositor.h"
#include "components/exo/wayland/wp_presentation.h"
#include "components/exo/wayland/wp_viewporter.h"
#include "components/exo/wayland/zcr_secure_output.h"
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
#include "components/exo/wayland/zwp_relative_pointer_manager.h"
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

DEFINE_UI_CLASS_PROPERTY_TYPE(wl_resource*)

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

// A property key containing a boolean set to true if a blending object is
// associated with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasBlendingKey, false)

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
                   kWlDataDeviceManagerVersion, display_,
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
  wl_global_create(wl_display_.get(),
                   &zwp_relative_pointer_manager_v1_interface, 1, display_,
                   bind_relative_pointer_manager);
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
