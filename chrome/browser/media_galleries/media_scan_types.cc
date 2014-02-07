// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_scan_types.h"

MediaGalleryScanResult::MediaGalleryScanResult()
    : audio_count(0),
      image_count(0),
      video_count(0) {
}

bool IsEmptyScanResult(const MediaGalleryScanResult& scan_result) {
  return (scan_result.audio_count == 0 &&
          scan_result.image_count == 0 &&
          scan_result.video_count == 0);
}
