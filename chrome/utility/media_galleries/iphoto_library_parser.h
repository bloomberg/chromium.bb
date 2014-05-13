// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_MEDIA_GALLERIES_IPHOTO_LIBRARY_PARSER_H_
#define CHROME_UTILITY_MEDIA_GALLERIES_IPHOTO_LIBRARY_PARSER_H_

#include <string>

#include "chrome/common/media_galleries/iphoto_library.h"

namespace iphoto {

class IPhotoLibraryParser {
 public:
  IPhotoLibraryParser();
  ~IPhotoLibraryParser();

  // Returns true if at least one track was found. Malformed track entries
  // are silently ignored.
  bool Parse(const std::string& xml);

  const parser::Library& library() { return library_; }

 private:
  parser::Library library_;

  DISALLOW_COPY_AND_ASSIGN(IPhotoLibraryParser);
};

}  // namespace iphoto

#endif  // CHROME_UTILITY_MEDIA_GALLERIES_IPHOTO_LIBRARY_PARSER_H_
