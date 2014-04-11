// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_FILE_HANDLER_INFO_H_
#define CHROME_COMMON_EXTENSIONS_FILE_HANDLER_INFO_H_

#include <set>
#include <string>
#include <vector>

namespace extensions {

struct FileHandlerInfo {
  FileHandlerInfo();
  ~FileHandlerInfo();

  std::string id;
  std::string title;

  // File extensions associated with this handler.
  std::set<std::string> extensions;

  // MIME types associated with this handler.
  std::set<std::string> types;
};

struct FileHandlersInfo {
  explicit FileHandlersInfo(const std::vector<FileHandlerInfo>* handlers);
  ~FileHandlersInfo();

  const std::vector<FileHandlerInfo>* handlers;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_FILE_HANDLER_INFO_H_
