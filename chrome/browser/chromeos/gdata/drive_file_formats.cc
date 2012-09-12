// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/drive_file_formats.h"

#include "base/basictypes.h"

namespace gdata {

namespace {

// Used to convert an extension to DriveFileFormat.
struct DriveFileFormatMap {
  DriveFileFormat file_format;
  const FilePath::CharType* extension;
};

const DriveFileFormatMap kDriveFileFormatMap[] = {
  { FILE_FORMAT_AAC, FILE_PATH_LITERAL(".AAC") },
  { FILE_FORMAT_ASF, FILE_PATH_LITERAL(".ASF") },
  { FILE_FORMAT_AVI, FILE_PATH_LITERAL(".AVI") },
  { FILE_FORMAT_CSV, FILE_PATH_LITERAL(".CSV") },
  { FILE_FORMAT_DOC, FILE_PATH_LITERAL(".DOC") },
  { FILE_FORMAT_DOCX, FILE_PATH_LITERAL(".DOCX") },
  { FILE_FORMAT_FLV, FILE_PATH_LITERAL(".FLV") },
  { FILE_FORMAT_JPG, FILE_PATH_LITERAL(".JPG") },
  { FILE_FORMAT_MJPG, FILE_PATH_LITERAL(".MJPG") },
  { FILE_FORMAT_MOV, FILE_PATH_LITERAL(".MOV") },
  { FILE_FORMAT_MP3, FILE_PATH_LITERAL(".MP3") },
  { FILE_FORMAT_MP4, FILE_PATH_LITERAL(".MP4") },
  { FILE_FORMAT_MPG, FILE_PATH_LITERAL(".MPG") },
  { FILE_FORMAT_PDF, FILE_PATH_LITERAL(".PDF") },
  { FILE_FORMAT_PPT, FILE_PATH_LITERAL(".PPT") },
  { FILE_FORMAT_PPTX, FILE_PATH_LITERAL(".PPTX") },
  { FILE_FORMAT_PSD, FILE_PATH_LITERAL(".PSD") },
  { FILE_FORMAT_RAR, FILE_PATH_LITERAL(".RAR") },
  { FILE_FORMAT_WMA, FILE_PATH_LITERAL(".WMA") },
  { FILE_FORMAT_WMV, FILE_PATH_LITERAL(".WMV") },
  { FILE_FORMAT_XLS, FILE_PATH_LITERAL(".XLS") },
  { FILE_FORMAT_XLSX, FILE_PATH_LITERAL(".XLSX") },
  { FILE_FORMAT_ZIP, FILE_PATH_LITERAL(".ZIP") },
};

// Subtract by 1, as FILE_FORMAT_OTHER is not included in kDriveFileFormatMap.
COMPILE_ASSERT(arraysize(kDriveFileFormatMap) == FILE_FORMAT_MAX_VALUE - 1,
               kDriveFileFormatMap_DriveFileFormat_are_not_in_sync);

}  // namespace

DriveFileFormat GetDriveFileFormat(const FilePath::StringType& extension) {
  for (size_t i = 0; i < arraysize(kDriveFileFormatMap); ++i) {
    if (FilePath::CompareEqualIgnoreCase(extension,
                                         kDriveFileFormatMap[i].extension)) {
      return kDriveFileFormatMap[i].file_format;
    }
  }
  return FILE_FORMAT_OTHER;
}

}  // namespace gdata
