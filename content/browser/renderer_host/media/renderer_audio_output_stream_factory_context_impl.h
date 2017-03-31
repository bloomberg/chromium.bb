// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_RENDERER_AUDIO_OUTPUT_STREAM_FACTORY_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_RENDERER_AUDIO_OUTPUT_STREAM_FACTORY_CONTEXT_IMPL_H_

#include <memory>
#include <string>

#include "content/browser/renderer_host/media/renderer_audio_output_stream_factory_context.h"
#include "content/common/media/renderer_audio_output_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"

namespace media {
class AudioManager;
class AudioSystem;
}  // namespace media

namespace content {

class MediaStreamManager;

// In addition to being a RendererAudioOutputStreamFactoryContext, this class
// also handles requests for mojom::RendererAudioOutputStreamFactory instances.
//
// Ownership diagram for stream IPC classes (excluding interfaces):
// RendererAudioOutputStreamFactoryContext
//                 ^
//                 | owns (at most one per render frame in the process).
//                 |
// RenderFrameAudioOutputStreamFactory
//                 ^
//                 | owns (one per stream for the frame).
//                 |
// media::MojoAudioOutputStreamProvider
//                 ^
//                 | owns (one).
//                 |
// media::MojoAudioOutputStream

class CONTENT_EXPORT RendererAudioOutputStreamFactoryContextImpl
    : public RendererAudioOutputStreamFactoryContext {
 public:
  RendererAudioOutputStreamFactoryContextImpl(
      int render_process_id,
      media::AudioSystem* audio_system,
      media::AudioManager* audio_manager,
      MediaStreamManager* media_stream_manager,
      const std::string& salt);

  ~RendererAudioOutputStreamFactoryContextImpl() override;

  // Creates a factory and binds it to the request. Intended to be registered
  // in a RenderFrameHosts InterfaceRegistry.
  void CreateFactory(
      int frame_host_id,
      mojo::InterfaceRequest<mojom::RendererAudioOutputStreamFactory> request);

  // RendererAudioOutputStreamFactoryContext implementation.
  int GetRenderProcessId() const override;

  std::string GetHMACForDeviceId(
      const url::Origin& origin,
      const std::string& raw_device_id) const override;

  void RequestDeviceAuthorization(
      int render_frame_id,
      int session_id,
      const std::string& device_id,
      const url::Origin& security_origin,
      AuthorizationCompletedCallback cb) const override;

  std::unique_ptr<media::AudioOutputDelegate> CreateDelegate(
      const std::string& unique_device_id,
      int render_frame_id,
      const media::AudioParameters& params,
      media::AudioOutputDelegate::EventHandler* handler) override;

 private:
  // Used for hashing the device_id.
  const std::string salt_;
  media::AudioSystem* const audio_system_;
  media::AudioManager* const audio_manager_;
  MediaStreamManager* const media_stream_manager_;
  const AudioOutputAuthorizationHandler authorization_handler_;
  const int render_process_id_;

  // All streams requires ids for logging, so we keep a count for that.
  int next_stream_id_ = 0;

  // The factories created by |this| is kept here, so that we can make sure they
  // don't keep danging references to |this|.
  mojo::StrongBindingSet<mojom::RendererAudioOutputStreamFactory> factories_;

  DISALLOW_COPY_AND_ASSIGN(RendererAudioOutputStreamFactoryContextImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_RENDERER_AUDIO_OUTPUT_STREAM_FACTORY_CONTEXT_IMPL_H_
