// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_FILE_HANDLERS_FILE_HANDLERS_PARSER_H_
#define CHROME_COMMON_EXTENSIONS_API_FILE_HANDLERS_FILE_HANDLERS_PARSER_H_

#include <set>
#include <string>
#include <vector>

#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handler.h"

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

struct FileHandlers : public Extension::ManifestData {
  FileHandlers();
  virtual ~FileHandlers();

  std::vector<FileHandlerInfo> file_handlers;

  static const std::vector<FileHandlerInfo>* GetFileHandlers(
      const Extension* extension);
};

// Parses the "file_handlers" manifest key.
class FileHandlersParser : public ManifestHandler {
 public:
  FileHandlersParser();
  virtual ~FileHandlersParser();

  virtual bool Parse(Extension* extension, string16* error) OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(FileHandlersParser);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_FILE_HANDLERS_FILE_HANDLERS_PARSER_H_
