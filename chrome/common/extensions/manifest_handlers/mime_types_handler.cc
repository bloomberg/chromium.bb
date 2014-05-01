// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/mime_types_handler.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace keys = extensions::manifest_keys;
namespace errors = extensions::manifest_errors;

namespace {

const char* const kMIMETypeHandlersWhitelist[] = {
    extension_misc::kPdfExtensionId,
    extension_misc::kQuickOfficeComponentExtensionId,
    extension_misc::kQuickOfficeInternalExtensionId,
    extension_misc::kQuickOfficeExtensionId,
    extension_misc::kStreamsPrivateTestExtensionId};

// Stored on the Extension.
struct MimeTypesHandlerInfo : public extensions::Extension::ManifestData {
  MimeTypesHandler handler_;

  MimeTypesHandlerInfo();
  virtual ~MimeTypesHandlerInfo();
};

MimeTypesHandlerInfo::MimeTypesHandlerInfo() {
}

MimeTypesHandlerInfo::~MimeTypesHandlerInfo() {
}

}  // namespace

// static
std::vector<std::string> MimeTypesHandler::GetMIMETypeWhitelist() {
  std::vector<std::string> whitelist;
  for (size_t i = 0; i < arraysize(kMIMETypeHandlersWhitelist); ++i)
    whitelist.push_back(kMIMETypeHandlersWhitelist[i]);
  return whitelist;
}

MimeTypesHandler::MimeTypesHandler() {
}

MimeTypesHandler::~MimeTypesHandler() {
}

void MimeTypesHandler::AddMIMEType(const std::string& mime_type) {
  mime_type_set_.insert(mime_type);
}

bool MimeTypesHandler::CanHandleMIMEType(const std::string& mime_type) const {
  return mime_type_set_.find(mime_type) != mime_type_set_.end();
}

// static
MimeTypesHandler* MimeTypesHandler::GetHandler(
    const extensions::Extension* extension) {
  MimeTypesHandlerInfo* info = static_cast<MimeTypesHandlerInfo*>(
      extension->GetManifestData(keys::kMimeTypesHandler));
  if (info)
    return &info->handler_;
  return NULL;
}

MimeTypesHandlerParser::MimeTypesHandlerParser() {
}

MimeTypesHandlerParser::~MimeTypesHandlerParser() {
}

bool MimeTypesHandlerParser::Parse(extensions::Extension* extension,
                                   base::string16* error) {
  const base::ListValue* mime_types_value = NULL;
  if (!extension->manifest()->GetList(keys::kMIMETypes,
                                      &mime_types_value)) {
    *error = base::ASCIIToUTF16(errors::kInvalidMimeTypesHandler);
    return false;
  }

  scoped_ptr<MimeTypesHandlerInfo> info(new MimeTypesHandlerInfo);
  info->handler_.set_extension_id(extension->id());
  for (size_t i = 0; i < mime_types_value->GetSize(); ++i) {
    std::string filter;
    if (!mime_types_value->GetString(i, &filter)) {
      *error = base::ASCIIToUTF16(errors::kInvalidMIMETypes);
      return false;
    }
    info->handler_.AddMIMEType(filter);
  }

  std::string mime_types_handler;
  if (extension->manifest()->GetString(keys::kMimeTypesHandler,
                                       &mime_types_handler)) {
    info->handler_.set_handler_url(mime_types_handler);
  }

  extension->SetManifestData(keys::kMimeTypesHandler, info.release());
  return true;
}

const std::vector<std::string> MimeTypesHandlerParser::Keys() const {
  std::vector<std::string> keys;
  keys.push_back(keys::kMIMETypes);
  keys.push_back(keys::kMimeTypesHandler);
  return keys;
}
