// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/media_galleries_handler.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest.h"
#include "extensions/common/error_utils.h"

namespace keys = extension_manifest_keys;
namespace errors = extension_manifest_errors;

namespace {

// Stored on the Extension.
struct MediaGalleriesHandlerInfo : public extensions::Extension::ManifestData {
  MediaGalleriesHandler::List media_galleries_handlers;

  MediaGalleriesHandlerInfo();
  virtual ~MediaGalleriesHandlerInfo();
};

MediaGalleriesHandlerInfo::MediaGalleriesHandlerInfo() {
}

MediaGalleriesHandlerInfo::~MediaGalleriesHandlerInfo() {
}

MediaGalleriesHandler* LoadMediaGalleriesHandler(
    const std::string& extension_id,
    const DictionaryValue* media_galleries_handler,
    string16* error) {
  scoped_ptr<MediaGalleriesHandler> result(new MediaGalleriesHandler());
  result->set_extension_id(extension_id);

  std::string handler_id;
  // Read the file action |id| (mandatory).
  if (!media_galleries_handler->HasKey(keys::kPageActionId) ||
      !media_galleries_handler->GetString(keys::kPageActionId, &handler_id)) {
    *error = ASCIIToUTF16(errors::kInvalidPageActionId);
    return NULL;
  }
  result->set_id(handler_id);

  // Read the page action title from |default_title| (mandatory).
  std::string title;
  if (!media_galleries_handler->HasKey(keys::kPageActionDefaultTitle) ||
      !media_galleries_handler->GetString(keys::kPageActionDefaultTitle,
                                          &title)) {
    *error = ASCIIToUTF16(errors::kInvalidPageActionDefaultTitle);
    return NULL;
  }
  result->set_title(title);

  std::string default_icon;
  // Read the media galleries action |default_icon| (optional).
  if (media_galleries_handler->HasKey(keys::kPageActionDefaultIcon)) {
    if (!media_galleries_handler->GetString(
            keys::kPageActionDefaultIcon, &default_icon) ||
        default_icon.empty()) {
      *error = ASCIIToUTF16(errors::kInvalidPageActionIconPath);
      return NULL;
    }
    result->set_icon_path(default_icon);
  }

  return result.release();
}

// Loads MediaGalleriesHandlers from |extension_actions| into a list in
// |result|.
bool LoadMediaGalleriesHandlers(
    const std::string& extension_id,
    const ListValue* extension_actions,
    MediaGalleriesHandler::List* result,
    string16* error) {
  for (ListValue::const_iterator iter = extension_actions->begin();
       iter != extension_actions->end();
       ++iter) {
    if (!(*iter)->IsType(Value::TYPE_DICTIONARY)) {
      *error = ASCIIToUTF16(errors::kInvalidMediaGalleriesHandler);
      return false;
    }
    scoped_ptr<MediaGalleriesHandler> action(
        LoadMediaGalleriesHandler(
            extension_id, reinterpret_cast<DictionaryValue*>(*iter), error));
    if (!action.get())
      return false;  // Failed to parse media galleries action definition.
    result->push_back(linked_ptr<MediaGalleriesHandler>(action.release()));
  }
  return true;
}

}  // namespace

MediaGalleriesHandler::MediaGalleriesHandler() {
}

MediaGalleriesHandler::~MediaGalleriesHandler() {
}

// static
MediaGalleriesHandler::List*
MediaGalleriesHandler::GetHandlers(const extensions::Extension* extension) {
  MediaGalleriesHandlerInfo* info = static_cast<MediaGalleriesHandlerInfo*>(
      extension->GetManifestData(keys::kMediaGalleriesHandlers));
  if (info)
    return &info->media_galleries_handlers;
  return NULL;
}

MediaGalleriesHandlerParser::MediaGalleriesHandlerParser() {
}

MediaGalleriesHandlerParser::~MediaGalleriesHandlerParser() {
}

bool MediaGalleriesHandlerParser::Parse(extensions::Extension* extension,
                                        string16* error) {
  const ListValue* media_galleries_handlers_value = NULL;
  if (!extension->manifest()->GetList(keys::kMediaGalleriesHandlers,
                                      &media_galleries_handlers_value)) {
    *error = ASCIIToUTF16(errors::kInvalidMediaGalleriesHandler);
    return false;
  }
  scoped_ptr<MediaGalleriesHandlerInfo> info(new MediaGalleriesHandlerInfo);
  if (!LoadMediaGalleriesHandlers(extension->id(),
                                  media_galleries_handlers_value,
                                  &info->media_galleries_handlers,
                                  error)) {
    return false;  // Failed to parse media galleries actions definition.
  }

  extension->SetManifestData(keys::kMediaGalleriesHandlers, info.release());
  return true;
}

const std::vector<std::string> MediaGalleriesHandlerParser::Keys() const {
  return SingleKey(keys::kMediaGalleriesHandlers);
}
