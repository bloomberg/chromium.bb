// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_RENDERER_WRAPPER_H_
#define MEDIA_MOJO_CLIENTS_MOJO_RENDERER_WRAPPER_H_

#include <memory>

#include "media/base/renderer.h"
#include "media/mojo/clients/mojo_renderer.h"

namespace media {

// Simple wrapper around a MojoRenderer.
// Provides a default behavior for forwarding all media::Renderer calls to a
// media::Renderer instance in a different process, through |mojo_renderer_|.
// Used as a base class to reduce boiler plate code for derived types, which can
// override only the methods they need to specialize.
class MojoRendererWrapper : public Renderer {
 public:
  explicit MojoRendererWrapper(std::unique_ptr<MojoRenderer> mojo_renderer);
  ~MojoRendererWrapper() override;

  // Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  media::RendererClient* client,
                  PipelineStatusCallback init_cb) override;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) override;
  void SetLatencyHint(base::Optional<base::TimeDelta> latency_hint) override;
  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;

 private:
  std::unique_ptr<MojoRenderer> mojo_renderer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MojoRendererWrapper);
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_RENDERER_WRAPPER_H_
