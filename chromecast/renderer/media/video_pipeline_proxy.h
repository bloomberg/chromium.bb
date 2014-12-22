// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_MEDIA_VIDEO_PIPELINE_PROXY_H_
#define CHROMECAST_RENDERER_MEDIA_VIDEO_PIPELINE_PROXY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromecast/media/cma/pipeline/video_pipeline.h"
#include "media/base/pipeline_status.h"

namespace base {
class MessageLoopProxy;
class SharedMemory;
}

namespace media {
class VideoDecoderConfig;
}

namespace chromecast {
namespace media {
struct AvPipelineClient;
class AvStreamerProxy;
class CodedFrameProvider;
class VideoPipelineProxyInternal;
class MediaChannelProxy;

class VideoPipelineProxy : public VideoPipeline {
 public:
  VideoPipelineProxy(
      scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy,
      scoped_refptr<MediaChannelProxy> media_channel_proxy);
  ~VideoPipelineProxy() override;

  void Initialize(const ::media::VideoDecoderConfig& config,
                  scoped_ptr<CodedFrameProvider> frame_provider,
                  const ::media::PipelineStatusCB& status_cb);
  void StartFeeding();
  void Flush(const base::Closure& done_cb);
  void Stop();

  // VideoPipeline implementation.
  void SetClient(const VideoPipelineClient& video_client) override;

 private:
  base::ThreadChecker thread_checker_;

  void OnAvPipeCreated(
      const ::media::VideoDecoderConfig& config,
      const ::media::PipelineStatusCB& status_cb,
      scoped_ptr<base::SharedMemory> shared_memory);
  void OnPipeWrite();
  void OnPipeRead();

  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;

  // |proxy_| main goal is to convert function calls to IPC messages.
  scoped_ptr<VideoPipelineProxyInternal> proxy_;

  scoped_ptr<AvStreamerProxy> video_streamer_;

  base::WeakPtr<VideoPipelineProxy> weak_this_;
  base::WeakPtrFactory<VideoPipelineProxy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoPipelineProxy);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_MEDIA_VIDEO_PIPELINE_PROXY_H_