// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/cast_mojo_media_client.h"

#include "chromecast/browser/media/cast_renderer.h"
#include "chromecast/browser/media/cma_media_pipeline_client.h"

namespace {
class CastRendererFactory : public media::RendererFactory {
 public:
  CastRendererFactory(
      scoped_refptr<chromecast::media::CmaMediaPipelineClient> pipeline_client,
      const scoped_refptr<media::MediaLog>& media_log)
      : pipeline_client_(pipeline_client), media_log_(media_log) {}
  ~CastRendererFactory() final {}

  scoped_ptr<media::Renderer> CreateRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      media::AudioRendererSink* audio_renderer_sink,
      media::VideoRendererSink* video_renderer_sink,
      const media::RequestSurfaceCB& request_surface_cb) final {
    DCHECK(!audio_renderer_sink && !video_renderer_sink);
    return make_scoped_ptr(new chromecast::media::CastRenderer(
        pipeline_client_, media_task_runner));
  }

 private:
  scoped_refptr<chromecast::media::CmaMediaPipelineClient> pipeline_client_;
  scoped_refptr<media::MediaLog> media_log_;
  DISALLOW_COPY_AND_ASSIGN(CastRendererFactory);
};
}  // namespace

namespace chromecast {
namespace media {

CastMojoMediaClient::CastMojoMediaClient(
    scoped_refptr<CmaMediaPipelineClient> pipeline_client)
    : pipeline_client_(pipeline_client) {}

CastMojoMediaClient::~CastMojoMediaClient() {}

scoped_ptr<::media::RendererFactory> CastMojoMediaClient::CreateRendererFactory(
    const scoped_refptr<::media::MediaLog>& media_log) {
  return make_scoped_ptr(new CastRendererFactory(pipeline_client_, media_log));
}

}  // namespace media
}  // namespace chromecast
