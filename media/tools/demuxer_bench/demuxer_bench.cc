// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// demuxer_bench is a standalone benchmarking tool for FFmpegDemuxer. It
// simulates the reading requirements for playback by reading from the stream
// that has the earliest timestamp.

#include <iostream>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "media/base/media.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/file_data_source.h"

namespace switches {
const char kEnableBitstreamConverter[] = "enable-bitstream-converter";
}  // namespace switches

class DemuxerHostImpl : public media::DemuxerHost {
 public:
  DemuxerHostImpl() {}
  virtual ~DemuxerHostImpl() {}

  // DataSourceHost implementation.
  virtual void SetTotalBytes(int64 total_bytes) OVERRIDE {}
  virtual void AddBufferedByteRange(int64 start, int64 end) OVERRIDE {}
  virtual void AddBufferedTimeRange(base::TimeDelta start,
                                    base::TimeDelta end) OVERRIDE {}

  // DemuxerHost implementation.
  virtual void SetDuration(base::TimeDelta duration) OVERRIDE {}
  virtual void OnDemuxerError(media::PipelineStatus error) OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DemuxerHostImpl);
};

void QuitLoopWithStatus(base::MessageLoop* message_loop,
                        media::PipelineStatus status) {
  CHECK_EQ(status, media::PIPELINE_OK);
  message_loop->PostTask(FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

static void NeedKey(const std::string& type, scoped_ptr<uint8[]> init_data,
             int init_data_size) {
  LOG(INFO) << "File is encrypted.";
}

typedef std::vector<media::DemuxerStream* > Streams;

// Simulates playback reading requirements by reading from each stream
// present in |demuxer| in as-close-to-monotonically-increasing timestamp order.
class StreamReader {
 public:
  StreamReader(media::Demuxer* demuxer, bool enable_bitstream_converter);
  ~StreamReader();

  // Performs a single step read.
  void Read();

  // Returns true when all streams have reached end of stream.
  bool IsDone();

  int number_of_streams() { return streams_.size(); }
  const Streams& streams() { return streams_; }
  const std::vector<int>& counts() { return counts_; }

 private:
  void OnReadDone(base::MessageLoop* message_loop,
                  bool* end_of_stream,
                  base::TimeDelta* timestamp,
                  media::DemuxerStream::Status status,
                  const scoped_refptr<media::DecoderBuffer>& buffer);
  int GetNextStreamIndexToRead();

  Streams streams_;
  std::vector<bool> end_of_stream_;
  std::vector<base::TimeDelta> last_read_timestamp_;
  std::vector<int> counts_;

  DISALLOW_COPY_AND_ASSIGN(StreamReader);
};

StreamReader::StreamReader(media::Demuxer* demuxer,
                           bool enable_bitstream_converter) {
  media::DemuxerStream* stream =
      demuxer->GetStream(media::DemuxerStream::AUDIO);
  if (stream) {
    streams_.push_back(stream);
    end_of_stream_.push_back(false);
    last_read_timestamp_.push_back(media::kNoTimestamp());
    counts_.push_back(0);
  }

  stream = demuxer->GetStream(media::DemuxerStream::VIDEO);
  if (stream) {
    streams_.push_back(stream);
    end_of_stream_.push_back(false);
    last_read_timestamp_.push_back(media::kNoTimestamp());
    counts_.push_back(0);

    if (enable_bitstream_converter)
      stream->EnableBitstreamConverter();
  }
}

StreamReader::~StreamReader() {}

void StreamReader::Read() {
  int index = GetNextStreamIndexToRead();
  bool end_of_stream = false;
  base::TimeDelta timestamp;

  streams_[index]->Read(base::Bind(
      &StreamReader::OnReadDone, base::Unretained(this),
      base::MessageLoop::current(), &end_of_stream, &timestamp));
  base::MessageLoop::current()->Run();

  CHECK(end_of_stream || timestamp != media::kNoTimestamp());
  end_of_stream_[index] = end_of_stream;
  last_read_timestamp_[index] = timestamp;
  counts_[index]++;
}

bool StreamReader::IsDone() {
  for (size_t i = 0; i < end_of_stream_.size(); ++i) {
    if (!end_of_stream_[i])
      return false;
  }
  return true;
}

void StreamReader::OnReadDone(
    base::MessageLoop* message_loop,
    bool* end_of_stream,
    base::TimeDelta* timestamp,
    media::DemuxerStream::Status status,
    const scoped_refptr<media::DecoderBuffer>& buffer) {
  CHECK_EQ(status, media::DemuxerStream::kOk);
  CHECK(buffer);
  *end_of_stream = buffer->IsEndOfStream();
  *timestamp = *end_of_stream ? media::kNoTimestamp() : buffer->GetTimestamp();
  message_loop->PostTask(FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

int StreamReader::GetNextStreamIndexToRead() {
  int index = -1;
  for (int i = 0; i < number_of_streams(); ++i) {
    // Ignore streams at EOS.
    if (end_of_stream_[i])
      continue;

    // Use a stream if it hasn't been read from yet.
    if (last_read_timestamp_[i] == media::kNoTimestamp())
      return i;

    if (index < 0 ||
        last_read_timestamp_[i] < last_read_timestamp_[index]) {
      index = i;
    }
  }
  CHECK_GE(index, 0) << "Couldn't find a stream to read";
  return index;
}

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  media::InitializeMediaLibraryForTesting();

  CommandLine::Init(argc, argv);
  CommandLine* cmd_line = CommandLine::ForCurrentProcess();

  if (cmd_line->GetArgs().empty()) {
    std::cerr << "Usage: " << argv[0] << " [file]\n\n"
              << "Options:\n"
              << "  --" << switches::kEnableBitstreamConverter
              << " Enables H.264 Annex B bitstream conversion"
              << std::endl;
    return 1;
  }

  base::MessageLoop message_loop;
  DemuxerHostImpl demuxer_host;
  base::FilePath file_path(cmd_line->GetArgs()[0]);

  // Setup.
  media::FileDataSource data_source;
  CHECK(data_source.Initialize(file_path));

  media::FFmpegNeedKeyCB need_key_cb = base::Bind(&NeedKey);
  media::FFmpegDemuxer demuxer(
      message_loop.message_loop_proxy(), &data_source, need_key_cb);

  demuxer.Initialize(&demuxer_host, base::Bind(
      &QuitLoopWithStatus, &message_loop));
  message_loop.Run();

  StreamReader stream_reader(
      &demuxer, cmd_line->HasSwitch(switches::kEnableBitstreamConverter));

  // Benchmark.
  base::TimeTicks start = base::TimeTicks::HighResNow();
  while (!stream_reader.IsDone()) {
    stream_reader.Read();
  }
  base::TimeTicks end = base::TimeTicks::HighResNow();

  // Results.
  std::cout << "Time: " << (end - start).InMillisecondsF() << " ms\n";
  for (int i = 0; i < stream_reader.number_of_streams(); ++i) {
    media::DemuxerStream* stream = stream_reader.streams()[i];
    std::cout << "Stream #" << i << ": ";

    if (stream->type() == media::DemuxerStream::AUDIO) {
      std::cout << "audio";
    } else if (stream->type() == media::DemuxerStream::VIDEO) {
      std::cout << "video";
    } else {
      std::cout << "unknown";
    }

    std::cout << ", " << stream_reader.counts()[i] << " packets" << std::endl;
  }

  // Teardown.
  demuxer.Stop(base::Bind(
      &QuitLoopWithStatus, &message_loop, media::PIPELINE_OK));
  message_loop.Run();

  return 0;
}
