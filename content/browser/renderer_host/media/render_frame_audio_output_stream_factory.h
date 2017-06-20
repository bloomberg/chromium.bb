// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_RENDER_FRAME_AUDIO_OUTPUT_STREAM_FACTORY_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_RENDER_FRAME_AUDIO_OUTPUT_STREAM_FACTORY_H_

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/common/media/renderer_audio_output_stream_factory.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace url {
class Origin;
}

namespace content {

class RendererAudioOutputStreamFactoryContext;

// Handles a RendererAudioOutputStreamFactory request for a render frame host,
// using the provided RendererAudioOutputStreamFactoryContext. This class may
// be constructed on any thread, but must be used on the IO thread after that.
class CONTENT_EXPORT RenderFrameAudioOutputStreamFactory
    : public mojom::RendererAudioOutputStreamFactory {
 public:
  RenderFrameAudioOutputStreamFactory(
      int render_frame_id,
      RendererAudioOutputStreamFactoryContext* context);

  ~RenderFrameAudioOutputStreamFactory() override;

 private:
  using OutputStreamProviderSet =
      base::flat_set<std::unique_ptr<media::mojom::AudioOutputStreamProvider>>;

  // mojom::RendererAudioOutputStreamFactory implementation.
  void RequestDeviceAuthorization(
      media::mojom::AudioOutputStreamProviderRequest stream_provider,
      int64_t session_id,
      const std::string& device_id,
      RequestDeviceAuthorizationCallback callback) override;

  void RequestDeviceAuthorizationForOrigin(
      base::TimeTicks auth_start_time,
      media::mojom::AudioOutputStreamProviderRequest stream_provider_request,
      int session_id,
      const std::string& device_id,
      RequestDeviceAuthorizationCallback callback,
      const url::Origin& origin);

  void AuthorizationCompleted(
      base::TimeTicks auth_start_time,
      media::mojom::AudioOutputStreamProviderRequest request,
      RequestDeviceAuthorizationCallback callback,
      const url::Origin& origin,
      media::OutputDeviceStatus status,
      bool should_send_id,
      const media::AudioParameters& params,
      const std::string& raw_device_id);

  void RemoveStream(media::mojom::AudioOutputStreamProvider* stream_provider);

  const int render_frame_id_;
  RendererAudioOutputStreamFactoryContext* const context_;
  base::ThreadChecker thread_checker_;

  // The stream providers will contain the corresponding streams.
  OutputStreamProviderSet stream_providers_;

  base::WeakPtrFactory<RenderFrameAudioOutputStreamFactory> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameAudioOutputStreamFactory);
};

// This class is a convenient bundle of factory and binding.
class CONTENT_EXPORT RenderFrameAudioOutputStreamFactoryHandle {
 public:
  static std::unique_ptr<RenderFrameAudioOutputStreamFactoryHandle,
                         BrowserThread::DeleteOnIOThread>
  CreateFactory(RendererAudioOutputStreamFactoryContext* context,
                int frame_id,
                mojom::RendererAudioOutputStreamFactoryRequest request);

  ~RenderFrameAudioOutputStreamFactoryHandle();

 private:
  RenderFrameAudioOutputStreamFactoryHandle(
      RendererAudioOutputStreamFactoryContext* context,
      int frame_id);

  void Init(mojom::RendererAudioOutputStreamFactoryRequest request);

  RenderFrameAudioOutputStreamFactory impl_;
  mojo::Binding<mojom::RendererAudioOutputStreamFactory> binding_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameAudioOutputStreamFactoryHandle);
};

using UniqueAudioOutputStreamFactoryPtr =
    std::unique_ptr<RenderFrameAudioOutputStreamFactoryHandle,
                    BrowserThread::DeleteOnIOThread>;

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_RENDER_FRAME_AUDIO_OUTPUT_STREAM_FACTORY_H_
