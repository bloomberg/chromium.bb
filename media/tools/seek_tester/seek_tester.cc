// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This standalone binary is a helper for diagnosing seek behavior of the
// demuxer setup in media/ code.  It answers the question: "if I ask the demuxer
// to Seek to X ms, where will it actually seek to? (necessitating
// frame-dropping until the original seek target is reached)".  Sample run:
//
// $ ./out/Debug/seek_tester .../LayoutTests/media/content/test.ogv 6300
// [0207/130327:INFO:seek_tester.cc(63)] Requested: 6123ms
// [0207/130327:INFO:seek_tester.cc(68)]   audio seeked to: 5526ms
// [0207/130327:INFO:seek_tester.cc(74)]   video seeked to: 5577ms


#include "base/at_exit.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "media/base/media.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/file_data_source.h"

class DemuxerHostImpl : public media::DemuxerHost {
 public:
  // DataSourceHost implementation.
  virtual void SetTotalBytes(int64 total_bytes) OVERRIDE {}
  virtual void SetBufferedBytes(int64 buffered_bytes) OVERRIDE {}
  virtual void SetNetworkActivity(bool is_downloading_data) OVERRIDE {}

  // DemuxerHost implementation.
  virtual void SetDuration(base::TimeDelta duration) OVERRIDE {}
  virtual void SetCurrentReadPosition(int64 offset) OVERRIDE {}
  virtual void OnDemuxerError(media::PipelineStatus error) OVERRIDE {}
};

void QuitMessageLoop(MessageLoop* loop, media::PipelineStatus status) {
  CHECK_EQ(status, media::PIPELINE_OK);
  loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

void TimestampExtractor(uint64* timestamp_ms,
                        MessageLoop* loop,
                        const scoped_refptr<media::Buffer>& buffer) {
  if (buffer->GetTimestamp() == media::kNoTimestamp())
    *timestamp_ms = -1;
  else
    *timestamp_ms = buffer->GetTimestamp().InMillisecondsF();
  loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  media::InitializeMediaLibraryForTesting();

  CHECK_EQ(argc, 3) << "\nUsage: " << argv[0] << " <file> <seekTimeInMs>";
  uint64 seek_target_ms;
  CHECK(base::StringToUint64(argv[2], &seek_target_ms));
  scoped_refptr<media::FileDataSource> file_data_source(
      new media::FileDataSource());
  CHECK_EQ(file_data_source->Initialize(argv[1]), media::PIPELINE_OK);

  DemuxerHostImpl host;
  MessageLoop loop;
  media::PipelineStatusCB quitter = base::Bind(&QuitMessageLoop, &loop);
  scoped_refptr<media::FFmpegDemuxer> demuxer(
      new media::FFmpegDemuxer(&loop, file_data_source, true));
  demuxer->Initialize(&host, quitter);
  loop.Run();

  demuxer->Seek(base::TimeDelta::FromMilliseconds(seek_target_ms), quitter);
  loop.Run();

  uint64 audio_seeked_to_ms;
  uint64 video_seeked_to_ms;
  scoped_refptr<media::DemuxerStream> audio_stream(
      demuxer->GetStream(media::DemuxerStream::AUDIO));
  scoped_refptr<media::DemuxerStream> video_stream(
      demuxer->GetStream(media::DemuxerStream::VIDEO));
  LOG(INFO) << "Requested: " << seek_target_ms << "ms";
  if (audio_stream) {
    audio_stream->Read(base::Bind(
        &TimestampExtractor, &audio_seeked_to_ms, &loop));
    loop.Run();
    LOG(INFO) << "  audio seeked to: " << audio_seeked_to_ms << "ms";
  }
  if (video_stream) {
    video_stream->Read(
        base::Bind(&TimestampExtractor, &video_seeked_to_ms, &loop));
    loop.Run();
    LOG(INFO) << "  video seeked to: " << video_seeked_to_ms << "ms";
  }

  demuxer->Stop(base::Bind(&MessageLoop::Quit, base::Unretained(&loop)));
  loop.Run();

  return 0;
}
