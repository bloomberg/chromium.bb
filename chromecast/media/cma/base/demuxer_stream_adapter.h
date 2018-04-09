// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BASE_DEMUXER_STREAM_ADAPTER_H_
#define CHROMECAST_MEDIA_CMA_BASE_DEMUXER_STREAM_ADAPTER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chromecast/media/cma/base/coded_frame_provider.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder_config.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {
class DemuxerStream;
}

namespace chromecast {
namespace media {
class BalancedMediaTaskRunnerFactory;
class MediaTaskRunner;

// DemuxerStreamAdapter wraps a DemuxerStream into a CodedFrameProvider.
class DemuxerStreamAdapter : public CodedFrameProvider {
 public:
  DemuxerStreamAdapter(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const scoped_refptr<BalancedMediaTaskRunnerFactory>&
          media_task_runner_factory,
      ::media::DemuxerStream* demuxer_stream);
  ~DemuxerStreamAdapter() override;

  // CodedFrameProvider implementation.
  void Read(const ReadCB& read_cb) override;
  void Flush(const base::Closure& flush_cb) override;

 private:
  void ResetMediaTaskRunner();

  void ReadInternal(const ReadCB& read_cb);
  void RequestBuffer(const ReadCB& read_cb);

  // Callback invoked from the demuxer stream to signal a buffer is ready.
  void OnNewBuffer(const ReadCB& read_cb,
                   ::media::DemuxerStream::Status status,
                   scoped_refptr<::media::DecoderBuffer> input);

  base::ThreadChecker thread_checker_;

  // Task runner DemuxerStreamAdapter is running on.
  scoped_refptr<base::SingleThreadTaskRunner> const task_runner_;

  // Media task runner to pace requests to the DemuxerStream.
  scoped_refptr<BalancedMediaTaskRunnerFactory> const
      media_task_runner_factory_;
  scoped_refptr<MediaTaskRunner> media_task_runner_;
  base::TimeDelta max_pts_;

  // Frames are provided by |demuxer_stream_|.
  ::media::DemuxerStream* const demuxer_stream_;

  // Indicate if there is a pending read.
  bool is_pending_read_;

  // Indicate if |demuxer_stream_| has a pending read.
  bool is_pending_demuxer_read_;

  // In case of a pending flush operation, this is the callback
  // that is invoked when flush is completed.
  base::Closure flush_cb_;

  // Audio/video configuration that applies to the next frame.
  ::media::AudioDecoderConfig audio_config_;
  ::media::VideoDecoderConfig video_config_;

  base::WeakPtr<DemuxerStreamAdapter> weak_this_;
  base::WeakPtrFactory<DemuxerStreamAdapter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DemuxerStreamAdapter);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BASE_DEMUXER_STREAM_ADAPTER_H_
