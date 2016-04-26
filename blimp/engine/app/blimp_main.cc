// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "blimp/engine/app/blimp_content_main_delegate.h"
#include "content/public/app/content_main.h"

const char kEnableOverlayScrollbar[] = "--enable-overlay-scrollbar";
const char kDisableCachedPictureRaster[] = "--disable-cached-picture-raster";
const char kDisableGpu[] = "--disable-gpu";
const char kDisableRemoteFonts[] = "--disable-remote-fonts";
const char kUseRemoteCompositing[] = "--use-remote-compositing";

int main(int argc, const char** argv) {
  base::CommandLine::Init(argc, argv);

  // Set internal command line switches for Blimp engine.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kEnableOverlayScrollbar);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      kDisableCachedPictureRaster);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kDisableGpu);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kDisableRemoteFonts);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kUseRemoteCompositing);

  blimp::engine::BlimpContentMainDelegate delegate;
  content::ContentMainParams params(&delegate);
  params.argc = argc;
  params.argv = argv;
  return content::ContentMain(params);
}
