// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/file_browser_handler.h"

#include "base/logging.h"
#include "chrome/common/extensions/url_pattern.h"
#include "googleurl/src/gurl.h"

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
