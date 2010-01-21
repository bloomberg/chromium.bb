// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// A test program that drives an OpenMAX video decoder module. This program
// will take video in elementary stream and read into the decoder.
// Usage of this program:
// ./omx_test --file=<file> --component=<component> --codec=<codec>
//     <file> = Input file name
//     <component> = Name of the OpenMAX component
//     <codec> = Codec to be used, available codecs: h264, vc1, mpeg4, h263.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "media/omx/input_buffer.h"
#include "media/omx/omx_codec.h"

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
          bool measure_fps,
          bool enable_csc,
          int loop_count)
      : input_filename_(input_filename),
        output_filename_(output_filename),
        input_format_(input_format),
        output_format_(output_format),
        simulate_copy_(simulate_copy),
        measure_fps_(measure_fps),
        enable_csc_(enable_csc),
        copy_buf_size_(0),
        csc_buf_size_(0),
        input_file_(NULL),
        output_file_(NULL),
        stopped_(false),
        error_(false),
        loop_count_(loop_count) {
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
    printf("Error callback received!\n");
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

    if (measure_fps_ && !frame_count_)
      first_sample_delivered_time_ = base::TimeTicks::HighResNow();

    // If we are readding to the end, then stop.
    if (!size) {
      codec_->Stop(NewCallback(this, &TestApp::StopCallback));
      return;
    }

    // Read one more from the decoder.
    codec_->Read(NewCallback(this, &TestApp::ReadCompleteCallback));

    // Copy the output of the decoder to user memory.
    if (simulate_copy_ || output_file_) {  // |output_file_| implies a copy.
      if (size > copy_buf_size_) {
        copy_buf_.reset(new uint8[size]);
        copy_buf_size_ = size;
      }
      memcpy(copy_buf_.get(), buffer, size);
      if (output_file_)
        DumpOutputFile(copy_buf_.get(), size);
    }

    // could OMX IL return patial sample for decoder?
    frame_count_++;
    bit_count_ += size << 3;
  }

  void ReadInputFileYuv(uint8** output, int* size) {
    while (true) {
      uint8* data = NULL;
      int bytes_read = 0;
      // OMX require encoder input are delivered in frames (or planes).
      // Assume the input file is I420 YUV file.
      int width = input_format_.video_header.width;
      int height = input_format_.video_header.height;
      int frame_size = width * height * 3 / 2;
      data = new uint8[frame_size];
      if (enable_csc_) {
        CHECK(csc_buf_size_ >= frame_size);
        bytes_read = fread(csc_buf_.get(), 1,
                           frame_size, input_file_);
        // We do not convert partial frames.
        if (bytes_read == frame_size)
          IYUVtoNV21(csc_buf_.get(), data, width, height);
        else
          bytes_read = 0;  // force cleanup or loop around.
      } else {
        bytes_read = fread(data, 1, frame_size, input_file_);
      }

      if (bytes_read) {
        *size = bytes_read;
        *output = data;
        break;
      } else {
        // Encounter the end of file.
        if (loop_count_ == 1) {
          // Signal end of stream.
          *size = 0;
          *output = data;
          break;
        } else {
          --loop_count_;
          delete [] data;
          fseek(input_file_, 0, SEEK_SET);
        }
      }
    }
  }

  void ReadInputFileArbitrary(uint8** data, int* size) {
      // Feeds the decoder with 32KB of input data.
      const int kSize = 32768;
      *data = new uint8[kSize];
      *size = fread(*data, 1, kSize, input_file_);
  }

  void ReadInputFileH264(uint8** data, int* size) {
    const int kSize = 1024 * 1024;
    static int current = 0;
    static int used = 0;

    // Allocate read buffer.
    if (!read_buf_.get())
      read_buf_.reset(new uint8[kSize]);

    // Fill the buffer when it's less than half full.
    int read = 0;
    if (used < kSize / 2) {
      read = fread(read_buf_.get(), 1, kSize - used, input_file_);
      CHECK(read >= 0);
      used += read;
    }

    // If we failed to read.
    if (current == read) {
      *data = new uint8[1];
      *size = 0;
      return;
    } else {
      // Try to find start code of 0x00, 0x00, 0x01.
      bool found = false;
      int pos = current + 3;
      for (; pos < used - 2; ++pos) {
        if (read_buf_[pos] == 0 &&
            read_buf_[pos+1] == 0 &&
            read_buf_[pos+2] == 1) {
          found = true;
          break;
        }
      }
      if (found) {
        CHECK(pos > current);
        *size = pos - current;
        *data = new uint8[*size];
        memcpy(*data, read_buf_.get() + current, *size);
        current = pos;
      } else {
        CHECK(used > current);
        *size = used - current;
        *data = new uint8[*size];
        memcpy(*data, read_buf_.get() + current, *size);
        current = used;
      }
      if (used - current < current) {
        CHECK(used > current);
        memcpy(read_buf_.get(),
               read_buf_.get() + current,
               used - current);
        used = used - current;
        current = 0;
      }
      return;
    }
  }

  void FeedInputBuffer() {
    uint8* data;
    int read;
    if (input_format_.codec == media::OmxCodec::kCodecRaw)
      ReadInputFileYuv(&data, &read);
    else if (input_format_.codec == media::OmxCodec::kCodecH264)
      ReadInputFileH264(&data, &read);
    else
      ReadInputFileArbitrary(&data, &read);
    codec_->Feed(new media::InputBuffer(data, read),
                 NewCallback(this, &TestApp::FeedCallback));
  }

  void Run() {
    // Open the input file.
    input_file_ = file_util::OpenFile(input_filename_, "rb");
    if (!input_file_) {
      printf("Error - can't open file %s\n", input_filename_);
      return;
    }

    // Open the dump file.
    if (strlen(output_filename_)) {
      output_file_ = file_util::OpenFile(output_filename_, "wb");
      if (!input_file_) {
        fclose(input_file_);
        printf("Error - can't open dump file %s\n", output_filename_);
        return;
      }
    }

    if (measure_fps_)
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

    if (measure_fps_)
      StopProfiler();

    fclose(input_file_);
    if (output_file_)
      fclose(output_file_);
  }

  void StartProfiler() {
    start_time_ = base::TimeTicks::HighResNow();
    frame_count_ = 0;
    bit_count_ = 0;
  }

  void StopProfiler() {
    printf("\n<<< frame delivered : %d >>>", frame_count_);
    stop_time_ = base::TimeTicks::HighResNow();
    base::TimeDelta duration = stop_time_ - start_time_;
    int64 micro_sec = duration.InMicroseconds();
    int64 fps = (static_cast<int64>(frame_count_) *
                base::Time::kMicrosecondsPerSecond) / micro_sec;
    printf("\n<<< time used(us) : %d >>>", static_cast<int>(micro_sec));
    printf("\n<<< fps : %d >>>", static_cast<int>(fps));
    duration = first_sample_delivered_time_ - start_time_;
    micro_sec = duration.InMicroseconds();
    printf("\n<<< initial delay used(us): %d >>>", static_cast<int>(micro_sec));
    // printf("\n<<< bitrate>>> : %I64d\n", bit_count_ * 1000000 / micro_sec);
    printf("\n");
  }

  // Not intended to be used in production.
  static void NV21toIYUV(uint8* nv21, uint8* i420, int width, int height) {
    memcpy(i420, nv21, width * height * sizeof(uint8));
    i420 += width * height;
    nv21 += width * height;
    uint8* u = i420;
    uint8* v = i420 + width * height / 4;

    for (int i = 0; i < width * height / 4; ++i) {
      *v++ = *nv21++;
      *u++ = *nv21++;
    }
  }

  static void NV21toYV12(uint8* nv21, uint8* yv12, int width, int height) {
    memcpy(yv12, nv21, width * height * sizeof(uint8));
    yv12 += width * height;
    nv21 += width * height;
    uint8* v = yv12;
    uint8* u = yv12 + width * height / 4;

    for (int i = 0; i < width * height / 4; ++i) {
      *v++ = *nv21++;
      *u++ = *nv21++;
    }
  }

  static void IYUVtoNV21(uint8* i420, uint8* nv21, int width, int height) {
    memcpy(nv21, i420, width * height * sizeof(uint8));
    i420 += width * height;
    nv21 += width * height;
    uint8* u = i420;
    uint8* v = i420 + width * height / 4;

    for (int i = 0; i < width * height / 4; ++i) {
      *nv21++ = *v++;
      *nv21++ = *u++;
    }
  }

  static void YV12toNV21(uint8* yv12, uint8* nv21, int width, int height) {
    memcpy(nv21, yv12, width * height * sizeof(uint8));
    yv12 += width * height;
    nv21 += width * height;
    uint8* v = yv12;
    uint8* u = yv12 + width * height / 4;

    for (int i = 0; i < width * height / 4; ++i) {
      *nv21++ = *v++;
      *nv21++ = *u++;
    }
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
      NV21toIYUV(in_buffer, out_buffer, width, height);
    }
    fwrite(out_buffer, sizeof(uint8), size, output_file_);
  }

  scoped_refptr<media::OmxCodec> codec_;
  MessageLoop message_loop_;
  const char* input_filename_;
  const char* output_filename_;
  media::OmxCodec::OmxMediaFormat input_format_;
  media::OmxCodec::OmxMediaFormat output_format_;
  bool simulate_copy_;
  bool measure_fps_;
  bool enable_csc_;
  scoped_array<uint8> copy_buf_;
  int copy_buf_size_;
  scoped_array<uint8> csc_buf_;
  int csc_buf_size_;
  FILE *input_file_, *output_file_;
  scoped_array<uint8> read_buf_;
  bool stopped_;
  bool error_;
  base::TimeTicks start_time_;
  base::TimeTicks stop_time_;
  base::TimeTicks first_sample_delivered_time_;
  int frame_count_;
  int bit_count_;
  int loop_count_;
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
             " [--copy] [--measure-fps]\n");
      printf("    CODEC: h264/mpeg4/h263/vc1\n");
      printf("\n");
      printf("Optional Arguments\n");
      printf("    --output-file Dump raw OMX output to file.\n");
      printf("    --enable-csc  Dump the CSCed output to file.\n");
      printf("    --copy        Simulate a memcpy from the output.\n");
      printf("    --measure-fps Measuring performance in fps\n");
      printf("    --loop=COUNT  loop input stream\n");
      return 1;
    }
  } else {
    if (argc < 7) {
      printf("Usage: omx_test --input-file=FILE --codec=CODEC"
             " --width=PIXEL_WIDTH --height=PIXEL_HEIGHT"
             " --bitrate=BIT_PER_SECOND --framerate=FRAME_PER_SECOND"
             " [--output-file=FILE] [--enable-csc]"
             " [--copy] [--measure-fps]\n");
      printf("    CODEC: h264/mpeg4/h263/vc1\n");
      printf("\n");
      printf("Optional Arguments\n");
      printf("    --output-file Dump raw OMX output to file.\n");
      printf("    --enable-csc  Dump the CSCed input from file.\n");
      printf("    --copy        Simulate a memcpy from the output.\n");
      printf("    --measure-fps Measuring performance in fps\n");
      printf("    --loop=COUNT  loop input streams\n");
      return 1;
    }
  }

  std::string input_filename = cmd_line->GetSwitchValueASCII("input-file");
  std::string output_filename = cmd_line->GetSwitchValueASCII("output-file");
  std::string codec = cmd_line->GetSwitchValueASCII("codec");
  bool copy = cmd_line->HasSwitch("copy");
  bool measure_fps = cmd_line->HasSwitch("measure-fps");
  bool enable_csc = cmd_line->HasSwitch("enable-csc");
  int loop_count = 1;
  if (cmd_line->HasSwitch("loop"))
    loop_count = StringToInt(cmd_line->GetSwitchValueASCII("loop"));
  DCHECK_GE(loop_count, 1);

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
    if (codec == "h264")
      input.codec = media::OmxCodec::kCodecH264;
    else if (codec == "mpeg4")
      input.codec = media::OmxCodec::kCodecMpeg4;
    else if (codec == "h263")
      input.codec = media::OmxCodec::kCodecH263;
    else if (codec == "vc1")
      input.codec = media::OmxCodec::kCodecVc1;
    else {
      printf("Unknown codec.\n");
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
               measure_fps,
               enable_csc,
               loop_count);

  // This call will run the decoder until EOS is reached or an error
  // is encountered.
  test.Run();
  return 0;
}
