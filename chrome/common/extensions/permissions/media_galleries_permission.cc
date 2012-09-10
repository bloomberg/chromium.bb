// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/media_galleries_permission.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/extensions/permissions/permission_message.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kAllAutoDetectedString[] = "all-auto-detected";
const char kReadString[] = "read";
const char kInvalidString[] = "invalid";

}  // namespace

namespace extensions {

MediaGalleriesPermission::MediaGalleriesPermission(
    const APIPermissionInfo* info)
    : APIPermission(info) {
}

MediaGalleriesPermission::~MediaGalleriesPermission() {
}

// static
bool MediaGalleriesPermission::HasReadAccess(const Extension& extension) {
  CheckParam read_param(kRead);
  return extension.CheckAPIPermissionWithParam(
      extensions::APIPermission::kMediaGalleries, &read_param);
}

// static
bool MediaGalleriesPermission::HasAllGalleriesAccess(
    const Extension& extension) {
  CheckParam all_param(kAllAutoDetected);
  return extension.CheckAPIPermissionWithParam(
      extensions::APIPermission::kMediaGalleries, &all_param);
}

// static
MediaGalleriesPermission::PermissionTypes
MediaGalleriesPermission::PermissionStringToType(const std::string& str) {
  if (str == kAllAutoDetectedString)
    return kAllAutoDetected;
  if (str == kReadString)
    return kRead;
  return kNone;
}

// static
const char* MediaGalleriesPermission::PermissionTypeToString(
    PermissionTypes type) {
  switch (type) {
    case kAllAutoDetected:
      return kAllAutoDetectedString;
    case kRead:
      return kReadString;
    default:
      NOTREACHED();
      return kInvalidString;
  }
}

bool MediaGalleriesPermission::HasMessages() const {
  CheckParam param(kAllAutoDetected);
  return Check(&param);
}

PermissionMessages MediaGalleriesPermission::GetMessages() const {
  CheckParam param(kAllAutoDetected);
  if (Check(&param)) {
    PermissionMessages result;
    result.push_back(PermissionMessage(
        PermissionMessage::kMediaGalleriesAllGalleries,
        l10n_util::GetStringUTF16(
            IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_ALL_GALLERIES)));
    return result;
  }
  NOTREACHED();
  PermissionMessages result;
  result.push_back(PermissionMessage(PermissionMessage::kUnknown, string16()));
  return result;
}

bool MediaGalleriesPermission::Check(
    const APIPermission::CheckParam* param) const {
  const CheckParam* media_galleries_param =
      static_cast<const CheckParam*>(param);
  return ContainsKey(permissions_, media_galleries_param->permission);
}

bool MediaGalleriesPermission::Contains(const APIPermission* rhs) const {
  CHECK_EQ(rhs->info(), info());
  const MediaGalleriesPermission* perm =
      static_cast<const MediaGalleriesPermission*>(rhs);
  return std::includes(permissions_.begin(), permissions_.end(),
                       perm->permissions_.begin(), perm->permissions_.end());
}

bool MediaGalleriesPermission::Equal(const APIPermission* rhs) const {
  CHECK(rhs->info() == info());
  const MediaGalleriesPermission* perm =
      static_cast<const MediaGalleriesPermission*>(rhs);
  return permissions_ == perm->permissions_;
}

bool MediaGalleriesPermission::FromValue(const base::Value* value) {
  permissions_.clear();

  if (!value)
    return false;

  const base::ListValue* list = NULL;
  if (!value->GetAsList(&list) && list->GetSize() == 0)
    return false;

  for (size_t i = 0; i < list->GetSize(); ++i) {
    std::string str;
    if (!list->GetString(i, &str))
      return false;
    PermissionTypes type = PermissionStringToType(str);
    if (type == kNone)
      return false;
    permissions_.insert(type);
  }
  return true;
}

void MediaGalleriesPermission::ToValue(base::Value** value) const {
  base::ListValue* list = new ListValue();

  for (std::set<PermissionTypes>::const_iterator it = permissions_.begin();
       it != permissions_.end();
       ++it) {
    list->Append(base::Value::CreateStringValue(PermissionTypeToString(*it)));
  }
  *value = list;
}

APIPermission* MediaGalleriesPermission::Clone() const {
  MediaGalleriesPermission* result = new MediaGalleriesPermission(info());
  result->permissions_ = permissions_;
  return result;
}

APIPermission* MediaGalleriesPermission::Diff(const APIPermission* rhs) const {
  CHECK(rhs->info() == info());
  const MediaGalleriesPermission* perm =
      static_cast<const MediaGalleriesPermission*>(rhs);
  scoped_ptr<MediaGalleriesPermission> result(
      new MediaGalleriesPermission(info()));
  std::set_difference(permissions_.begin(), permissions_.end(),
                      perm->permissions_.begin(), perm->permissions_.end(),
                      std::inserter<std::set<PermissionTypes> >(
                          result->permissions_, result->permissions_.begin()));
  return result->permissions_.empty() ? NULL : result.release();
}

APIPermission* MediaGalleriesPermission::Union(const APIPermission* rhs) const {
  CHECK(rhs->info() == info());
  const MediaGalleriesPermission* perm =
      static_cast<const MediaGalleriesPermission*>(rhs);
  scoped_ptr<MediaGalleriesPermission> result(
      new MediaGalleriesPermission(info()));
  std::set_union(permissions_.begin(), permissions_.end(),
                 perm->permissions_.begin(), perm->permissions_.end(),
                 std::inserter<std::set<PermissionTypes> >(
                     result->permissions_, result->permissions_.begin()));
  return result->permissions_.empty() ? NULL : result.release();
}

APIPermission* MediaGalleriesPermission::Intersect(
    const APIPermission* rhs) const {
  CHECK(rhs->info() == info());
  const MediaGalleriesPermission* perm =
      static_cast<const MediaGalleriesPermission*>(rhs);
  scoped_ptr<MediaGalleriesPermission> result(
      new MediaGalleriesPermission(info()));
  std::set_intersection(permissions_.begin(), permissions_.end(),
                        perm->permissions_.begin(), perm->permissions_.end(),
                        std::inserter<std::set<PermissionTypes> >(
                            result->permissions_,
                            result->permissions_.begin()));
  return result->permissions_.empty() ? NULL : result.release();
}

void MediaGalleriesPermission::Write(IPC::Message* m) const {
  IPC::WriteParam(m, permissions_);
}

bool MediaGalleriesPermission::Read(const IPC::Message* m,
                                    PickleIterator* iter) {
  return IPC::ReadParam(m, iter, &permissions_);
}

void MediaGalleriesPermission::Log(std::string* log) const {
  IPC::LogParam(permissions_, log);
}

}  // namespace extensions
