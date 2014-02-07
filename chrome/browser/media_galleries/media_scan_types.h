// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_SCAN_TYPES_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_SCAN_TYPES_H_

struct MediaGalleryScanResult {
  MediaGalleryScanResult();
  int audio_count;
  int image_count;
  int video_count;
};

enum MediaGalleryScanFileType {
  MEDIA_GALLERY_SCAN_FILE_TYPE_UNKNOWN = 0,
  MEDIA_GALLERY_SCAN_FILE_TYPE_AUDIO = 1 << 0,
  MEDIA_GALLERY_SCAN_FILE_TYPE_IMAGE = 1 << 1,
  MEDIA_GALLERY_SCAN_FILE_TYPE_VIDEO = 1 << 2,
};

bool IsEmptyScanResult(const MediaGalleryScanResult& scan_result);

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_SCAN_TYPES_H_
