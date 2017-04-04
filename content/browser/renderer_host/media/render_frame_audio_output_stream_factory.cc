// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/render_frame_audio_output_stream_factory.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_checker.h"
#include "content/browser/renderer_host/media/renderer_audio_output_stream_factory_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/services/mojo_audio_output_stream_provider.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/message.h"

namespace content {

namespace {

void UMALogDeviceAuthorizationTime(base::TimeTicks auth_start_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES("Media.Audio.OutputDeviceAuthorizationTime",
                             base::TimeTicks::Now() - auth_start_time,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMilliseconds(5000), 50);
}

url::Origin GetOrigin(int process_id, int frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHost* frame = RenderFrameHost::FromID(process_id, frame_id);
  return frame ? frame->GetLastCommittedOrigin() : url::Origin();
}

}  // namespace

RenderFrameAudioOutputStreamFactory::RenderFrameAudioOutputStreamFactory(
    int render_frame_id,
    RendererAudioOutputStreamFactoryContext* context)
    : render_frame_id_(render_frame_id),
      context_(context),
      weak_ptr_factory_(this) {
  DCHECK(context_);
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
    const RequestDeviceAuthorizationCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const base::TimeTicks auth_start_time = base::TimeTicks::Now();

  if (!base::IsValueInRangeForNumericType<int>(session_id)) {
    mojo::ReportBadMessage("session_id is not in integer range");
    // Note: We must call the callback even though we are killing the renderer.
    // This is mandated by mojo.
    callback.Run(
        media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED,
        media::AudioParameters::UnavailableDeviceParams(), std::string());
    return;
  }

  base::PostTaskAndReplyWithResult(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::UI).get(), FROM_HERE,
      base::Bind(GetOrigin, context_->GetRenderProcessId(), render_frame_id_),
      base::Bind(&RenderFrameAudioOutputStreamFactory::
                     RequestDeviceAuthorizationForOrigin,
                 weak_ptr_factory_.GetWeakPtr(), auth_start_time,
                 base::Passed(&stream_provider_request),
                 static_cast<int>(session_id), device_id, callback));
}

void RenderFrameAudioOutputStreamFactory::RequestDeviceAuthorizationForOrigin(
    base::TimeTicks auth_start_time,
    media::mojom::AudioOutputStreamProviderRequest stream_provider_request,
    int session_id,
    const std::string& device_id,
    const RequestDeviceAuthorizationCallback& callback,
    const url::Origin& origin) {
  DCHECK(thread_checker_.CalledOnValidThread());
  context_->RequestDeviceAuthorization(
      render_frame_id_, session_id, device_id, origin,
      base::Bind(&RenderFrameAudioOutputStreamFactory::AuthorizationCompleted,
                 weak_ptr_factory_.GetWeakPtr(), auth_start_time,
                 base::Passed(&stream_provider_request), callback, origin));
}

void RenderFrameAudioOutputStreamFactory::AuthorizationCompleted(
    base::TimeTicks auth_start_time,
    media::mojom::AudioOutputStreamProviderRequest request,
    const RequestDeviceAuthorizationCallback& callback,
    const url::Origin& origin,
    media::OutputDeviceStatus status,
    bool should_send_id,
    const media::AudioParameters& params,
    const std::string& raw_device_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  UMALogDeviceAuthorizationTime(auth_start_time);

  if (status != media::OUTPUT_DEVICE_STATUS_OK) {
    callback.Run(media::OutputDeviceStatus(status),
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
          base::Bind(&RenderFrameAudioOutputStreamFactory::RemoveStream,
                     base::Unretained(this))));

  callback.Run(media::OutputDeviceStatus(status), params,
               should_send_id
                   ? context_->GetHMACForDeviceId(origin, raw_device_id)
                   : std::string());
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
