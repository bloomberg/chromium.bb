// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/drag_download_util.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"

namespace drag_download_util {

bool ParseDownloadMetadata(const string16& metadata,
                           string16* mime_type,
                           FilePath* file_name,
                           GURL* url) {
  const char16 separator = L':';

  size_t mime_type_end_pos = metadata.find(separator);
  if (mime_type_end_pos == string16::npos)
    return false;

  size_t file_name_end_pos = metadata.find(separator, mime_type_end_pos + 1);
  if (file_name_end_pos == string16::npos)
    return false;

  GURL parsed_url = GURL(metadata.substr(file_name_end_pos + 1));
  if (!parsed_url.is_valid())
    return false;

  if (mime_type)
    *mime_type = metadata.substr(0, mime_type_end_pos);
  if (file_name) {
    string16 file_name_str = metadata.substr(
        mime_type_end_pos + 1, file_name_end_pos - mime_type_end_pos  - 1);
#if defined(OS_WIN)
    *file_name = FilePath(file_name_str);
#else
    *file_name = FilePath(UTF16ToUTF8(file_name_str));
#endif
  }
  if (url)
    *url = parsed_url;

  return true;
}

}  // namespace drag_download_util
