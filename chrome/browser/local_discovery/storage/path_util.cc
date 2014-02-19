// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/storage/path_util.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"

namespace local_discovery {

namespace {

std::string UnescapeSlashes(const std::string& str) {
  std::string output = "";
  for (size_t i = 0; i < str.length(); i++) {
    if (str[i] == '$') {
      i++;
      switch (str[i]) {
        case 's':
          output += '/';
          break;
        case 'b':
          output += '\\';
        break;
        case '$':
          output += '$';
          break;
        default:
          NOTREACHED();
      }
    } else {
      output += str[i];
    }
  }

  return output;
}

const size_t kNumComponentsInBasePrivetPath = 4;
const int kIndexOfServiceNameInComponentList = 2;

std::string PathStringToString(const base::FilePath::StringType& string) {
#if defined(OS_WIN)
  return base::UTF16ToUTF8(string);
#else
  return string;
#endif
}

}  // namespace

ParsedPrivetPath::ParsedPrivetPath(const base::FilePath& file_path) {
  std::vector<base::FilePath::StringType> components;
  file_path.GetComponents(&components);
  DCHECK(components.size() >= kNumComponentsInBasePrivetPath);
  service_name = UnescapeSlashes(PathStringToString(
      components[kIndexOfServiceNameInComponentList]));


  for (size_t i = kNumComponentsInBasePrivetPath; i < components.size(); i++) {
    path += '/' + PathStringToString(components[i]);
  }

  if (path.empty()) path = "/";
}

ParsedPrivetPath::~ParsedPrivetPath() {
}

}  // namespace local_discovery
