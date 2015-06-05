// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media.h"

#include <windows.h>
#if defined(_WIN32_WINNT_WIN8)
// The Windows 8 SDK defines FACILITY_VISUALCPP in winerror.h.
#undef FACILITY_VISUALCPP
#endif
#include <delayimp.h>

#include "base/files/file_path.h"
#include "base/metrics/sparse_histogram.h"
#include "media/ffmpeg/ffmpeg_common.h"

#pragma comment(lib, "delayimp.lib")

namespace media {
namespace internal {

bool InitializeMediaLibraryInternal(const base::FilePath& module_dir) {
  // LoadLibraryEx(..., LOAD_WITH_ALTERED_SEARCH_PATH) cannot handle
  // relative path.
  if (!module_dir.IsAbsolute())
    return false;

  // Use alternate DLL search path so we don't load dependencies from the
  // system path.  Refer to http://crbug.com/35857
  static const char kFFmpegDLL[] = "ffmpegsumo.dll";
  HMODULE lib = ::LoadLibraryEx(
      module_dir.AppendASCII(kFFmpegDLL).value().c_str(), NULL,
      LOAD_WITH_ALTERED_SEARCH_PATH);

  bool initialized = (lib != NULL);

  // TODO(scherkus): Remove all the bool-ness from these functions as we no
  // longer support disabling HTML5 media at runtime. http://crbug.com/440892
  if (!initialized) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("Media.Initialize.Windows", GetLastError());
    return false;
  }

  // VS2013 has a bug where FMA3 instructions will be executed on CPUs that
  // support them despite them being disabled at the OS level, causing illegal
  // instruction exceptions. Because Web Audio's FFT code *might* run before
  // HTML5 media code, call av_log_set_level() to force library initialziation.
  // See http://crbug.com/440892 for details.
  av_log_set_level(AV_LOG_QUIET);

  return initialized;
}

}  // namespace internal
}  // namespace media
