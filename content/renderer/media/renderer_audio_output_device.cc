// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_audio_output_device.h"

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "content/renderer/media/audio_message_filter.h"

namespace content {

RendererAudioOutputDevice::RendererAudioOutputDevice(
    AudioMessageFilter* message_filter,
    const scoped_refptr<base::MessageLoopProxy>& io_loop)
    : AudioOutputDevice(message_filter, io_loop),
      source_render_view_id_(MSG_ROUTING_NONE),
      is_after_stream_created_(false) {
}

RendererAudioOutputDevice::~RendererAudioOutputDevice() {}

void RendererAudioOutputDevice::Start() {
  AudioOutputDevice::Start();
  message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&RendererAudioOutputDevice::OnStart, this));
}

void RendererAudioOutputDevice::Stop() {
  AudioOutputDevice::Stop();
  message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&RendererAudioOutputDevice::OnStop, this));
}

void RendererAudioOutputDevice::SetSourceRenderView(int render_view_id) {
  message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&RendererAudioOutputDevice::OnSourceChange, this,
                 render_view_id));
}

void RendererAudioOutputDevice::OnStart() {
  DCHECK(message_loop()->BelongsToCurrentThread());
  is_after_stream_created_ = true;
  OnSourceChange(source_render_view_id_);
}

void RendererAudioOutputDevice::OnStop() {
  DCHECK(message_loop()->BelongsToCurrentThread());
  is_after_stream_created_ = false;
}

void RendererAudioOutputDevice::OnSourceChange(int render_view_id) {
  DCHECK(message_loop()->BelongsToCurrentThread());
  source_render_view_id_ = render_view_id;
  if (is_after_stream_created_ && source_render_view_id_ != MSG_ROUTING_NONE) {
    AudioMessageFilter* const filter =
        static_cast<AudioMessageFilter*>(audio_output_ipc());
    if (filter)
      filter->AssociateStreamWithProducer(stream_id(), source_render_view_id_);
  }
}

}  // namespace content
