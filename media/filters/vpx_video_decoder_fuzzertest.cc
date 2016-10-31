// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <random>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media.h"
#include "media/base/media_util.h"
#include "media/filters/vpx_video_decoder.h"

using namespace media;

struct Env {
  Env() {
    InitializeMediaLibrary();
    base::CommandLine::Init(0, nullptr);
    logging::SetMinLogLevel(logging::LOG_FATAL);
  }

  base::AtExitManager at_exit_manager;
  base::MessageLoop message_loop;
};
Env* env = new Env();

void OnDecodeComplete(const base::Closure& quit_closure, DecodeStatus status) {
  quit_closure.Run();
}

void OnInitDone(const base::Closure& quit_closure,
                bool* success_dest,
                bool success) {
  *success_dest = success;
  quit_closure.Run();
}

void OnOutputComplete(const scoped_refptr<VideoFrame>& frame) {}

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::mt19937_64 rng;

  {  // Seed rng from data.
    std::string str = std::string(reinterpret_cast<const char*>(data), size);
    std::size_t data_hash = std::hash<std::string>()(str);
    rng.seed(data_hash);
  }

  // Compute randomized constants. Put all rng() usages here.
  auto codec = static_cast<VideoCodec>(rng() % kVideoCodecMax);
  auto profile =
      static_cast<VideoCodecProfile>(rng() % VIDEO_CODEC_PROFILE_MAX);
  auto pixel_format = static_cast<VideoPixelFormat>(rng() % PIXEL_FORMAT_MAX);
  auto color_space = static_cast<ColorSpace>(rng() % COLOR_SPACE_MAX);
  auto coded_size = gfx::Size(rng() % 128, rng() % 128);
  auto visible_rect = gfx::Rect(rng() % 128, rng() % 128);
  auto natural_size = gfx::Size(rng() % 128, rng() % 128);

  VideoDecoderConfig config(codec, profile, pixel_format, color_space,
                            coded_size, visible_rect, natural_size,
                            EmptyExtraData(), Unencrypted());

  VpxVideoDecoder decoder;

  {
    base::RunLoop run_loop;
    bool success = false;
    decoder.Initialize(
        config, true /* low_delay */, nullptr /* cdm_context */,
        base::Bind(&OnInitDone, run_loop.QuitClosure(), &success),
        base::Bind(&OnOutputComplete));
    run_loop.Run();
    if (!success)
      return 0;
  }

  {
    base::RunLoop run_loop;
    auto buffer = DecoderBuffer::CopyFrom(data, size);
    decoder.Decode(buffer,
                   base::Bind(&OnDecodeComplete, run_loop.QuitClosure()));
    run_loop.Run();
  }

  return 0;
}
