// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/fuzzer/harness.h"

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-util.h>

#include <cstring>
#include <functional>

#include "base/logging.h"
#include "components/exo/wayland/fuzzer/actions.pb.h"

namespace exo {
namespace wayland_fuzzer {

namespace wayland {
namespace wl_display {
void sync(Harness* harness,
          const wayland_fuzzer::wayland_wl_display_sync& action);
void get_registry(Harness* harness,
                  const wayland_wl_display_get_registry& action);
}  // namespace wl_display
namespace wl_registry {
void bind(Harness* harness, const wayland_wl_registry_bind& action);
void global(void* data,
            struct wl_registry* registry,
            uint32_t name,
            const char* interface,
            uint32_t version);
void global_remove(void* data, struct wl_registry* registry, uint32_t name);
static const struct wl_registry_listener kListener = {global, global_remove};
}  // namespace wl_registry
namespace wl_callback {
void done(void* data, struct wl_callback* receiver, uint32_t callback_data);
static const struct wl_callback_listener kListener = {done};
}  // namespace wl_callback

namespace wl_compositor {}

namespace wl_shm {
void format(void* data, struct wl_shm* receiver, uint32_t format);
static const struct wl_shm_listener kListener = {format};
}  // namespace wl_shm
}  // namespace wayland

void wayland::wl_display::sync(
    Harness* harness,
    const wayland_fuzzer::wayland_wl_display_sync& action) {
  struct wl_display* receiver = harness->get_wl_display(action.receiver());
  if (!receiver)
    return;
  struct wl_callback* callback = wl_display_sync(receiver);
  wl_callback_add_listener(callback, &wayland::wl_callback::kListener, harness);
  harness->add_wl_callback(callback);
}

void wayland::wl_display::get_registry(
    Harness* harness,
    const wayland_fuzzer::wayland_wl_display_get_registry& action) {
  struct wl_display* receiver = harness->get_wl_display(action.receiver());
  if (!receiver)
    return;
  struct wl_registry* new_object = wl_display_get_registry(receiver);
  wl_registry_add_listener(new_object, &wayland::wl_registry::kListener,
                           harness);
  harness->add_wl_registry(new_object);
}

void wayland::wl_registry::bind(Harness* harness,
                                const wayland_wl_registry_bind& action) {
  struct wl_registry* receiver = harness->get_wl_registry(action.receiver());
  if (!receiver)
    return;
  void* new_object = nullptr;
  switch (action.global()) {
    case global::wayland_wl_compositor:
      if (harness->wl_compositor_globals_.empty())
        return;
      new_object = wl_registry_bind(
          receiver, harness->wl_compositor_globals_[0].first,
          &wl_compositor_interface, harness->wl_compositor_globals_[0].second);
      harness->add_wl_compositor(
          static_cast<struct wl_compositor*>(new_object));
      break;
    case global::wayland_wl_shm:
      if (harness->wl_shm_globals_.empty())
        return;
      new_object = wl_registry_bind(receiver, harness->wl_shm_globals_[0].first,
                                    &wl_shm_interface,
                                    harness->wl_shm_globals_[0].second);

      wl_shm_add_listener(static_cast<struct wl_shm*>(new_object),
                          &wayland::wl_shm::kListener, harness);
      harness->add_wl_shm(static_cast<struct wl_shm*>(new_object));
      break;
    default:
      return;
  }
}

void wayland::wl_registry::global(void* data,
                                  struct wl_registry* registry,
                                  uint32_t name,
                                  const char* interface,
                                  uint32_t version) {
#define GLOBL(WLT)                                       \
  if (strcmp(interface, #WLT) == 0) {                    \
    harness->WLT##_globals_.emplace_back(name, version); \
  }

  Harness* harness = static_cast<Harness*>(data);
  GLOBL(wl_display);
  GLOBL(wl_registry);
  GLOBL(wl_callback);
  GLOBL(wl_compositor);
  GLOBL(wl_shm);
#undef GLOBL
}

void wayland::wl_registry::global_remove(void* data,
                                         struct wl_registry* registry,
                                         uint32_t name) {}

void wayland::wl_callback::done(void* data,
                                struct wl_callback* receiver,
                                uint32_t callback_data) {}

void wayland::wl_shm::format(void* data,
                             struct wl_shm* receiver,
                             uint32_t format) {}

Harness::Harness() {
  wl_display_list_.emplace_back(wl_display_connect(nullptr));
  DCHECK(wl_display_list_.front());
}

Harness::~Harness() {
  wl_display_disconnect(wl_display_list_.front());
}

void Harness::Run(const wayland_fuzzer::actions& actions) {
  for (const wayland_fuzzer::action& act : actions.acts())
    Run(act);
}

void Harness::Run(const wayland_fuzzer::action& action) {
  switch (action.act_case()) {
    case action::ActCase::ACT_NOT_SET:
      wl_display_roundtrip(wl_display_list_.front());
      break;
    case action::ActCase::kActWaylandWlDisplayGetRegistry:
      wayland::wl_display::get_registry(
          this, action.act_wayland_wl_display_get_registry());
      break;
    case action::ActCase::kActWaylandWlDisplaySync:
      wayland::wl_display::sync(this, action.act_wayland_wl_display_sync());
      break;
    case action::ActCase::kActWaylandWlRegistryBind:
      wayland::wl_registry::bind(this, action.act_wayland_wl_registry_bind());
      break;
  }
}

}  // namespace wayland_fuzzer
}  // namespace exo
