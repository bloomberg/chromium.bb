// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
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
          const char* component,
          media::OmxCodec::OmxMediaFormat& input_format,
          media::OmxCodec::OmxMediaFormat& output_format,
          bool simulate_copy)
      : input_filename_(input_filename),
        output_filename_(output_filename),
        component_(component),
        input_format_(input_format),
        output_format_(output_format),
        simulate_copy_(simulate_copy),
        copy_buf_size_(0),
        input_file_(NULL),
        output_file_(NULL),
        stopped_(false),
        error_(false) {
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

  void FeedCallback(media::InputBuffer* buffer) {
    // We receive this callback when the decoder has consumed an input buffer.
    // In this case, delete the previous buffer and enqueue a new one.
    // There are some conditions we don't want to enqueue, for example when
    // the last buffer is an end-of-stream buffer, when we have stopped, and
    // when we have received an error.
    bool eos = buffer->IsEndOfStream();
    delete buffer;
    if (!eos && !stopped_ && !error_)
      FeedDecoder();
  }

  void ReadCompleteCallback(uint8* buffer, int size) {
    // This callback is received when the decoder has completed a decoding
    // task and given us some output data. The buffer is owned by the decoder.
    if (stopped_ || error_)
      return;

    // If we are readding to the end, then stop.
    if (!size) {
      decoder_->Stop(NewCallback(this, &TestApp::StopCallback));
      return;
    }

    // Read one more from the decoder.
    decoder_->Read(NewCallback(this, &TestApp::ReadCompleteCallback));

    // Copy the output of the decoder to user memory.
    if (simulate_copy_ || output_file_) {  // output_file_ implies a copy.
      if (size > copy_buf_size_) {
        copy_buf_.reset(new uint8[size]);
        copy_buf_size_ = size;
      }
      memcpy(copy_buf_.get(), buffer, size);
      if (output_file_)
        fwrite(copy_buf_.get(), sizeof(uint8), size, output_file_);
    }

    // could OMX IL return patial sample for decoder?
    frame_count_++;
    bit_count_ += size << 3;
  }

  void FeedDecoder() {
    // This method feeds the decoder with 32KB of input data.
    const int kSize = 32768;
    uint8* data = new uint8[kSize];
    int read = fread(data, 1, kSize, input_file_);
    decoder_->Feed(new media::InputBuffer(data, read),
                   NewCallback(this, &TestApp::FeedCallback));
  }

  void Run() {
    // Open the input file.
    input_file_ = file_util::OpenFile(input_filename_, "rb");
    if (!input_file_) {
      printf("Error - can't open file %s\n", input_filename_);
      return;
    }

    // Open the dump file
    if (strlen(output_filename_)) {
      output_file_ = file_util::OpenFile(output_filename_, "wb");
      if (!input_file_) {
        fclose(input_file_);
        printf("Error - can't open dump file %s\n", output_filename_);
        return;
      }
    }

    // Setup the decoder with the message loop of the current thread. Also
    // setup component name, codec and callbacks.
    decoder_ = new media::OmxCodec(&message_loop_);
    decoder_->Setup(component_, input_format_, output_format_);
    decoder_->SetErrorCallback(NewCallback(this, &TestApp::ErrorCallback));

    // Start the decoder.
    decoder_->Start();
    for (int i = 0; i < 20; ++i)
      FeedDecoder();
    decoder_->Read(NewCallback(this, &TestApp::ReadCompleteCallback));

    // Execute the message loop so that we can run tasks on it. This call
    // will return when we call message_loop_.Quit().
    message_loop_.Run();

    fclose(input_file_);
    if (output_file_)
      fclose(output_file_);
  }

  void StartProfiler() {
    start_time_ = base::Time::Now();
    frame_count_ = 0;
    bit_count_ = 0;
  }

  void StopProfiler() {
    base::Time stop_time = base::Time::Now();
    base::TimeDelta duration = stop_time - start_time_;
    int64 micro_sec = duration.InMicroseconds();
    // TODO(hclam): Linux 64bit doesn't like the following lines.
    // printf("\n<<< frame delivered : %lld >>>", frame_count_);
    // printf("\n<<< time used(us) : %lld >>>", micro_sec);
    // printf("\n<<< fps : %lld >>>", frame_count_ * 1000000 / micro_sec);
    // printf("\n<<< bitrate>>> : %lld\n", bit_count_ * 1000000 / micro_sec);
    printf("\n");
  }

  scoped_refptr<media::OmxCodec> decoder_;
  MessageLoop message_loop_;
  const char* input_filename_;
  const char* output_filename_;
  const char* component_;
  media::OmxCodec::OmxMediaFormat input_format_;
  media::OmxCodec::OmxMediaFormat output_format_;
  bool simulate_copy_;
  scoped_array<uint8> copy_buf_;
  int copy_buf_size_;
  FILE *input_file_, *output_file_;
  bool stopped_;
  bool error_;
  base::Time start_time_;
  int64 frame_count_;
  int64 bit_count_;
};

// Not intend to be used in production
void NV21toI420(uint8* nv21, uint8* i420, int width, int height) {
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

void NV21toYV12(uint8* nv21, uint8* yv12, int width, int height) {
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

void I420toNV21(uint8* i420, uint8* nv21, int width, int height) {
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

void YV12toNV21(uint8* yv12, uint8* nv21, int width, int height) {
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

int main(int argc, char** argv) {
  base::AtExitManager at_exit_manager;

  CommandLine::Init(argc, argv);
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();

  if (argc < 2) {
    printf("Usage: omx_test --input-file=FILE"
           " --component=COMPONENT --codec=CODEC"
           " [--output-file=FILE] [--enable-csc=FILE]"
           " [--copy] [--measure-fps]\n");
    printf("    COMPONENT: OpenMAX component name\n");
    printf("    CODEC: h264/mpeg4/h263/vc1\n");
    printf("\n");
    printf("Optional Arguments\n");
    printf("    --output-file Dump raw OMX output to file.\n");
    printf("    --enable-csc  Dump the CSCed output to file.\n");
    printf("    --copy        Simulate a memcpy from the output of decoder.\n");
    printf("    --measure-fps Measuring performance in fps\n");
    return 1;
  }

  std::string input_filename = cmd_line->GetSwitchValueASCII("input-file");
  std::string output_filename = cmd_line->GetSwitchValueASCII("output-file");
  std::string component = cmd_line->GetSwitchValueASCII("component");
  std::string codec = cmd_line->GetSwitchValueASCII("codec");
  bool copy = cmd_line->HasSwitch("copy");
  bool measure_fps = cmd_line->HasSwitch("measure-fps");


  media::OmxCodec::OmxMediaFormat input, output;
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

  // Create a TestApp object and run the decoder.
  TestApp test(input_filename.c_str(),
               output_filename.c_str(),
               component.c_str(),
               input,
               output,
               copy);


  if (measure_fps)
    test.StartProfiler();

  // This call will run the decoder until EOS is reached or an error
  // is encountered.
  test.Run();

  if (measure_fps)
    test.StopProfiler();

  // Color space conversion
  if ( !output_filename.empty() ) {
    std::string dumpyuv_name = cmd_line->GetSwitchValueASCII("enable-csc");
    if (!dumpyuv_name.empty()) {
      // now assume the raw output is NV21;
      // now assume decoder.
      FILE* dump_raw = file_util::OpenFile(output_filename.c_str(), "rb");
      FILE* dump_yuv = file_util::OpenFile(dumpyuv_name.c_str(), "wb");
      if (!dump_raw || !dump_yuv) {
        printf("Error - can't open file for color conversion %s\n",
               dumpyuv_name.c_str());
      } else {
        // TODO(jiesun): get rid of hard coded value when Startup()
        // call back function is ready.
        int width = 352;
        int height = 288;
        int frame_size = width * height * 3 / 2;  // assume 4:2:0 chroma format
        uint8* in_buffer = new uint8[frame_size];
        uint8* out_buffer = new uint8[frame_size];
        while (true) {
          int read = fread(in_buffer, sizeof(uint8), frame_size, dump_raw);
          if (read != frame_size)
            break;
          NV21toI420(in_buffer, out_buffer, width, height);
          fwrite(out_buffer, sizeof(uint8), frame_size, dump_yuv);
        }
        delete [] in_buffer;
        delete [] out_buffer;
      }
      if (dump_raw)
        fclose(dump_raw);
      if (dump_yuv)
        fclose(dump_yuv);
    }
  }

  return 0;
}
