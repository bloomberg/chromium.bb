// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_browser_handler.h"

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

const char kReadAccessString[] = "read";
const char kReadWriteAccessString[] = "read-write";
const char kCreateAccessString[] = "create";

unsigned int kPermissionsNotDefined = 0;
unsigned int kReadPermission = 1;
unsigned int kWritePermission = 1 << 1;
unsigned int kCreatePermission = 1 << 2;
unsigned int kInvalidPermission = 1 << 3;

unsigned int GetAccessPermissionFlagFromString(const std::string& access_str) {
  if (access_str == kReadAccessString)
    return kReadPermission;
  if (access_str == kReadWriteAccessString)
    return kReadPermission | kWritePermission;
  if (access_str == kCreateAccessString)
    return kCreatePermission;
  return kInvalidPermission;
}

const char* const kMIMETypeHandlersWhitelist[] = {
  extension_misc::kQuickOfficeExtensionId,
  extension_misc::kQuickOfficeDevExtensionId
};

// Stored on the Extension.
struct FileBrowserHandlerInfo : public extensions::Extension::ManifestData {
  FileBrowserHandler::List file_browser_handlers;

  FileBrowserHandlerInfo();
  virtual ~FileBrowserHandlerInfo();
};

FileBrowserHandlerInfo::FileBrowserHandlerInfo() {
}

FileBrowserHandlerInfo::~FileBrowserHandlerInfo() {
}

}  // namespace

// static
bool FileBrowserHandler::ExtensionWhitelistedForMIMETypes(
    const std::string& extension_id) {
  if (g_test_extension_id_ && extension_id == *g_test_extension_id_)
    return true;
  for (size_t i = 0; i < arraysize(kMIMETypeHandlersWhitelist); ++i) {
    if (extension_id == kMIMETypeHandlersWhitelist[i])
      return true;
  }
  return false;
}

// static
std::vector<std::string> FileBrowserHandler::GetMIMETypeWhitelist() {
  std::vector<std::string> whitelist;
  if (g_test_extension_id_)
    whitelist.push_back(*g_test_extension_id_);
  for (size_t i = 0; i < arraysize(kMIMETypeHandlersWhitelist); ++i)
    whitelist.push_back(kMIMETypeHandlersWhitelist[i]);
  return whitelist;
}

FileBrowserHandler::FileBrowserHandler()
    : file_access_permission_flags_(kPermissionsNotDefined) {
}

FileBrowserHandler::~FileBrowserHandler() {
}

void FileBrowserHandler::AddPattern(const URLPattern& pattern) {
  url_set_.AddPattern(pattern);
}

void FileBrowserHandler::ClearPatterns() {
  url_set_.ClearPatterns();
}

bool FileBrowserHandler::MatchesURL(const GURL& url) const {
  return url_set_.MatchesURL(url);
}

void FileBrowserHandler::AddMIMEType(const std::string& mime_type) {
  DCHECK(ExtensionWhitelistedForMIMETypes(extension_id()));
  mime_type_set_.insert(mime_type);
}

bool FileBrowserHandler::CanHandleMIMEType(const std::string& mime_type) const {
  return mime_type_set_.find(mime_type) != mime_type_set_.end();
}

bool FileBrowserHandler::AddFileAccessPermission(
    const std::string& access) {
  file_access_permission_flags_ |= GetAccessPermissionFlagFromString(access);
  return (file_access_permission_flags_ & kInvalidPermission) != 0U;
}

bool FileBrowserHandler::ValidateFileAccessPermissions() {
  bool is_invalid = (file_access_permission_flags_ & kInvalidPermission) != 0U;
  bool can_create = (file_access_permission_flags_ & kCreatePermission) != 0U;
  bool can_read_or_write = (file_access_permission_flags_ &
      (kReadPermission | kWritePermission)) != 0U;
  if (is_invalid || (can_create && can_read_or_write)) {
    file_access_permission_flags_ = kInvalidPermission;
    return false;
  }

  if (file_access_permission_flags_ == kPermissionsNotDefined)
    file_access_permission_flags_ = kReadPermission | kWritePermission;
  return true;
}

bool FileBrowserHandler::CanRead() const {
  DCHECK(!(file_access_permission_flags_ & kInvalidPermission));
  return (file_access_permission_flags_ & kReadPermission) != 0;
}

bool FileBrowserHandler::CanWrite() const {
  DCHECK(!(file_access_permission_flags_ & kInvalidPermission));
  return (file_access_permission_flags_ & kWritePermission) != 0;
}

bool FileBrowserHandler::HasCreateAccessPermission() const {
  DCHECK(!(file_access_permission_flags_ & kInvalidPermission));
  return (file_access_permission_flags_ & kCreatePermission) != 0;
}

// static
FileBrowserHandler::List*
FileBrowserHandler::GetHandlers(const extensions::Extension* extension) {
  FileBrowserHandlerInfo* info = static_cast<FileBrowserHandlerInfo*>(
      extension->GetManifestData(keys::kFileBrowserHandlers));
  if (info)
    return &info->file_browser_handlers;
  return NULL;
}

std::string* FileBrowserHandler::g_test_extension_id_ = NULL;

FileBrowserHandlerParser::FileBrowserHandlerParser() {
}

FileBrowserHandlerParser::~FileBrowserHandlerParser() {
}

namespace {

FileBrowserHandler* LoadFileBrowserHandler(
    const std::string& extension_id,
    const DictionaryValue* file_browser_handler,
    string16* error) {
  scoped_ptr<FileBrowserHandler> result(new FileBrowserHandler());
  result->set_extension_id(extension_id);

  std::string handler_id;
  // Read the file action |id| (mandatory).
  if (!file_browser_handler->HasKey(keys::kPageActionId) ||
      !file_browser_handler->GetString(keys::kPageActionId, &handler_id)) {
    *error = ASCIIToUTF16(errors::kInvalidPageActionId);
    return NULL;
  }
  result->set_id(handler_id);

  // Read the page action title from |default_title| (mandatory).
  std::string title;
  if (!file_browser_handler->HasKey(keys::kPageActionDefaultTitle) ||
      !file_browser_handler->GetString(keys::kPageActionDefaultTitle, &title)) {
    *error = ASCIIToUTF16(errors::kInvalidPageActionDefaultTitle);
    return NULL;
  }
  result->set_title(title);

  // Initialize access permissions (optional).
  const ListValue* access_list_value = NULL;
  if (file_browser_handler->HasKey(keys::kFileAccessList)) {
    if (!file_browser_handler->GetList(keys::kFileAccessList,
                                       &access_list_value) ||
        access_list_value->empty()) {
      *error = ASCIIToUTF16(errors::kInvalidFileAccessList);
      return NULL;
    }
    for (size_t i = 0; i < access_list_value->GetSize(); ++i) {
      std::string access;
      if (!access_list_value->GetString(i, &access) ||
          result->AddFileAccessPermission(access)) {
        *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidFileAccessValue, base::IntToString(i));
        return NULL;
      }
    }
  }
  if (!result->ValidateFileAccessPermissions()) {
    *error = ASCIIToUTF16(errors::kInvalidFileAccessList);
    return NULL;
  }

  // Initialize file filters (mandatory, unless "create" access is specified,
  // in which case is ignored).
  if (!result->HasCreateAccessPermission()) {
    const ListValue* file_filters = NULL;
    if (!file_browser_handler->HasKey(keys::kFileFilters) ||
        !file_browser_handler->GetList(keys::kFileFilters, &file_filters) ||
        file_filters->empty()) {
      *error = ASCIIToUTF16(errors::kInvalidFileFiltersList);
      return NULL;
    }
    for (size_t i = 0; i < file_filters->GetSize(); ++i) {
      std::string filter;
      if (!file_filters->GetString(i, &filter)) {
        *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidFileFilterValue, base::IntToString(i));
        return NULL;
      }
      StringToLowerASCII(&filter);
      if (!StartsWithASCII(filter,
                           std::string(chrome::kFileSystemScheme) + ':',
                           true)) {
        *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidURLPatternError, filter);
        return NULL;
      }
      // The user inputs filesystem:*; we don't actually implement scheme
      // wildcards in URLPattern, so transform to what will match correctly.
      filter.replace(0, 11, "chrome-extension://*/");
      URLPattern pattern(URLPattern::SCHEME_EXTENSION);
      if (pattern.Parse(filter) != URLPattern::PARSE_SUCCESS) {
        *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidURLPatternError, filter);
        return NULL;
      }
      std::string path = pattern.path();
      bool allowed = path == "/*" || path == "/*.*" ||
          (path.compare(0, 3, "/*.") == 0 &&
           path.find_first_of('*', 3) == std::string::npos);
      if (!allowed) {
        *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidURLPatternError, filter);
        return NULL;
      }
      result->AddPattern(pattern);
    }

    // Initialize MIME type filters (optional).
    // NOTE: This is only used by QuickOffice extension to register MIME types
    // it can handle by directly downloading them. It will *not* be used in File
    // Manager UI. This is why file filters are mandatory even when MIME type
    // filters are specified.
    const ListValue* mime_type_filters = NULL;
    if (file_browser_handler->HasKey(keys::kMIMETypes)) {
      if (!FileBrowserHandler::ExtensionWhitelistedForMIMETypes(extension_id)) {
        *error = ASCIIToUTF16(
            errors::kNoPermissionForFileBrowserHandlerMIMETypes);
        return NULL;
      }

      if (!file_browser_handler->GetList(keys::kMIMETypes,
                                         &mime_type_filters)) {
        *error = ASCIIToUTF16(errors::kInvalidFileBrowserHandlerMIMETypes);
        return NULL;
      }

      for (size_t i = 0; i < mime_type_filters->GetSize(); ++i) {
        std::string filter;
        if (!mime_type_filters->GetString(i, &filter)) {
          *error = ASCIIToUTF16(errors::kInvalidFileBrowserHandlerMIMETypes);
          return NULL;
        }
        result->AddMIMEType(filter);
      }
    }
  }

  std::string default_icon;
  // Read the file browser action |default_icon| (optional).
  if (file_browser_handler->HasKey(keys::kPageActionDefaultIcon)) {
    if (!file_browser_handler->GetString(
            keys::kPageActionDefaultIcon, &default_icon) ||
        default_icon.empty()) {
      *error = ASCIIToUTF16(errors::kInvalidPageActionIconPath);
      return NULL;
    }
    result->set_icon_path(default_icon);
  }

  return result.release();
}

// Loads FileBrowserHandlers from |extension_actions| into a list in |result|.
bool LoadFileBrowserHandlers(
    const std::string& extension_id,
    const ListValue* extension_actions,
    FileBrowserHandler::List* result,
    string16* error) {
  for (ListValue::const_iterator iter = extension_actions->begin();
       iter != extension_actions->end();
       ++iter) {
    if (!(*iter)->IsType(Value::TYPE_DICTIONARY)) {
      *error = ASCIIToUTF16(errors::kInvalidFileBrowserHandler);
      return false;
    }
    scoped_ptr<FileBrowserHandler> action(
        LoadFileBrowserHandler(
            extension_id, reinterpret_cast<DictionaryValue*>(*iter), error));
    if (!action.get())
      return false;  // Failed to parse file browser action definition.
    result->push_back(linked_ptr<FileBrowserHandler>(action.release()));
  }
  return true;
}

}  // namespace

bool FileBrowserHandlerParser::Parse(extensions::Extension* extension,
                                     string16* error) {
  const ListValue* file_browser_handlers_value = NULL;
  if (!extension->manifest()->GetList(keys::kFileBrowserHandlers,
                                      &file_browser_handlers_value)) {
    *error = ASCIIToUTF16(errors::kInvalidFileBrowserHandler);
    return false;
  }
  scoped_ptr<FileBrowserHandlerInfo> info(new FileBrowserHandlerInfo);
  if (!LoadFileBrowserHandlers(extension->id(),
                               file_browser_handlers_value,
                               &info->file_browser_handlers,
                               error)) {
    return false;  // Failed to parse file browser actions definition.
  }

  extension->SetManifestData(keys::kFileBrowserHandlers, info.release());
  return true;
}

const std::vector<std::string> FileBrowserHandlerParser::Keys() const {
  return SingleKey(keys::kFileBrowserHandlers);
}
