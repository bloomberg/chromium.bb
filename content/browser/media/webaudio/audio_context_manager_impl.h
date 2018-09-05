// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_WEBAUDIO_AUDIO_CONTEXT_MANAGER_IMPL_H_
#define CONTENT_BROWSER_MEDIA_WEBAUDIO_AUDIO_CONTEXT_MANAGER_IMPL_H_

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/mojom/webaudio/audio_context_manager.mojom.h"

namespace content {

class RenderFrameHost;
class RenderFrameHostImpl;

// Implements the mojo interface between WebAudio and the browser so that
// WebAudio can report when audible sounds from an AudioContext starts and
// stops.
class CONTENT_EXPORT AudioContextManagerImpl
    : public blink::mojom::AudioContextManager {
 public:
  explicit AudioContextManagerImpl(RenderFrameHost* render_frame_host);
  ~AudioContextManagerImpl() override;

  static void Create(RenderFrameHost* render_frame_host,
                     blink::mojom::AudioContextManagerRequest request);

  // Called when AudioContext starts or stops playing audible audio.
  void AudioContextAudiblePlaybackStarted(int32_t audio_context_id) final;
  void AudioContextAudiblePlaybackStopped(int32_t audio_context_id) final;

 private:
  RenderFrameHostImpl* const render_frame_host_impl_;

  DISALLOW_COPY_AND_ASSIGN(AudioContextManagerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_WEBAUDIO_AUDIO_CONTEXT_MANAGER_IMPL_H_
