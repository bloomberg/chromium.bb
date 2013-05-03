// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/file_handlers/file_handlers_parser.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest.h"
#include "extensions/common/error_utils.h"

namespace keys = extension_manifest_keys;

namespace extensions {

FileHandlerInfo::FileHandlerInfo() {}
FileHandlerInfo::~FileHandlerInfo() {}

FileHandlers::FileHandlers() {}
FileHandlers::~FileHandlers() {}

// static
const std::vector<FileHandlerInfo>* FileHandlers::GetFileHandlers(
    const Extension* extension) {
  FileHandlers* info = static_cast<FileHandlers*>(
      extension->GetManifestData(extension_manifest_keys::kFileHandlers));
  return info ? &info->file_handlers : NULL;
}

FileHandlersParser::FileHandlersParser() {
}

FileHandlersParser::~FileHandlersParser() {
}

bool LoadFileHandler(const std::string& handler_id,
                     const DictionaryValue& handler_info,
                     std::vector<FileHandlerInfo>* file_handlers,
                     string16* error) {
  DCHECK(error);
  FileHandlerInfo handler;

  handler.id = handler_id;

  const ListValue* mime_types = NULL;
  if (handler_info.HasKey(keys::kFileHandlerTypes) &&
      !handler_info.GetList(keys::kFileHandlerTypes, &mime_types)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        extension_manifest_errors::kInvalidFileHandlerType, handler_id);
    return false;
  }

  const ListValue* file_extensions = NULL;
  if (handler_info.HasKey(keys::kFileHandlerExtensions) &&
      !handler_info.GetList(keys::kFileHandlerExtensions, &file_extensions)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        extension_manifest_errors::kInvalidFileHandlerExtension, handler_id);
    return false;
  }

  if ((!mime_types || mime_types->GetSize() == 0) &&
      (!file_extensions || file_extensions->GetSize() == 0)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        extension_manifest_errors::kInvalidFileHandlerNoTypeOrExtension,
        handler_id);
    return false;
  }

  if (handler_info.HasKey(keys::kFileHandlerTitle) &&
      !handler_info.GetString(keys::kFileHandlerTitle, &handler.title)) {
    *error = ASCIIToUTF16(extension_manifest_errors::kInvalidFileHandlerTitle);
    return false;
  }

  if (mime_types) {
    std::string type;
    for (size_t i = 0; i < mime_types->GetSize(); ++i) {
      if (!mime_types->GetString(i, &type)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            extension_manifest_errors::kInvalidFileHandlerTypeElement,
            handler_id,
            std::string(base::IntToString(i)));
        return false;
      }
      handler.types.insert(type);
    }
  }

  if (file_extensions) {
    std::string file_extension;
    for (size_t i = 0; i < file_extensions->GetSize(); ++i) {
      if (!file_extensions->GetString(i, &file_extension)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            extension_manifest_errors::kInvalidFileHandlerExtensionElement,
            handler_id,
            std::string(base::IntToString(i)));
        return false;
      }
      handler.extensions.insert(file_extension);
    }
  }

  file_handlers->push_back(handler);
  return true;
}

bool FileHandlersParser::Parse(Extension* extension, string16* error) {
  scoped_ptr<FileHandlers> info(new FileHandlers);
  const DictionaryValue* all_handlers = NULL;
  if (!extension->manifest()->GetDictionary(keys::kFileHandlers,
                                            &all_handlers)) {
    *error = ASCIIToUTF16(extension_manifest_errors::kInvalidFileHandlers);
    return false;
  }

  DCHECK(extension->is_platform_app());

  for (DictionaryValue::Iterator iter(*all_handlers); !iter.IsAtEnd();
       iter.Advance()) {
    // A file handler entry is a title and a list of MIME types to handle.
    const DictionaryValue* handler = NULL;
    if (iter.value().GetAsDictionary(&handler)) {
      if (!LoadFileHandler(iter.key(), *handler, &info->file_handlers, error))
        return false;
    } else {
      *error = ASCIIToUTF16(extension_manifest_errors::kInvalidFileHandlers);
      return false;
    }
  }

  extension->SetManifestData(keys::kFileHandlers, info.release());
  return true;
}

const std::vector<std::string> FileHandlersParser::Keys() const {
  return SingleKey(keys::kFileHandlers);
}

}  // namespace extensions
