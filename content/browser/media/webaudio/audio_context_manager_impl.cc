// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/webaudio/audio_context_manager_impl.h"

#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

void AudioContextManagerImpl::Create(
    RenderFrameHost* render_frame_host,
    blink::mojom::AudioContextManagerRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<AudioContextManagerImpl>(render_frame_host),
      std::move(request));
}

AudioContextManagerImpl::AudioContextManagerImpl(
    RenderFrameHost* render_frame_host)
    : render_frame_host_impl_(
          static_cast<RenderFrameHostImpl*>(render_frame_host)) {
  DCHECK(render_frame_host);
}

AudioContextManagerImpl::~AudioContextManagerImpl() = default;

void AudioContextManagerImpl::AudioContextAudiblePlaybackStarted(
    int32_t audio_context_id) {
  // Notify observers that audible audio started playing from a WebAudio
  // AudioContext.
  render_frame_host_impl_->AudioContextPlaybackStarted(audio_context_id);
}

void AudioContextManagerImpl::AudioContextAudiblePlaybackStopped(
    int32_t audio_context_id) {
  // Notify observers that audible audio stopped playing from a WebAudio
  // AudioContext.
  render_frame_host_impl_->AudioContextPlaybackStopped(audio_context_id);
}

}  // namespace content
