// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/filesystem_permission.h"

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

const char kImplicitReadString[] = "read";
const char kWriteString[] = "write";
const char kInvalidString[] = "invalid";

}  // namespace

namespace extensions {

FileSystemPermission::FileSystemPermission(
    const APIPermissionInfo* info)
    : APIPermission(info) {
}

FileSystemPermission::~FileSystemPermission() {
}

// static
bool FileSystemPermission::HasReadAccess(const Extension& extension) {
  return extension.HasAPIPermission(extensions::APIPermission::kFileSystem);
}

// static
bool FileSystemPermission::HasWriteAccess(const Extension& extension) {
  CheckParam write_param(kWrite);
  return extension.CheckAPIPermissionWithParam(
      extensions::APIPermission::kFileSystem, &write_param);
}

// static
FileSystemPermission::PermissionTypes
FileSystemPermission::PermissionStringToType(const std::string& str) {
  if (str == kWriteString)
    return kWrite;
  if (str == kImplicitReadString)
    return kImplicitRead;
  return kNone;
}

// static
const char* FileSystemPermission::PermissionTypeToString(PermissionTypes type) {
  switch (type) {
    case kWrite:
      return kWriteString;
    case kImplicitRead:
      return kImplicitReadString;
    default:
      NOTREACHED();
      return kInvalidString;
  }
}

bool FileSystemPermission::HasMessages() const {
  CheckParam param(kWrite);
  return Check(&param);
}

PermissionMessages FileSystemPermission::GetMessages() const {
  CheckParam param(kWrite);
  if (Check(&param)) {
    PermissionMessages result;
    result.push_back(PermissionMessage(
        PermissionMessage::kFileSystemWrite,
        l10n_util::GetStringUTF16(
            IDS_EXTENSION_PROMPT_WARNING_FILE_SYSTEM_WRITE)));
    return result;
  }
  NOTREACHED();
  PermissionMessages result;
  result.push_back(PermissionMessage(PermissionMessage::kUnknown, string16()));
  return result;
}

bool FileSystemPermission::Check(
    const APIPermission::CheckParam* param) const {
  const CheckParam* filesystem_param =
      static_cast<const CheckParam*>(param);
  return ContainsKey(permissions_, filesystem_param->permission);
}

bool FileSystemPermission::Contains(const APIPermission* rhs) const {
  CHECK_EQ(rhs->info(), info());
  const FileSystemPermission* perm =
      static_cast<const FileSystemPermission*>(rhs);
  return std::includes(permissions_.begin(), permissions_.end(),
                       perm->permissions_.begin(), perm->permissions_.end());
}

bool FileSystemPermission::Equal(const APIPermission* rhs) const {
  CHECK(rhs->info() == info());
  const FileSystemPermission* perm =
      static_cast<const FileSystemPermission*>(rhs);
  return permissions_ == perm->permissions_;
}

bool FileSystemPermission::FromValue(const base::Value* value) {
  permissions_.clear();

  // Always configure with the implicit read string. This may also be specified
  // in the manifest, but is redundant.
  permissions_.insert(kImplicitRead);

  // The simple "fileSystem" form, without an argument, is allowed.
  if (!value)
    return true;

  const base::ListValue* list = NULL;
  if (!value->GetAsList(&list))
    return false;

  for (size_t i = 0; i < list->GetSize(); ++i) {
    std::string str;
    if (!list->GetString(i, &str))
      return false;
    PermissionTypes type = PermissionStringToType(str);
    if (type == kNone)  // should never be serialized
      return false;
    permissions_.insert(type);
  }
  return true;
}

void FileSystemPermission::ToValue(base::Value** value) const {
  base::ListValue* list = new ListValue();

  for (std::set<PermissionTypes>::const_iterator it = permissions_.begin();
       it != permissions_.end();
       ++it) {
    list->Append(base::Value::CreateStringValue(PermissionTypeToString(*it)));
  }
  *value = list;
}

APIPermission* FileSystemPermission::Clone() const {
  FileSystemPermission* result = new FileSystemPermission(info());
  result->permissions_ = permissions_;
  return result;
}

APIPermission* FileSystemPermission::Diff(const APIPermission* rhs) const {
  CHECK(rhs->info() == info());
  const FileSystemPermission* perm =
      static_cast<const FileSystemPermission*>(rhs);
  scoped_ptr<FileSystemPermission> result(
      new FileSystemPermission(info()));
  std::set_difference(permissions_.begin(), permissions_.end(),
                      perm->permissions_.begin(), perm->permissions_.end(),
                      std::inserter<std::set<PermissionTypes> >(
                          result->permissions_, result->permissions_.begin()));
  return result->permissions_.empty() ? NULL : result.release();
}

APIPermission* FileSystemPermission::Union(const APIPermission* rhs) const {
  CHECK(rhs->info() == info());
  const FileSystemPermission* perm =
      static_cast<const FileSystemPermission*>(rhs);
  scoped_ptr<FileSystemPermission> result(new FileSystemPermission(info()));
  std::set_union(permissions_.begin(), permissions_.end(),
                 perm->permissions_.begin(), perm->permissions_.end(),
                 std::inserter<std::set<PermissionTypes> >(
                     result->permissions_, result->permissions_.begin()));
  return result->permissions_.empty() ? NULL : result.release();
}

APIPermission* FileSystemPermission::Intersect(
    const APIPermission* rhs) const {
  CHECK(rhs->info() == info());
  const FileSystemPermission* perm =
      static_cast<const FileSystemPermission*>(rhs);
  scoped_ptr<FileSystemPermission> result(new FileSystemPermission(info()));
  std::set_intersection(permissions_.begin(), permissions_.end(),
                        perm->permissions_.begin(), perm->permissions_.end(),
                        std::inserter<std::set<PermissionTypes> >(
                            result->permissions_,
                            result->permissions_.begin()));
  return result->permissions_.empty() ? NULL : result.release();
}

void FileSystemPermission::Write(IPC::Message* m) const {
  IPC::WriteParam(m, permissions_);
}

bool FileSystemPermission::Read(const IPC::Message* m,
                                    PickleIterator* iter) {
  return IPC::ReadParam(m, iter, &permissions_);
}

void FileSystemPermission::Log(std::string* log) const {
  IPC::LogParam(permissions_, log);
}

}  // namespace
