// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_BROWSER_FILE_HANDLER_UTIL_H_
#define APPS_BROWSER_FILE_HANDLER_UTIL_H_

#include <string>

namespace apps {
namespace file_handler_util {

// Refers to a file entry that a renderer has been given access to.
struct GrantedFileEntry {
  GrantedFileEntry();
  ~GrantedFileEntry();

  std::string id;
  std::string filesystem_id;
  std::string registered_name;
};

}  // namespace file_handler_util
}  // namespace apps

#endif  // APPS_BROWSER_FILE_HANDLER_UTIL_H_
