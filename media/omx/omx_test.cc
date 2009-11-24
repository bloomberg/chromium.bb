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
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "media/omx/input_buffer.h"
#include "media/omx/omx_video_decoder.h"

// This is the driver object to feed the decoder with data from a file.
// It also provides callbacks for the decoder to receive events from the
// decoder.
class TestApp {
 public:
  TestApp(const char* filename,
          const char* component,
          media::OmxVideoDecoder::Codec codec)
      : filename_(filename),
        component_(component),
        codec_(codec),
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
    bool eos = buffer->Eos();
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
  }

  void FeedDecoder() {
    // This method feeds the decoder with 32KB of input data.
    const int kSize = 32768;
    uint8* data = new uint8[kSize];
    int read = fread(data, 1, kSize, file_);
    decoder_->Feed(new media::InputBuffer(data, read),
                   NewCallback(this, &TestApp::FeedCallback));
  }

  void Run() {
    // Open the input file.
    file_ = fopen(filename_, "rb");
    if (!file_) {
      printf("Error - can't open file %s\n", filename_);
      return;
    }

    // Setup the decoder with the message loop of the current thread. Also
    // setup component name, codec and callbacks.
    decoder_ = new media::OmxVideoDecoder(&message_loop_);
    decoder_->Setup(component_, codec_);
    decoder_->SetErrorCallback(NewCallback(this, &TestApp::ErrorCallback));

    // Start the decoder.
    decoder_->Start();
    for (int i = 0; i < 20; ++i)
      FeedDecoder();
    decoder_->Read(NewCallback(this, &TestApp::ReadCompleteCallback));

    // Execute the message loop so that we can run tasks on it. This call
    // will return when we call message_loop_.Quit().
    message_loop_.Run();
  }

  scoped_refptr<media::OmxVideoDecoder> decoder_;
  MessageLoop message_loop_;
  const char* filename_;
  const char* component_;
  media::OmxVideoDecoder::Codec codec_;
  FILE* file_;
  bool stopped_;
  bool error_;
};

int main(int argc, char** argv) {
  base::AtExitManager at_exit_manager;

  CommandLine::Init(argc, argv);
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();

  if (argc < 2) {
    printf("Usage: omx_test --file=FILE --component=COMPONENT --codec=CODEC\n");
    printf("    COMPONENT: OpenMAX component name\n");
    printf("    CODEC: h264/mpeg4/h263/vc1\n");
    return 1;
  }

  std::string filename = cmd_line->GetSwitchValueASCII("file");
  std::string component = cmd_line->GetSwitchValueASCII("component");
  std::string codec = cmd_line->GetSwitchValueASCII("codec");

  media::OmxVideoDecoder::Codec codec_id = media::OmxVideoDecoder::kCodecNone;
  if (codec == "h264")
    codec_id = media::OmxVideoDecoder::kCodecH264;
  else if (codec == "mpeg4")
    codec_id = media::OmxVideoDecoder::kCodecMpeg4;
  else if (codec == "h263")
    codec_id = media::OmxVideoDecoder::kCodecH263;
  else if (codec == "vc1")
    codec_id = media::OmxVideoDecoder::kCodecVc1;
  else {
    printf("Unknown codec.\n");
    return 1;
  }

  // Create a TestApp object and run the decoder.
  TestApp test(filename.c_str(), component.c_str(), codec_id);

  // This call will run the decoder until EOS is reached or an error
  // is encountered.
  test.Run();
  return 0;
}
