// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/render_frame_audio_output_stream_factory.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/task_runner_util.h"
#include "content/browser/renderer_host/media/renderer_audio_output_stream_factory_context.h"
#include "content/public/browser/render_frame_host.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/services/mojo_audio_output_stream_provider.h"
#include "mojo/public/cpp/bindings/message.h"

namespace content {

namespace {

void UMALogDeviceAuthorizationTime(base::TimeTicks auth_start_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES("Media.Audio.OutputDeviceAuthorizationTime",
                             base::TimeTicks::Now() - auth_start_time,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMilliseconds(5000), 50);
}

}  // namespace

// static
std::unique_ptr<RenderFrameAudioOutputStreamFactoryHandle,
                BrowserThread::DeleteOnIOThread>
RenderFrameAudioOutputStreamFactoryHandle::CreateFactory(
    RendererAudioOutputStreamFactoryContext* context,
    int render_frame_id,
    mojom::RendererAudioOutputStreamFactoryRequest request) {
  std::unique_ptr<RenderFrameAudioOutputStreamFactoryHandle,
                  BrowserThread::DeleteOnIOThread>
      handle(new RenderFrameAudioOutputStreamFactoryHandle(context,
                                                           render_frame_id));
  // Unretained is safe since |*handle| must be posted to the IO thread prior to
  // deletion.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&RenderFrameAudioOutputStreamFactoryHandle::Init,
                     base::Unretained(handle.get()), std::move(request)));
  return handle;
}

RenderFrameAudioOutputStreamFactoryHandle::
    ~RenderFrameAudioOutputStreamFactoryHandle() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

RenderFrameAudioOutputStreamFactoryHandle::
    RenderFrameAudioOutputStreamFactoryHandle(
        RendererAudioOutputStreamFactoryContext* context,
        int render_frame_id)
    : impl_(render_frame_id, context), binding_(&impl_) {}

void RenderFrameAudioOutputStreamFactoryHandle::Init(
    mojom::RendererAudioOutputStreamFactoryRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  binding_.Bind(std::move(request));
}

RenderFrameAudioOutputStreamFactory::RenderFrameAudioOutputStreamFactory(
    int render_frame_id,
    RendererAudioOutputStreamFactoryContext* context)
    : render_frame_id_(render_frame_id),
      context_(context),
      weak_ptr_factory_(this) {
  DCHECK(context_);
  // No thread-hostile state has been initialized yet, so we don't have to bind
  // to this specific thread.
  thread_checker_.DetachFromThread();
}

RenderFrameAudioOutputStreamFactory::~RenderFrameAudioOutputStreamFactory() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Make sure to close all streams.
  stream_providers_.clear();
}

void RenderFrameAudioOutputStreamFactory::RequestDeviceAuthorization(
    media::mojom::AudioOutputStreamProviderRequest stream_provider_request,
    int64_t session_id,
    const std::string& device_id,
    RequestDeviceAuthorizationCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const base::TimeTicks auth_start_time = base::TimeTicks::Now();

  if (!base::IsValueInRangeForNumericType<int>(session_id)) {
    mojo::ReportBadMessage("session_id is not in integer range");
    // Note: We must call the callback even though we are killing the renderer.
    // This is mandated by mojo.
    std::move(callback).Run(
        media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED,
        media::AudioParameters::UnavailableDeviceParams(), std::string());
    return;
  }

  context_->RequestDeviceAuthorization(
      render_frame_id_, session_id, device_id,
      base::BindOnce(
          &RenderFrameAudioOutputStreamFactory::AuthorizationCompleted,
          weak_ptr_factory_.GetWeakPtr(), auth_start_time,
          std::move(stream_provider_request), std::move(callback)));
}

void RenderFrameAudioOutputStreamFactory::AuthorizationCompleted(
    base::TimeTicks auth_start_time,
    media::mojom::AudioOutputStreamProviderRequest request,
    RequestDeviceAuthorizationCallback callback,
    media::OutputDeviceStatus status,
    const media::AudioParameters& params,
    const std::string& raw_device_id,
    const std::string& device_id_for_renderer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  UMALogDeviceAuthorizationTime(auth_start_time);

  if (status != media::OUTPUT_DEVICE_STATUS_OK) {
    std::move(callback).Run(media::OutputDeviceStatus(status),
                            media::AudioParameters::UnavailableDeviceParams(),
                            std::string());
    return;
  }

  // Since |context_| outlives |this| and |this| outlives |stream_providers_|,
  // unretained is safe.
  stream_providers_.insert(
      base::MakeUnique<media::MojoAudioOutputStreamProvider>(
          std::move(request),
          base::BindOnce(
              &RendererAudioOutputStreamFactoryContext::CreateDelegate,
              base::Unretained(context_), raw_device_id, render_frame_id_),
          base::BindOnce(&RenderFrameAudioOutputStreamFactory::RemoveStream,
                         base::Unretained(this))));

  std::move(callback).Run(media::OutputDeviceStatus(status), params,
                          device_id_for_renderer);
}

void RenderFrameAudioOutputStreamFactory::RemoveStream(
    media::mojom::AudioOutputStreamProvider* stream_provider) {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::EraseIf(
      stream_providers_,
      [stream_provider](
          const std::unique_ptr<media::mojom::AudioOutputStreamProvider>&
              other) { return other.get() == stream_provider; });
}

}  // namespace content
