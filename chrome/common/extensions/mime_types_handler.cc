// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/mime_types_handler.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/url_pattern.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_util.h"

namespace keys = extension_manifest_keys;
namespace errors = extension_manifest_errors;

namespace {

const char* const kMIMETypeHandlersWhitelist[] = {
  extension_misc::kQuickOfficeComponentExtensionId,
  extension_misc::kQuickOfficeDevExtensionId,
  extension_misc::kQuickOfficeExtensionId,
  extension_misc::kStreamsPrivateTestExtensionId
};

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
                                   string16* error) {
  const ListValue* mime_types_value = NULL;
  if (!extension->manifest()->GetList(keys::kMIMETypes,
                                      &mime_types_value)) {
    *error = ASCIIToUTF16(errors::kInvalidMimeTypesHandler);
    return false;
  }

  scoped_ptr<MimeTypesHandlerInfo> info(new MimeTypesHandlerInfo);
  info->handler_.set_extension_id(extension->id());
  for (size_t i = 0; i < mime_types_value->GetSize(); ++i) {
    std::string filter;
    if (!mime_types_value->GetString(i, &filter)) {
      *error = ASCIIToUTF16(errors::kInvalidMIMETypes);
      return false;
    }
    info->handler_.AddMIMEType(filter);
  }

  extension->SetManifestData(keys::kMimeTypesHandler, info.release());
  return true;
}

const std::vector<std::string> MimeTypesHandlerParser::Keys() const {
  return SingleKey(keys::kMIMETypes);
}
