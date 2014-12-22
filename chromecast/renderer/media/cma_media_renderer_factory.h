// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_MEDIA_CMA_MEDIA_RENDERER_FACTORY_H_
#define CHROMECAST_RENDERER_MEDIA_CMA_MEDIA_RENDERER_FACTORY_H_

#include "base/macros.h"
#include "media/base/renderer_factory.h"

namespace chromecast {
namespace media {

class CmaMediaRendererFactory : public ::media::RendererFactory {
 public:
  explicit CmaMediaRendererFactory(int render_frame_id);
  ~CmaMediaRendererFactory() final;

  // ::media::RendererFactory implementation.
  scoped_ptr< ::media::Renderer> CreateRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      ::media::AudioRendererSink* audio_renderer_sink) final;

 private:
  int render_frame_id_;
  DISALLOW_COPY_AND_ASSIGN(CmaMediaRendererFactory);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_MEDIA_CMA_MEDIA_RENDERER_FACTORY_H_