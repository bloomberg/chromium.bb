// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_FILE_FORMATS_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_FILE_FORMATS_H_

#include "base/file_path.h"

namespace gdata {

// File formats used for Drive.FileFormat histogram.
enum DriveFileFormat {
  FILE_FORMAT_AAC,  // Advanced Audio Coding
  FILE_FORMAT_ASF,  // Advanced Systems Format (Microsoft audio/video)
  FILE_FORMAT_AVI,  // Audio Video Interleave
  FILE_FORMAT_CSV,  // Comma-separated values
  FILE_FORMAT_DOC,  // Microsoft Word
  FILE_FORMAT_DOCX,  // Microsoft Word
  FILE_FORMAT_FLV,  // Flash Video
  FILE_FORMAT_JPG,  // JPEG images
  FILE_FORMAT_MJPG,  // Motion JPEG
  FILE_FORMAT_MOV,  // Quick time
  FILE_FORMAT_MP3,  // MP3 audio
  FILE_FORMAT_MP4,  // MPEG-4 audio
  FILE_FORMAT_MPG,  // MPEG video
  FILE_FORMAT_OTHER,  // Other format
  FILE_FORMAT_PDF,  // Portable Document Format
  FILE_FORMAT_PPT,  // Microsoft Powerpoint
  FILE_FORMAT_PPTX,  // Microsoft Powerpoint
  FILE_FORMAT_PSD,  // Photoshop
  FILE_FORMAT_RAR,  // RAR archive
  FILE_FORMAT_WMA,  // Windows Media Audio
  FILE_FORMAT_WMV,  // Windows Media Video
  FILE_FORMAT_XLS,  // Microsoft Excel
  FILE_FORMAT_XLSX,  // Microsoft Excel
  FILE_FORMAT_ZIP,  // ZIP archive
  // New file formats should be added here. Don't reorder the existing ones.

  // This should be the last item.
  FILE_FORMAT_MAX_VALUE,
};

// Gets a DriveFileFormat from |extension| like ".jpg", which can be
// obtained with FilePath::Extension(). |extension| is case-insensitive.
// Returns |FILE_FORMAT_OTHER| if |extension| is unknown.
DriveFileFormat GetDriveFileFormat(const FilePath::StringType& extension);

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_FILE_FORMATS_H_
