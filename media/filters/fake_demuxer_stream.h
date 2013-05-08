// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FAKE_DEMUXER_STREAM_H_
#define MEDIA_FILTERS_FAKE_DEMUXER_STREAM_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/video_decoder_config.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace media {

class MEDIA_EXPORT FakeDemuxerStream : public DemuxerStream {
 public:
  // Constructs an object that outputs |num_configs| different configs in
  // sequence with |num_frames_in_one_config| buffers for each config. The
  // output buffers are encrypted if |is_encrypted| is true.
  FakeDemuxerStream(int num_configs,
                    int num_buffers_in_one_config,
                    bool is_encrypted);
  virtual ~FakeDemuxerStream();

  // DemuxerStream implementation.
  virtual void Read(const ReadCB& read_cb) OVERRIDE;
  virtual const AudioDecoderConfig& audio_decoder_config() OVERRIDE;
  virtual const VideoDecoderConfig& video_decoder_config() OVERRIDE;
  virtual Type type() OVERRIDE;
  virtual void EnableBitstreamConverter() OVERRIDE;

  // Upon the next read, holds the read callback until SatisfyRead() or Reset()
  // is called.
  void HoldNextRead();

  // Satisfies the pending read with the next scheduled status and buffer.
  void SatisfyRead();

  // Satisfies the pending read (if any) with kAborted and NULL. This call
  // always clears |hold_next_read_|.
  void Reset();

 private:
  void UpdateVideoDecoderConfig();
  void DoRead();

  scoped_refptr<base::MessageLoopProxy> message_loop_;

  int num_configs_left_;
  int num_buffers_in_one_config_;
  bool is_encrypted_;

  // Number of frames left with the current decoder config.
  int num_buffers_left_in_current_config_;

  base::TimeDelta current_timestamp_;
  base::TimeDelta duration_;

  AudioDecoderConfig audio_decoder_config_;

  gfx::Size next_coded_size_;
  VideoDecoderConfig video_decoder_config_;

  ReadCB read_cb_;
  bool hold_next_read_;

  DISALLOW_COPY_AND_ASSIGN(FakeDemuxerStream);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FAKE_DEMUXER_STREAM_H_
