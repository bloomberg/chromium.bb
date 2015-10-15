// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_MEDIA_AUDIO_PIPELINE_PROXY_H_
#define CHROMECAST_RENDERER_MEDIA_AUDIO_PIPELINE_PROXY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromecast/common/media/cma_ipc_common.h"
#include "media/base/pipeline_status.h"

namespace base {
class SingleThreadTaskRunner;
class SharedMemory;
}

namespace media {
class AudioDecoderConfig;
}

namespace chromecast {
namespace media {
class AudioPipelineProxyInternal;
class AvStreamerProxy;
class CodedFrameProvider;
class MediaChannelProxy;
struct AvPipelineClient;

class AudioPipelineProxy {
 public:
  AudioPipelineProxy(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
                     scoped_refptr<MediaChannelProxy> media_channel_proxy);
  ~AudioPipelineProxy();

  void Initialize(
      const ::media::AudioDecoderConfig& config,
      scoped_ptr<CodedFrameProvider> frame_provider,
      const ::media::PipelineStatusCB& status_cb);
  void StartFeeding();
  void Flush(const base::Closure& done_cb);
  void Stop();

  void SetClient(const AvPipelineClient& client);
  void SetVolume(float volume);

 private:
  base::ThreadChecker thread_checker_;

  void OnAvPipeCreated(
      const ::media::AudioDecoderConfig& config,
      const ::media::PipelineStatusCB& status_cb,
      scoped_ptr<base::SharedMemory> shared_memory);
  void OnPipeWrite();
  void OnPipeRead();

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // |proxy_| main goal is to convert function calls to IPC messages.
  scoped_ptr<AudioPipelineProxyInternal> proxy_;

  scoped_ptr<AvStreamerProxy> audio_streamer_;

  base::WeakPtr<AudioPipelineProxy> weak_this_;
  base::WeakPtrFactory<AudioPipelineProxy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioPipelineProxy);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_MEDIA_AUDIO_PIPELINE_PROXY_H_
