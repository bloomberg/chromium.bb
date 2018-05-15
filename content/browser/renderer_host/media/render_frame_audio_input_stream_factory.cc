// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/render_frame_audio_input_stream_factory.h"

#include <utility>

#include "content/browser/media/capture/desktop_capture_device_uma_types.h"
#include "content/browser/media/forwarding_audio_stream_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "content/public/common/media_stream_request.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_input_device.h"
#include "media/base/audio_parameters.h"

namespace content {

namespace {

void LookUpDeviceAndRespondIfFound(
    scoped_refptr<AudioInputDeviceManager> audio_input_device_manager,
    int32_t session_id,
    base::OnceCallback<void(const MediaStreamDevice&)> response) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const MediaStreamDevice* device =
      audio_input_device_manager->GetOpenedDeviceById(session_id);
  if (device) {
    // Copies device.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::BindOnce(std::move(response), *device));
  }
}

}  // namespace

RenderFrameAudioInputStreamFactory::RenderFrameAudioInputStreamFactory(
    mojom::RendererAudioInputStreamFactoryRequest request,
    scoped_refptr<AudioInputDeviceManager> audio_input_device_manager,
    RenderFrameHost* render_frame_host)
    : binding_(this, std::move(request)),
      audio_input_device_manager_(std::move(audio_input_device_manager)),
      render_frame_host_(render_frame_host),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

RenderFrameAudioInputStreamFactory::~RenderFrameAudioInputStreamFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void RenderFrameAudioInputStreamFactory::CreateStream(
    mojom::RendererAudioInputStreamFactoryClientPtr client,
    int32_t session_id,
    const media::AudioParameters& audio_params,
    bool automatic_gain_control,
    uint32_t shared_memory_count) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &LookUpDeviceAndRespondIfFound, audio_input_device_manager_,
          session_id,
          base::BindOnce(&RenderFrameAudioInputStreamFactory::
                             CreateStreamAfterLookingUpDevice,
                         weak_ptr_factory_.GetWeakPtr(), std::move(client),
                         audio_params, automatic_gain_control,
                         shared_memory_count)));
}

void RenderFrameAudioInputStreamFactory::CreateStreamAfterLookingUpDevice(
    mojom::RendererAudioInputStreamFactoryClientPtr client,
    const media::AudioParameters& audio_params,
    bool automatic_gain_control,
    uint32_t shared_memory_count,
    const MediaStreamDevice& device) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ForwardingAudioStreamFactory* factory =
      ForwardingAudioStreamFactory::ForFrame(render_frame_host_);
  if (!factory)
    return;

  WebContentsMediaCaptureId capture_id;
  if (WebContentsMediaCaptureId::Parse(device.id, &capture_id)) {
    // For MEDIA_DESKTOP_AUDIO_CAPTURE, the source is selected from picker
    // window, we do not mute the source audio.
    // For MEDIA_TAB_AUDIO_CAPTURE, the probable use case is Cast, we mute
    // the source audio.
    // TODO(qiangchen): Analyze audio constraints to make a duplicating or
    // diverting decision. It would give web developer more flexibility.

    RenderFrameHost* source_host = RenderFrameHost::FromID(
        capture_id.render_process_id, capture_id.main_render_frame_id);
    if (!source_host) {
      // The source of the capture has already been destroyed, so fail early.
      return;
    }

    factory->CreateLoopbackStream(
        render_frame_host_, source_host, audio_params, shared_memory_count,
        capture_id.disable_local_echo, std::move(client));

    if (device.type == MEDIA_DESKTOP_AUDIO_CAPTURE)
      IncrementDesktopCaptureCounter(SYSTEM_LOOPBACK_AUDIO_CAPTURER_CREATED);
  } else {
    factory->CreateInputStream(render_frame_host_, device.id, audio_params,
                               shared_memory_count, automatic_gain_control,
                               std::move(client));

    // Only count for captures from desktop media picker dialog and system loop
    // back audio.
    if (device.type == MEDIA_DESKTOP_AUDIO_CAPTURE &&
        (media::AudioDeviceDescription::IsLoopbackDevice(device.id))) {
      IncrementDesktopCaptureCounter(SYSTEM_LOOPBACK_AUDIO_CAPTURER_CREATED);
    }
  }
}

}  // namespace content
