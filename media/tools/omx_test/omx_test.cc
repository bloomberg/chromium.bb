// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
#include "base/time.h"
#include "media/base/media.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/file_protocol.h"
#include "media/filters/bitstream_converter.h"
#include "media/omx/omx_codec.h"
#include "media/base/data_buffer.h"
#include "media/omx/omx_output_sink.h"
#include "media/tools/omx_test/color_space_util.h"
#include "media/tools/omx_test/file_reader_util.h"
#include "media/tools/omx_test/file_sink.h"

using media::BlockFileReader;
using media::FFmpegFileReader;
using media::FileReader;
using media::FileSink;
using media::H264FileReader;
using media::OmxCodec;
using media::OmxConfigurator;
using media::OmxDecoderConfigurator;
using media::OmxEncoderConfigurator;
using media::OmxOutputSink;
using media::YuvFileReader;
using media::Buffer;
using media::DataBuffer;

// This is the driver object to feed the decoder with data from a file.
// It also provides callbacks for the decoder to receive events from the
// decoder.
class TestApp {
 public:
  TestApp(OmxConfigurator* configurator, FileSink* file_sink,
          FileReader* file_reader)
      : configurator_(configurator),
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

  void StopCallback() {
    // If this callback is received, mark the |stopped_| flag so that we don't
    // feed more buffers into the decoder.
    // We need to exit the current message loop because we have no more work
    // to do on the message loop. This is done by calling
    // message_loop_.Quit().
    stopped_ = true;
    message_loop_.Quit();
  }

  void ErrorCallback() {
    // In case of error, this method is called. Mark the error flag and
    // exit the message loop because we have no more work to do.
    LOG(ERROR) << "Error callback received!";
    error_ = true;
    message_loop_.Quit();
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

  void FeedCallback(Buffer* buffer) {
    // We receive this callback when the decoder has consumed an input buffer.
    // In this case, delete the previous buffer and enqueue a new one.
    // There are some conditions we don't want to enqueue, for example when
    // the last buffer is an end-of-stream buffer, when we have stopped, and
    // when we have received an error.
    bool eos = buffer->IsEndOfStream();
    if (!eos && !stopped_ && !error_)
      FeedInputBuffer();
  }

  void ReadCompleteCallback(int buffer,
                            FileSink::BufferUsedCallback* callback) {
    // This callback is received when the decoder has completed a decoding
    // task and given us some output data. The buffer is owned by the decoder.
    if (stopped_ || error_)
      return;

    if (!frame_count_)
      first_sample_delivered_time_ = base::TimeTicks::HighResNow();

    // If we are readding to the end, then stop.
    if (buffer == OmxCodec::kEosBuffer) {
      codec_->Stop(NewCallback(this, &TestApp::StopCallback));
      return;
    }

    // Read one more from the decoder.
    codec_->Read(NewCallback(this, &TestApp::ReadCompleteCallback));

    if (file_sink_.get())
      file_sink_->BufferReady(buffer, callback);

    // could OMX IL return patial sample for decoder?
    frame_count_++;
  }

  void FeedInputBuffer() {
    uint8* data;
    int read;
    file_reader_->Read(&data, &read);
    codec_->Feed(new DataBuffer(data, read),
                 NewCallback(this, &TestApp::FeedCallback));
  }

  void Run() {
    StartProfiler();

    // Setup the |codec_| with the message loop of the current thread. Also
    // setup component name, codec format and callbacks.
    codec_ = new OmxCodec(&message_loop_);
    codec_->Setup(configurator_.get(), file_sink_.get());
    codec_->SetErrorCallback(NewCallback(this, &TestApp::ErrorCallback));
    codec_->SetFormatCallback(NewCallback(this, &TestApp::FormatCallback));

    // Start the |codec_|.
    codec_->Start();
    for (int i = 0; i < 20; ++i)
      FeedInputBuffer();
    codec_->Read(NewCallback(this, &TestApp::ReadCompleteCallback));

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

  scoped_refptr<OmxCodec> codec_;
  MessageLoop message_loop_;
  scoped_ptr<OmxConfigurator> configurator_;
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
  if (HasSwitch(name))
    return StringToInt(GetStringSwitch(name));
  return 0;
}

static bool PrepareDecodeFormats(OmxConfigurator::MediaFormat* input,
                                 OmxConfigurator::MediaFormat* output) {
  std::string codec = GetStringSwitch("codec");
  input->codec = OmxConfigurator::kCodecNone;
  if (codec == "h264") {
    input->codec = OmxConfigurator::kCodecH264;
  } else if (codec == "mpeg4") {
    input->codec = OmxConfigurator::kCodecMpeg4;
  } else if (codec == "h263") {
    input->codec = OmxConfigurator::kCodecH263;
  } else if (codec == "vc1") {
    input->codec = OmxConfigurator::kCodecVc1;
  } else {
    LOG(ERROR) << "Unknown codec.";
    return false;
  }
  output->codec = OmxConfigurator::kCodecRaw;
  return true;
}

static bool PrepareEncodeFormats(OmxConfigurator::MediaFormat* input,
                                 OmxConfigurator::MediaFormat* output) {
  input->codec = OmxConfigurator::kCodecRaw;
  input->video_header.width = GetIntSwitch("width");
  input->video_header.height = GetIntSwitch("height");
  input->video_header.frame_rate = GetIntSwitch("framerate");
  // TODO(jiesun): make other format available.
  output->codec = OmxConfigurator::kCodecMpeg4;
  output->video_header.width = GetIntSwitch("width");
  output->video_header.height = GetIntSwitch("height");
  output->video_header.frame_rate = GetIntSwitch("framerate");
  // TODO(jiesun): assume constant bitrate now.
  output->video_header.bit_rate = GetIntSwitch("bitrate");
  // TODO(jiesun): one I frame per second now. make it configurable.
  output->video_header.i_dist = output->video_header.frame_rate;
  // TODO(jiesun): disable B frame now. does they support it?
  output->video_header.p_dist = 0;
  return true;
}

static bool InitFFmpeg() {
  if (!media::InitializeMediaLibrary(FilePath()))
    return false;
  avcodec_init();
  av_register_all();
  av_register_protocol(&kFFmpegFileProtocol);
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

  // Read a bunch of parameters.
  std::string input_filename = GetStringSwitch("input-file");
  std::string output_filename = GetStringSwitch("output-file");
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

  // Set the media formats for I/O.
  OmxConfigurator::MediaFormat input, output;
  memset(&input, 0, sizeof(input));
  memset(&output, 0, sizeof(output));
  if (encoder)
    PrepareEncodeFormats(&input, &output);
  else
    PrepareDecodeFormats(&input, &output);

  // Creates the FileReader to read input file.
  FileReader* file_reader;
  if (encoder) {
    file_reader = new YuvFileReader(
        input_filename.c_str(), input.video_header.width,
        input.video_header.height, loop_count, enable_csc);
  } else if (use_ffmpeg) {
    // Use ffmepg for reading.
    file_reader = new FFmpegFileReader(input_filename.c_str());
  } else if (EndsWith(input_filename, ".264", false)) {
    file_reader = new H264FileReader(input_filename.c_str());
  } else {
    // Creates a reader that reads in blocks of 32KB.
    const int kReadSize = 32768;
    file_reader = new BlockFileReader(input_filename.c_str(), kReadSize);
  }

  // Create the configurator.
  OmxConfigurator* configurator;
  if (encoder)
    configurator = new OmxEncoderConfigurator(input, output);
  else
    configurator = new OmxDecoderConfigurator(input, output);

  // Create a file sink.
  FileSink* file_sink = new FileSink(output_filename, copy, enable_csc);

  // Create a test app object and initialize it.
  TestApp test(configurator, file_sink, file_reader);
  if (!test.Initialize()) {
    LOG(ERROR) << "can't initialize this application";
    return -1;
  }

  // This will run the decoder until EOS is reached or an error
  // is encountered.
  test.Run();
  return 0;
}
