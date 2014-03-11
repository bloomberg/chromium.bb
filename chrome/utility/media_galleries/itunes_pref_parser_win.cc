// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/media_galleries/itunes_pref_parser_win.h"

#include "base/base64.h"
#include "base/strings/string_util.h"
#include "chrome/utility/media_galleries/iapps_xml_utils.h"
#include "third_party/libxml/chromium/libxml_utils.h"

namespace itunes {

base::FilePath::StringType FindLibraryLocationInPrefXml(
    const std::string& pref_xml_data) {
  XmlReader reader;
  base::FilePath::StringType result;

  if (!reader.Load(pref_xml_data))
    return result;

  // Find the plist node and then search within that tag.
  if (!iapps::SeekToNodeAtCurrentDepth(&reader, "plist"))
    return result;
  if (!reader.Read())
    return result;

  if (!iapps::SeekToNodeAtCurrentDepth(&reader, "dict"))
    return result;

  if (!iapps::SeekInDict(&reader, "User Preferences"))
    return result;

  if (!iapps::SeekToNodeAtCurrentDepth(&reader, "dict"))
    return result;

  if (!iapps::SeekInDict(&reader, "iTunes Library XML Location:1"))
    return result;

  if (!iapps::SeekToNodeAtCurrentDepth(&reader, "data"))
    return result;

  std::string pref_value;
  if (!reader.ReadElementContent(&pref_value))
    return result;
  // The data is a base64 encoded wchar_t*. Because Base64Decode uses
  // std::strings, the result has to be casted to a wchar_t*.
  std::string data;
  if (!base::Base64Decode(base::CollapseWhitespaceASCII(pref_value, true),
                          &data))
    return result;
  return base::FilePath::StringType(
      reinterpret_cast<const wchar_t*>(data.data()), data.size() / 2);
}

}  // namespace itunes
