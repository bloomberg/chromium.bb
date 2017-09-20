// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/clients/simple.h"

#include "base/command_line.h"
#include "components/exo/wayland/clients/client_helper.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gl/gl_bindings.h"

namespace exo {
namespace wayland {
namespace clients {
namespace {

void FrameCallback(void* data, wl_callback* callback, uint32_t time) {
  bool* callback_pending = static_cast<bool*>(data);
  *callback_pending = false;
}

}  // namespace

Simple::Simple() = default;

void Simple::Run(int frames) {
  bool callback_pending = false;
  std::unique_ptr<wl_callback> frame_callback;
  wl_callback_listener frame_listener = {FrameCallback};
  int frame_count = 0;
  do {
    if (callback_pending)
      continue;

    if (frame_count == frames)
      break;

    callback_pending = true;
    Buffer* buffer = buffers_.front().get();
    SkCanvas* canvas = buffer->sk_surface->getCanvas();

    static const SkColor kColors[] = {SK_ColorRED, SK_ColorBLACK};
    canvas->clear(kColors[++frame_count % arraysize(kColors)]);

    if (gr_context_) {
      gr_context_->flush();
      glFinish();
    }
    wl_surface_set_buffer_scale(surface_.get(), scale_);
    wl_surface_set_buffer_transform(surface_.get(), transform_);
    wl_surface_damage(surface_.get(), 0, 0, surface_size_.width(),
                      surface_size_.height());
    wl_surface_attach(surface_.get(), buffer->buffer.get(), 0, 0);

    frame_callback.reset(wl_surface_frame(surface_.get()));
    wl_callback_add_listener(frame_callback.get(), &frame_listener,
                             &callback_pending);
    wl_surface_commit(surface_.get());
    wl_display_flush(display_.get());
  } while (wl_display_dispatch(display_.get()) != -1);
}

}  // namespace clients
}  // namespace wayland
}  // namespace exo
