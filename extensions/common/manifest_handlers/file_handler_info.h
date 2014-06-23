// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_FILE_HANDLER_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_FILE_HANDLER_INFO_H_

#include <set>
#include <string>
#include <vector>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

struct FileHandlerInfo {
  FileHandlerInfo();
  ~FileHandlerInfo();

  // The id of this handler.
  std::string id;

  // File extensions associated with this handler.
  std::set<std::string> extensions;

  // MIME types associated with this handler.
  std::set<std::string> types;
};

typedef std::vector<FileHandlerInfo> FileHandlersInfo;

struct FileHandlers : public Extension::ManifestData {
  FileHandlers();
  virtual ~FileHandlers();

  FileHandlersInfo file_handlers;

  static const FileHandlersInfo* GetFileHandlers(const Extension* extension);
};

// Parses the "file_handlers" manifest key.
class FileHandlersParser : public ManifestHandler {
 public:
  FileHandlersParser();
  virtual ~FileHandlersParser();

  virtual bool Parse(Extension* extension, base::string16* error) OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(FileHandlersParser);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_FILE_HANDLER_INFO_H_
