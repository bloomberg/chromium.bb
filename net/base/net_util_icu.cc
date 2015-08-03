// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include "base/i18n/time_formatting.h"
#include "base/json/string_escape.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/escape.h"

namespace net {

std::string GetDirectoryListingEntry(const base::string16& name,
                                     const std::string& raw_bytes,
                                     bool is_dir,
                                     int64_t size,
                                     base::Time modified) {
  std::string result;
  result.append("<script>addRow(");
  base::EscapeJSONString(name, true, &result);
  result.append(",");
  if (raw_bytes.empty()) {
    base::EscapeJSONString(EscapePath(base::UTF16ToUTF8(name)), true, &result);
  } else {
    base::EscapeJSONString(EscapePath(raw_bytes), true, &result);
  }

  if (is_dir) {
    result.append(",1,");
  } else {
    result.append(",0,");
  }

  // Negative size means unknown or not applicable (e.g. directory).
  base::string16 size_string;
  if (size >= 0)
    size_string = base::FormatBytesUnlocalized(size);
  base::EscapeJSONString(size_string, true, &result);

  result.append(",");

  base::string16 modified_str;
  // |modified| can be NULL in FTP listings.
  if (!modified.is_null())
    modified_str = base::TimeFormatShortDateAndTime(modified);
  base::EscapeJSONString(modified_str, true, &result);

  result.append(");</script>\n");

  return result;
}

}  // namespace net
