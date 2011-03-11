// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A test program that drives an OpenMAX video decoder module. This program
// will take video in elementary stream and read into the decoder.
//
// Run the following command to see usage:
// ./omx_test

#include "base/at_exit.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "media/base/data_buffer.h"
#include "media/base/media.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/file_protocol.h"
#include "media/filters/bitstream_converter.h"
#include "media/tools/omx_test/color_space_util.h"
#include "media/tools/omx_test/file_reader_util.h"
#include "media/tools/omx_test/file_sink.h"
#include "media/video/omx_video_decode_engine.h"

using media::BlockFileReader;
using media::Buffer;
using media::DataBuffer;
using media::FFmpegFileReader;
using media::FileReader;
using media::FileSink;
using media::H264FileReader;
using media::OmxConfigurator;
using media::OmxDecoderConfigurator;
using media::OmxEncoderConfigurator;
using media::OmxVideoDecodeEngine;
using media::PipelineStatistics;
using media::VideoFrame;
using media::YuvFileReader;

// This is the driver object to feed the decoder with data from a file.
// It also provides callbacks for the decoder to receive events from the
// decoder.
// TODO(wjia): AVStream should be replaced with a new structure which is
// neutral to any video decoder. Also change media.gyp correspondingly.
class TestApp : public base::RefCountedThreadSafe<TestApp>,
                public media::VideoDecodeEngine::EventHandler {
 public:
  TestApp(AVStream* av_stream,
          FileSink* file_sink,
          FileReader* file_reader)
      : av_stream_(av_stream),
        file_reader_(file_reader),
        file_sink_(file_sink),
        stopped_(false),
        error_(false) {
  }

  bool Initialize() {
    if (!file_reader_->Initialize()) {
      file_reader_.reset();
      LOG(ERROR) << "can't initialize file reader";
      return false;;
    }

    if (!file_sink_->Initialize()) {
      LOG(ERROR) << "can't initialize output writer";
      return false;
    }
    return true;
  }

  virtual void OnInitializeComplete(const media::VideoCodecInfo& info) {}

  virtual void OnUninitializeComplete() {
    // If this callback is received, mark the |stopped_| flag so that we don't
    // feed more buffers into the decoder.
    // We need to exit the current message loop because we have no more work
    // to do on the message loop. This is done by calling
    // message_loop_.Quit().
    stopped_ = true;
    message_loop_.Quit();
  }

  virtual void OnError() {
    // In case of error, this method is called. Mark the error flag and
    // exit the message loop because we have no more work to do.
    LOG(ERROR) << "Error callback received!";
    error_ = true;
    message_loop_.Quit();
  }

  virtual void OnFlushComplete() {
    NOTIMPLEMENTED();
  }

  virtual void OnSeekComplete() {
    NOTIMPLEMENTED();
  }

  virtual void OnFormatChange(media::VideoStreamInfo stream_info) {
    NOTIMPLEMENTED();
  }

  void FormatCallback(
      const OmxConfigurator::MediaFormat& input_format,
      const OmxConfigurator::MediaFormat& output_format) {
    // This callback will be called when port reconfiguration is done.
    // Input format and output format will be used in the codec.

    DCHECK_EQ(input_format.video_header.width,
              output_format.video_header.width);
    DCHECK_EQ(input_format.video_header.height,
              output_format.video_header.height);

    file_sink_->UpdateSize(input_format.video_header.width,
                             input_format.video_header.height);
  }

  virtual void ProduceVideoSample(scoped_refptr<Buffer> buffer) {
    // We receive this callback when the decoder has consumed an input buffer.
    // In this case, delete the previous buffer and enqueue a new one.
    // There are some conditions we don't want to enqueue, for example when
    // the last buffer is an end-of-stream buffer, when we have stopped, and
    // when we have received an error.
    bool eos = buffer.get() && buffer->IsEndOfStream();
    if (!eos && !stopped_ && !error_)
      FeedInputBuffer();
  }

  virtual void ConsumeVideoFrame(scoped_refptr<VideoFrame> frame,
                                 const PipelineStatistics& statistics) {
    // This callback is received when the decoder has completed a decoding
    // task and given us some output data. The frame is owned by the decoder.
    if (stopped_ || error_)
      return;

    if (!frame_count_)
      first_sample_delivered_time_ = base::TimeTicks::HighResNow();

    // If we are reading to the end, then stop.
    if (frame->IsEndOfStream()) {
      engine_->Uninitialize();
      return;
    }

    if (file_sink_.get()) {
      for (size_t i = 0; i < frame->planes(); i++) {
        int plane_size = frame->width() * frame->height();
        if (i > 0) plane_size >>= 2;
        file_sink_->BufferReady(plane_size, frame->data(i));
      }
    }

    // could OMX IL return patial sample for decoder?
    frame_count_++;
  }

  void FeedInputBuffer() {
    uint8* data;
    int read;
    file_reader_->Read(&data, &read);
    engine_->ConsumeVideoSample(new DataBuffer(data, read));
  }

  void Run() {
    StartProfiler();

    media::VideoCodecConfig config(
        media::CodecIDToVideoCodec(av_stream_->codec->codec_id),
        av_stream_->codec->coded_width,
        av_stream_->codec->coded_height,
        av_stream_->r_frame_rate.num,
        av_stream_->r_frame_rate.den,
        av_stream_->codec->extradata,
        av_stream_->codec->extradata_size);

    engine_.reset(new OmxVideoDecodeEngine());
    engine_->Initialize(&message_loop_, this, NULL, config);

    // Execute the message loop so that we can run tasks on it. This call
    // will return when we call message_loop_.Quit().
    message_loop_.Run();

    StopProfiler();
  }

  void StartProfiler() {
    start_time_ = base::TimeTicks::HighResNow();
    frame_count_ = 0;
  }

  void StopProfiler() {
    base::TimeDelta duration = base::TimeTicks::HighResNow() - start_time_;
    int64 duration_ms = duration.InMilliseconds();
    int64 fps = 0;
    if (duration_ms) {
      fps = (static_cast<int64>(frame_count_) *
             base::Time::kMillisecondsPerSecond) / duration_ms;
    }
    base::TimeDelta delay = first_sample_delivered_time_ - start_time_;
    printf("\n<<< frame delivered : %d >>>", frame_count_);
    printf("\n<<< time used(ms) : %d >>>", static_cast<int>(duration_ms));
    printf("\n<<< fps : %d >>>", static_cast<int>(fps));
    printf("\n<<< initial delay used(us): %d >>>",
           static_cast<int>(delay.InMicroseconds()));
    printf("\n");
  }

  scoped_ptr<OmxVideoDecodeEngine> engine_;
  MessageLoop message_loop_;
  scoped_ptr<AVStream> av_stream_;
  scoped_ptr<FileReader> file_reader_;
  scoped_ptr<FileSink> file_sink_;

  // Internal states for execution.
  bool stopped_;
  bool error_;

  // Counters for performance.
  base::TimeTicks start_time_;
  base::TimeTicks first_sample_delivered_time_;
  int frame_count_;
};

static std::string GetStringSwitch(const char* name) {
  return CommandLine::ForCurrentProcess()->GetSwitchValueASCII(name);
}

static bool HasSwitch(const char* name) {
  return CommandLine::ForCurrentProcess()->HasSwitch(name);
}

static int GetIntSwitch(const char* name) {
  if (HasSwitch(name)) {
    int val;
    base::StringToInt(GetStringSwitch(name), &val);
    return val;
  }
  return 0;
}

static bool PrepareDecodeFormats(AVStream *av_stream) {
  std::string codec = GetStringSwitch("codec");
  av_stream->codec->codec_id = CODEC_ID_NONE;
  if (codec == "h264") {
    av_stream->codec->codec_id = CODEC_ID_H264;
  } else if (codec == "mpeg4") {
    av_stream->codec->codec_id = CODEC_ID_MPEG4;
  } else if (codec == "h263") {
    av_stream->codec->codec_id = CODEC_ID_H263;
  } else if (codec == "vc1") {
    av_stream->codec->codec_id = CODEC_ID_VC1;
  } else {
    LOG(ERROR) << "Unknown codec.";
    return false;
  }
  return true;
}

static bool PrepareEncodeFormats(AVStream *av_stream) {
  av_stream->codec->width = GetIntSwitch("width");
  av_stream->codec->height = GetIntSwitch("height");
  av_stream->avg_frame_rate.num = GetIntSwitch("framerate");
  av_stream->avg_frame_rate.den = 1;

  std::string codec = GetStringSwitch("codec");
  av_stream->codec->codec_id = CODEC_ID_NONE;
  if (codec == "h264") {
    av_stream->codec->codec_id = CODEC_ID_H264;
  } else if (codec == "mpeg4") {
    av_stream->codec->codec_id = CODEC_ID_MPEG4;
  } else if (codec == "h263") {
    av_stream->codec->codec_id = CODEC_ID_H263;
  } else if (codec == "vc1") {
    av_stream->codec->codec_id = CODEC_ID_VC1;
  } else {
    LOG(ERROR) << "Unknown codec.";
    return false;
  }
  // TODO(jiesun): assume constant bitrate now.
  av_stream->codec->bit_rate = GetIntSwitch("bitrate");

  // TODO(wjia): add more configurations needed by encoder
  return true;
}

static bool InitFFmpeg() {
  if (!media::InitializeMediaLibrary(FilePath()))
    return false;
  avcodec_init();
  av_register_all();
  av_register_protocol2(&kFFmpegFileProtocol, sizeof(kFFmpegFileProtocol));
  return true;
}

static void PrintHelp() {
  printf("Using for decoding...\n");
  printf("\n");
  printf("Usage: omx_test --input-file=FILE --codec=CODEC"
             " [--output-file=FILE] [--enable-csc]"
             " [--copy] [--use-ffmpeg]\n");
  printf("    CODEC: h264/mpeg4/h263/vc1\n");
  printf("\n");
  printf("Optional Arguments\n");
  printf("    --output-file Dump raw OMX output to file.\n");
  printf("    --enable-csc  Dump the CSCed output to file.\n");
  printf("    --copy        Simulate a memcpy from the output.\n");
  printf("    --use-ffmpeg  Use ffmpeg demuxer\n");
  printf("\n");
  printf("Using for encoding...\n");
  printf("\n");
  printf("Usage: omx_test --encoder --input-file=FILE --codec=CODEC"
         " --width=PIXEL_WIDTH --height=PIXEL_HEIGHT"
         " --bitrate=BIT_PER_SECOND --framerate=FRAME_PER_SECOND"
         " [--output-file=FILE] [--enable-csc]"
         " [--copy]\n");
  printf("    CODEC: h264/mpeg4/h263/vc1\n");
  printf("\n");
  printf("Optional Arguments\n");
  printf("    --output-file Dump raw OMX output to file.\n");
  printf("    --enable-csc  Dump the CSCed input from file.\n");
  printf("    --copy        Simulate a memcpy from the output.\n");
  printf("    --loop=COUNT  loop input streams\n");
}

int main(int argc, char** argv) {
  base::AtExitManager at_exit_manager;
  CommandLine::Init(argc, argv);

  // Print help if there is not enough arguments.
  if (argc == 1) {
    PrintHelp();
    return -1;
  }

  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
  // Read a bunch of parameters.
  FilePath input_path = cmd_line.GetSwitchValuePath("input-file");
  FilePath output_path = cmd_line.GetSwitchValuePath("output-file");
  bool encoder = HasSwitch("encoder");
  bool copy = HasSwitch("copy");
  bool enable_csc = HasSwitch("enable-csc");
  bool use_ffmpeg = HasSwitch("use-ffmpeg");
  int loop_count = GetIntSwitch("loop");
  if (loop_count == 0)
    loop_count = 1;
  DCHECK_GE(loop_count, 1);

  // Initialize OpenMAX.
  if (!media::InitializeOpenMaxLibrary(FilePath())) {
    LOG(ERROR) << "Unable to initialize OpenMAX library.";
    return false;
  }

  // If FFmpeg should be used for demuxing load the library here and do
  // the initialization.
  if (use_ffmpeg && !InitFFmpeg()) {
    LOG(ERROR) << "Unable to initialize the media library.";
    return -1;
  }

  // Create AVStream
  AVStream *av_stream = new AVStream;
  AVCodecContext *av_codec_context = new AVCodecContext;
  memset(av_stream, 0, sizeof(AVStream));
  memset(av_codec_context, 0, sizeof(AVCodecContext));
  scoped_ptr<AVCodecContext> av_codec_context_deleter(av_codec_context);
  av_stream->codec = av_codec_context;
  av_codec_context->width = 320;
  av_codec_context->height = 240;
  if (encoder)
    PrepareEncodeFormats(av_stream);
  else
    PrepareDecodeFormats(av_stream);

  // Creates the FileReader to read input file.
  FileReader* file_reader;
  if (encoder) {
    file_reader = new YuvFileReader(
        input_path, av_stream->codec->width,
        av_stream->codec->height, loop_count, enable_csc);
  } else if (use_ffmpeg) {
    // Use ffmepg for reading.
    file_reader = new FFmpegFileReader(input_path);
  } else if (input_path.Extension() == FILE_PATH_LITERAL(".264")) {
    file_reader = new H264FileReader(input_path);
  } else {
    // Creates a reader that reads in blocks of 32KB.
    const int kReadSize = 32768;
    file_reader = new BlockFileReader(input_path, kReadSize);
  }

  // Create a file sink.
  FileSink* file_sink = new FileSink(output_path, copy, enable_csc);

  // Create a test app object and initialize it.
  scoped_refptr<TestApp> test = new TestApp(av_stream, file_sink, file_reader);
  if (!test->Initialize()) {
    LOG(ERROR) << "can't initialize this application";
    return -1;
  }

  // This will run the decoder until EOS is reached or an error
  // is encountered.
  test->Run();
  return 0;
}
