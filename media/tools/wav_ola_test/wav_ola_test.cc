// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This application is a test for AudioRendererAlgorithmOLA. It reads in a
// specified wav file (so far only 8, 16 and 32 bit are supported) and uses
// ARAO to scale the playback by a specified rate. Then it outputs the result
// to the specified location. Command line calls should be as follows:
//
//    wav_ola_test RATE INFILE OUTFILE

#include <iostream>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "media/base/data_buffer.h"
#include "media/filters/audio_renderer_algorithm_ola.h"

using file_util::ScopedFILE;
using media::AudioRendererAlgorithmOLA;
using media::DataBuffer;

const double kDefaultWindowLength = 0.08;

struct WavHeader {
  int32 riff;
  int32 chunk_size;
  char unused0[8];
  int32 subchunk1_size;
  int16 audio_format;
  int16 channels;
  int32 sample_rate;
  char unused1[6];
  int16 bit_rate;
  char unused2[4];
  int32 subchunk2_size;
};

// Dummy class to feed data to OLA algorithm. Necessary to create callback.
class Dummy {
 public:
  Dummy(FILE* in, AudioRendererAlgorithmOLA* ola, size_t window_size)
      : input_(in),
        ola_(ola),
        window_size_(window_size) {
  }

  void ReadDataForAlg() {
    scoped_refptr<DataBuffer> buffer(new DataBuffer(window_size_));
    buffer->SetDataSize(window_size_);
    uint8* buf = buffer->GetWritableData();
    if (fread(buf, 1, window_size_, input_) > 0) {
      ola_->EnqueueBuffer(buffer.get());
    }
  }

 private:
  FILE* input_;
  AudioRendererAlgorithmOLA* ola_;
  size_t window_size_;

  DISALLOW_COPY_AND_ASSIGN(Dummy);
};

int main(int argc, const char** argv) {
  AudioRendererAlgorithmOLA ola;
  CommandLine::Init(argc, argv);
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();

  const CommandLine::StringVector& filenames = cmd_line->GetArgs();
  if (filenames.empty()) {
    std::cerr << "Usage: " << argv[0] << " RATE INFILE OUTFILE\n"
              << std::endl;
    return 1;
  }

  // Retrieve command line options.
  FilePath in_path(filenames[1]);
  FilePath out_path(filenames[2]);
  double playback_rate = 0.0;

  // Determine speed of rerecord.
#if defined(OS_WIN)
  std::string filename_str = WideToASCII(filenames[0]);
#else
  const std::string& filename_str = filenames[0];
#endif
  if (!base::StringToDouble(filename_str, &playback_rate))
    playback_rate = 0.0;

  // Open input file.
  ScopedFILE input(file_util::OpenFile(in_path, "rb"));
  if (!(input.get())) {
    LOG(ERROR) << "could not open input";
    return 1;
  }

  // Open output file.
  ScopedFILE output(file_util::OpenFile(out_path, "wb"));
  if (!(output.get())) {
    LOG(ERROR) << "could not open output";
    return 1;
  }

  // Read in header.
  WavHeader wav;
  if (fread(&wav, sizeof(wav), 1, input.get()) < 1) {
    LOG(ERROR) << "could not read WAV header";
    return 1;
  }

  size_t window_size = static_cast<size_t>(wav.sample_rate
                                           * (wav.bit_rate / 8)
                                           * wav.channels
                                           * kDefaultWindowLength);

  // Instantiate dummy class and callback to feed data to |ola|.
  Dummy guy(input.get(), &ola, window_size);
  base::Closure cb = base::Bind(&Dummy::ReadDataForAlg, base::Unretained(&guy));
  ola.Initialize(wav.channels,
                 wav.sample_rate,
                 wav.bit_rate,
                 static_cast<float>(playback_rate),
                 cb);
  ola.FlushBuffers();

  // Print out input format.
  std::cout << in_path.value() << "\n"
            << "Channels: " << wav.channels << "\n"
            << "Sample Rate: " << wav.sample_rate << "\n"
            << "Bit Rate: " << wav.bit_rate << "\n"
            << "\n"
            << "Scaling audio by " << playback_rate << "x..." << std::endl;

  // Write the header back out again.
  if (fwrite(&wav, sizeof(wav), 1, output.get()) < 1) {
    LOG(ERROR) << "could not write WAV header";
    return 1;
  }

  // Create buffer to be filled by |ola|.
  scoped_array<uint8> buf(new uint8[window_size]);

  CHECK(buf.get());

  // Keep track of bytes written to disk and bytes copied to |b|.
  size_t bytes_written = 0;
  size_t bytes;
  while ((bytes = ola.FillBuffer(buf.get(), window_size)) > 0) {
    if (fwrite(buf.get(), 1, bytes, output.get()) != bytes) {
      LOG(ERROR) << "could not write data after " << bytes_written;
    } else {
      bytes_written += bytes;
    }
  }

  // Seek back to the beginning of our output file and update the header.
  wav.chunk_size = 36 + bytes_written;
  wav.subchunk1_size = 16;
  wav.subchunk2_size = bytes_written;
  fseek(output.get(), 0, SEEK_SET);
  if (fwrite(&wav, sizeof(wav), 1, output.get()) < 1) {
    LOG(ERROR) << "could not write wav header.";
    return 1;
  }
  CommandLine::Reset();
  return 0;
}
