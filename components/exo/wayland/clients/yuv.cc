// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <drm_fourcc.h>
#include <fcntl.h>
#include <gbm.h>
#include <sys/mman.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "components/exo/wayland/clients/client_base.h"
#include "components/exo/wayland/clients/client_helper.h"

namespace exo {
namespace wayland {
namespace clients {
namespace {

void FrameCallback(void* data, wl_callback* callback, uint32_t time) {
  bool* callback_pending = static_cast<bool*>(data);
  *callback_pending = false;
}

}  // namespace

class YuvClient : public ClientBase {
 public:
  YuvClient() {}

  bool WriteSolidColor(gbm_bo* bo, SkColor color);

  void Run(const ClientBase::InitParams& params);
};

bool YuvClient::WriteSolidColor(gbm_bo* bo, SkColor color) {
  for (size_t i = 0; i < gbm_bo_get_num_planes(bo); ++i) {
    base::ScopedFD fd(gbm_bo_get_plane_fd(bo, i));
    uint32_t stride = gbm_bo_get_plane_stride(bo, i);
    uint32_t offset = gbm_bo_get_plane_offset(bo, i);
    void* void_data = mmap(nullptr, gbm_bo_get_plane_size(bo, i) + offset,
                           (PROT_READ | PROT_WRITE), MAP_SHARED, fd.get(), 0);
    if (void_data == MAP_FAILED) {
      LOG(ERROR) << "Failed mmap().";
      return false;
    }
    uint8_t* data = static_cast<uint8_t*>(void_data) + offset;
    uint8_t yuv[] = {
        (0.257 * SkColorGetR(color)) + (0.504 * SkColorGetG(color)) +
            (0.098 * SkColorGetB(color)) + 16,
        -(0.148 * SkColorGetR(color)) - (0.291 * SkColorGetG(color)) +
            (0.439 * SkColorGetB(color)) + 128,
        (0.439 * SkColorGetR(color)) - (0.368 * SkColorGetG(color)) -
            (0.071 * SkColorGetB(color)) + 128};
    if (i == 0) {
      for (uint32_t y = 0; y < height_; ++y) {
        for (uint32_t x = 0; x < width_; ++x) {
          data[stride * y + x] = yuv[0];
        }
      }
    } else {
      for (uint32_t y = 0; y < height_ / 2; ++y) {
        for (uint32_t x = 0; x < width_ / 2; ++x) {
          data[stride * y + x * 2] = yuv[1];
          data[stride * y + x * 2 + 1] = yuv[2];
        }
      }
    }
  }
  return true;
}

void YuvClient::Run(const ClientBase::InitParams& params) {
  if (!ClientBase::Init(params))
    return;
  bool callback_pending = false;
  std::unique_ptr<wl_callback> frame_callback;
  wl_callback_listener frame_listener = {FrameCallback};

  size_t frame_number = 0;
  do {
    if (callback_pending)
      continue;
    frame_number++;

    auto buffer_it =
        std::find_if(buffers_.begin(), buffers_.end(),
                     [](const std::unique_ptr<ClientBase::Buffer>& buffer) {
                       return !buffer->busy;
                     });
    if (buffer_it == buffers_.end()) {
      LOG(ERROR) << "Can't find free buffer";
      return;
    }
    auto* buffer = buffer_it->get();
    buffer->busy = true;
    const SkColor kColors[] = {SK_ColorBLUE,   SK_ColorGREEN, SK_ColorRED,
                               SK_ColorYELLOW, SK_ColorCYAN,  SK_ColorMAGENTA};
    if (!WriteSolidColor(buffer->bo.get(),
                         kColors[frame_number % buffers_.size()]))
      return;

    wl_surface_set_buffer_scale(surface_.get(), scale_);
    wl_surface_damage(surface_.get(), 0, 0, width_ / scale_, height_ / scale_);
    wl_surface_attach(surface_.get(), buffer->buffer.get(), 0, 0);

    frame_callback.reset(wl_surface_frame(surface_.get()));
    wl_callback_add_listener(frame_callback.get(), &frame_listener,
                             &callback_pending);
    callback_pending = true;
    wl_surface_commit(surface_.get());
    wl_display_flush(display_.get());
  } while (wl_display_dispatch(display_.get()) != -1);
}

}  // namespace clients
}  // namespace wayland
}  // namespace exo

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  exo::wayland::clients::ClientBase::InitParams params;
  params.use_drm = true;
  if (!params.FromCommandLine(*command_line))
    return 1;

  // TODO(dcastagna): Support other YUV formats.
  params.drm_format = DRM_FORMAT_NV12;

  exo::wayland::clients::YuvClient client;
  client.Run(params);
  return 1;
}
