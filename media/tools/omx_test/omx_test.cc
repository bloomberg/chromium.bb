// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// A test program that drives an OpenMAX video decoder module. This program
// will take video in elementary stream and read into the decoder.
//
// Run the following command to see usage:
// ./omx_test

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/scoped_handle.h"
#include "base/time.h"
#include "media/base/media.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/file_protocol.h"
#include "media/filters/bitstream_converter.h"
#include "media/omx/input_buffer.h"
#include "media/omx/omx_codec.h"
#include "media/tools/omx_test/color_space_util.h"
#include "media/tools/omx_test/file_reader_util.h"

// This is the driver object to feed the decoder with data from a file.
// It also provides callbacks for the decoder to receive events from the
// decoder.
class TestApp {
 public:
  TestApp(const char* input_filename,
          const char* output_filename,
          media::OmxCodec::OmxMediaFormat& input_format,
          media::OmxCodec::OmxMediaFormat& output_format,
          bool simulate_copy,
          bool enable_csc,
          bool use_ffmpeg,
          int loop_count)
      : output_filename_(output_filename),
        input_format_(input_format),
        output_format_(output_format),
        simulate_copy_(simulate_copy),
        enable_csc_(enable_csc),
        copy_buf_size_(0),
        csc_buf_size_(0),
        output_file_(NULL),
        stopped_(false),
        error_(false) {
    // Creates the FileReader to read input file.
    if (input_format_.codec == media::OmxCodec::kCodecRaw) {
      file_reader_.reset(new media::YuvFileReader(
          input_filename,
          input_format.video_header.width,
          input_format.video_header.height,
          loop_count,
          enable_csc));
    } else if (use_ffmpeg) {
      file_reader_.reset(
          new media::FFmpegFileReader(input_filename));
    } else {
      // Creates a reader that reads in blocks of 32KB.
      const int kReadSize = 32768;
      file_reader_.reset(
          new media::BlockFileReader(input_filename, kReadSize));
    }
  }

  bool Initialize() {
    if (!file_reader_->Initialize()) {
      file_reader_.reset();
      LOG(ERROR) << "can't initialize file reader";
      return false;;
    }

    // Opens the output file for writing.
    if (!output_filename_.empty()) {
      output_file_.Set(file_util::OpenFile(output_filename_, "wb"));
      if (!output_file_.get()) {
        LOG(ERROR) << "can't open dump file %s" << output_filename_;
        return false;
      }
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
      media::OmxCodec::OmxMediaFormat* input_format,
      media::OmxCodec::OmxMediaFormat* output_format) {
    // This callback will be called when port reconfiguration is done.
    // Input format and output format will be used in the codec.

    // Make a copy of the changed format.
    input_format_ = *input_format;
    output_format_ = *output_format;

    DCHECK_EQ(input_format->video_header.width,
              output_format->video_header.width);
    DCHECK_EQ(input_format->video_header.height,
              output_format->video_header.height);
    int size = input_format_.video_header.width *
               input_format_.video_header.height * 3 / 2;
    if (enable_csc_ && size > csc_buf_size_) {
      csc_buf_.reset(new uint8[size]);
      csc_buf_size_ = size;
    }
  }

  void FeedCallback(media::InputBuffer* buffer) {
    // We receive this callback when the decoder has consumed an input buffer.
    // In this case, delete the previous buffer and enqueue a new one.
    // There are some conditions we don't want to enqueue, for example when
    // the last buffer is an end-of-stream buffer, when we have stopped, and
    // when we have received an error.
    bool eos = buffer->IsEndOfStream();
    delete buffer;
    if (!eos && !stopped_ && !error_)
      FeedInputBuffer();
  }

  void ReadCompleteCallback(uint8* buffer, int size) {
    // This callback is received when the decoder has completed a decoding
    // task and given us some output data. The buffer is owned by the decoder.
    if (stopped_ || error_)
      return;

    if (!frame_count_)
      first_sample_delivered_time_ = base::TimeTicks::HighResNow();

    // If we are readding to the end, then stop.
    if (!size) {
      codec_->Stop(NewCallback(this, &TestApp::StopCallback));
      return;
    }

    // Read one more from the decoder.
    codec_->Read(NewCallback(this, &TestApp::ReadCompleteCallback));

    // Copy the output of the decoder to user memory.
    if (simulate_copy_ || output_file_.get()) {  // Implies a copy.
      if (size > copy_buf_size_) {
        copy_buf_.reset(new uint8[size]);
        copy_buf_size_ = size;
      }
      memcpy(copy_buf_.get(), buffer, size);
      if (output_file_.get())
        DumpOutputFile(copy_buf_.get(), size);
    }

    // could OMX IL return patial sample for decoder?
    frame_count_++;
    bit_count_ += size << 3;
  }

  void FeedInputBuffer() {
    uint8* data;
    int read;
    file_reader_->Read(&data, &read);
    codec_->Feed(new media::InputBuffer(data, read),
                 NewCallback(this, &TestApp::FeedCallback));
  }

  void Run() {
    StartProfiler();

    // Setup the |codec_| with the message loop of the current thread. Also
    // setup component name, codec format and callbacks.
    codec_ = new media::OmxCodec(&message_loop_);
    codec_->Setup(input_format_, output_format_);
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
    bit_count_ = 0;
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
    // printf("\n<<< bitrate>>> : %I64d\n", bit_count_ * 1000000 / micro_sec);
    printf("\n");
  }

  void DumpOutputFile(uint8* in_buffer, int size) {
    // Assume chroma format 4:2:0.
    int width = input_format_.video_header.width;
    int height = input_format_.video_header.height;
    DCHECK_GT(width, 0);
    DCHECK_GT(height, 0);

    uint8* out_buffer = in_buffer;
    // Color space conversion.
    bool encoder = input_format_.codec == media::OmxCodec::kCodecRaw;
    if (enable_csc_ && !encoder) {
      DCHECK_EQ(size, width * height * 3 / 2);
      DCHECK_GE(csc_buf_size_, size);
      out_buffer = csc_buf_.get();
      // Now assume the raw output is NV21.
      media::NV21toIYUV(in_buffer, out_buffer, width, height);
    }
    fwrite(out_buffer, sizeof(uint8), size, output_file_.get());
  }

  scoped_refptr<media::OmxCodec> codec_;
  MessageLoop message_loop_;
  std::string output_filename_;
  media::OmxCodec::OmxMediaFormat input_format_;
  media::OmxCodec::OmxMediaFormat output_format_;
  bool simulate_copy_;
  bool enable_csc_;
  scoped_array<uint8> copy_buf_;
  int copy_buf_size_;
  scoped_array<uint8> csc_buf_;
  int csc_buf_size_;
  ScopedStdioHandle output_file_;
  bool stopped_;
  bool error_;
  base::TimeTicks start_time_;
  base::TimeTicks first_sample_delivered_time_;
  int frame_count_;
  int bit_count_;
  scoped_ptr<media::FileReader> file_reader_;
};

int main(int argc, char** argv) {
  base::AtExitManager at_exit_manager;

  CommandLine::Init(argc, argv);
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();

  bool encoder = cmd_line->HasSwitch("encoder");
  if (!encoder) {
    if (argc < 3) {
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
      return 1;
    }
  } else {
    if (argc < 7) {
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
      return 1;
    }
  }

  std::string input_filename = cmd_line->GetSwitchValueASCII("input-file");
  std::string output_filename = cmd_line->GetSwitchValueASCII("output-file");
  std::string codec = cmd_line->GetSwitchValueASCII("codec");
  bool copy = cmd_line->HasSwitch("copy");
  bool enable_csc = cmd_line->HasSwitch("enable-csc");
  bool use_ffmpeg = cmd_line->HasSwitch("use-ffmpeg");
  int loop_count = 1;
  if (cmd_line->HasSwitch("loop"))
    loop_count = StringToInt(cmd_line->GetSwitchValueASCII("loop"));
  DCHECK_GE(loop_count, 1);

  // If FFmpeg should be used for demuxing load the library here and do
  // the initialization.
  if (use_ffmpeg) {
    if (!media::InitializeMediaLibrary(FilePath())) {
      LOG(ERROR) << "Unable to initialize the media library.";
      return 1;
    }

    avcodec_init();
    av_register_all();
    av_register_protocol(&kFFmpegFileProtocol);
  }

  media::OmxCodec::OmxMediaFormat input, output;
  memset(&input, 0, sizeof(input));
  memset(&output, 0, sizeof(output));
  if (encoder) {
    input.codec = media::OmxCodec::kCodecRaw;
    // TODO(jiesun): make other format available.
    output.codec = media::OmxCodec::kCodecMpeg4;
    output.video_header.width = input.video_header.width =
        StringToInt(cmd_line->GetSwitchValueASCII("width"));
    output.video_header.height = input.video_header.height =
        StringToInt(cmd_line->GetSwitchValueASCII("height"));
    output.video_header.frame_rate = input.video_header.frame_rate =
        StringToInt(cmd_line->GetSwitchValueASCII("framerate"));
    // TODO(jiesun): assume constant bitrate now.
    output.video_header.bit_rate =
        StringToInt(cmd_line->GetSwitchValueASCII("bitrate"));
    // TODO(jiesun): one I frame per second now. make it configurable.
    output.video_header.i_dist = output.video_header.frame_rate;
    // TODO(jiesun): disable B frame now. does they support it?
    output.video_header.p_dist = 0;
  } else {
    input.codec = media::OmxCodec::kCodecNone;
    if (codec == "h264") {
      input.codec = media::OmxCodec::kCodecH264;
    } else if (codec == "mpeg4") {
      input.codec = media::OmxCodec::kCodecMpeg4;
    } else if (codec == "h263") {
      input.codec = media::OmxCodec::kCodecH263;
    } else if (codec == "vc1") {
      input.codec = media::OmxCodec::kCodecVc1;
    } else {
      LOG(ERROR) << "Unknown codec.";
      return 1;
    }
    output.codec = media::OmxCodec::kCodecRaw;
  }

  // Create a TestApp object and run the decoder.
  TestApp test(input_filename.c_str(),
               output_filename.c_str(),
               input,
               output,
               copy,
               enable_csc,
               use_ffmpeg,
               loop_count);

  // This call will run the decoder until EOS is reached or an error
  // is encountered.
  if (!test.Initialize()) {
    LOG(ERROR) << "can't initialize this application";
    return -1;
  }
  test.Run();
  return 0;
}
