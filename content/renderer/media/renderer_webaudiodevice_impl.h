// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"
#include "third_party/WebKit/public/platform/WebAudioDevice.h"
#include "third_party/WebKit/public/platform/WebAudioLatencyHint.h"
#include "url/origin.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {
class SilentSinkSuspender;
}

namespace content {
class CONTENT_EXPORT RendererWebAudioDeviceImpl
    : public blink::WebAudioDevice,
      public media::AudioRendererSink::RenderCallback {
 public:
  ~RendererWebAudioDeviceImpl() override;

  static std::unique_ptr<RendererWebAudioDeviceImpl> Create(
      media::ChannelLayout layout,
      int channels,
      const blink::WebAudioLatencyHint& latency_hint,
      blink::WebAudioDevice::RenderCallback* callback,
      int session_id,
      const url::Origin& security_origin);

  // blink::WebAudioDevice implementation.
  void Start() override;
  void Stop() override;
  double SampleRate() override;
  int FramesPerBuffer() override;

  // AudioRendererSink::RenderCallback implementation.
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             int prior_frames_skipped,
             media::AudioBus* dest) override;

  void OnRenderError() override;

  void SetMediaTaskRunnerForTesting(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner);

  const media::AudioParameters& get_sink_params_for_testing() {
    return sink_params_;
  }

 protected:
  // Callback to get output device params (for tests).
  using OutputDeviceParamsCallback = base::Callback<media::AudioParameters(
      int frame_id,
      int session_id,
      const std::string& device_id,
      const url::Origin& security_origin)>;

  // Callback get render frame ID for current context (for tests).
  using RenderFrameIdCallback = base::Callback<int()>;

  RendererWebAudioDeviceImpl(media::ChannelLayout layout,
                             int channels,
                             const blink::WebAudioLatencyHint& latency_hint,
                             blink::WebAudioDevice::RenderCallback* callback,
                             int session_id,
                             const url::Origin& security_origin,
                             const OutputDeviceParamsCallback& device_params_cb,
                             const RenderFrameIdCallback& render_frame_id_cb);

 private:
  const scoped_refptr<base::SingleThreadTaskRunner>& GetMediaTaskRunner();

  media::AudioParameters sink_params_;

  const blink::WebAudioLatencyHint latency_hint_;

  // Weak reference to the callback into WebKit code.
  blink::WebAudioDevice::RenderCallback* const client_callback_;

  // To avoid the need for locking, ensure the control methods of the
  // blink::WebAudioDevice implementation are called on the same thread.
  base::ThreadChecker thread_checker_;

  // When non-NULL, we are started.  When NULL, we are stopped.
  scoped_refptr<media::AudioRendererSink> sink_;

  // ID to allow browser to select the correct input device for unified IO.
  int session_id_;

  // Security origin, used to check permissions for |output_device_|.
  url::Origin security_origin_;

  // Used to suspend |sink_| usage when silence has been detected for too long.
  std::unique_ptr<media::SilentSinkSuspender> webaudio_suspender_;

  // Render frame routing ID for the current context.
  int frame_id_;

  // Allow unit tests to set a custom MediaThreadTaskRunner.
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(RendererWebAudioDeviceImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_
