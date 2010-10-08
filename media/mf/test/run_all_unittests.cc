// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/test/test_suite.h"
#include "media/base/media.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/file_protocol.h"

static bool InitFFmpeg() {
  if (!media::InitializeMediaLibrary(FilePath()))
    return false;
  avcodec_init();
  av_register_all();
  av_register_protocol2(&kFFmpegFileProtocol, sizeof(kFFmpegFileProtocol));
  return true;
}

int main(int argc, char** argv) {
  if (!InitFFmpeg()) {
    fprintf(stderr, "Failed to init ffmpeg\n");
    return -1;
  }
  return TestSuite(argc, argv).Run();
}
