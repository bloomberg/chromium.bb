// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/socket_permission.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

SocketPermission::SocketPermission(const APIPermissionInfo* info)
  : APIPermission(info) {
}

SocketPermission::~SocketPermission() {
}

bool SocketPermission::HasMessages() const {
  return info()->message_id() > PermissionMessage::kNone;
}

PermissionMessages SocketPermission::GetMessages() const {
  DCHECK(HasMessages());
  PermissionMessages result;
  result.push_back(GetMessage_());
  return result;
}

bool SocketPermission::Check(
    const APIPermission::CheckParam* param) const {
  const CheckParam* socket_param = static_cast<const CheckParam*>(param);
  std::set<SocketPermissionData>::const_iterator it = data_set_.begin();
  std::set<SocketPermissionData>::const_iterator end = data_set_.end();

  for (; it != end; ++it) {
    if (it->Match(socket_param->type, socket_param->host, socket_param->port))
      return true;
  }
  return false;
}

bool SocketPermission::Contains(const APIPermission* rhs) const {
  CHECK(rhs->info() == info());
  const SocketPermission* perm = static_cast<const SocketPermission*>(rhs);
  return std::includes(data_set_.begin(), data_set_.end(),
                       perm->data_set_.begin(), perm->data_set_.end());
}

bool SocketPermission::Equal(const APIPermission* rhs) const {
  CHECK(rhs->info() == info());
  const SocketPermission* perm = static_cast<const SocketPermission*>(rhs);
  return data_set_ == perm->data_set_;
}

bool SocketPermission::FromValue(const base::Value* value) {
  data_set_.clear();
  const base::ListValue* list = NULL;

  if (!value)
    return false;

  if (!value->GetAsList(&list) || list->GetSize() == 0)
    return false;

  for (size_t i = 0; i < list->GetSize(); ++i) {
    std::string str;
    SocketPermissionData data;
    if (!list->GetString(i, &str) || !data.Parse(str))
      return false;
    data_set_.insert(data);
  }
  return true;
}

void SocketPermission::ToValue(base::Value** value) const {
  base::ListValue* list = new ListValue();

  std::set<SocketPermissionData>::const_iterator it = data_set_.begin();
  std::set<SocketPermissionData>::const_iterator end = data_set_.end();

  for (;it != end; ++it) {
    list->Append(base::Value::CreateStringValue(it->GetAsString()));
  }
  *value = list;
}

APIPermission* SocketPermission::Clone() const {
  SocketPermission* result = new SocketPermission(info());
  result->data_set_ = data_set_;
  return result;
}

APIPermission* SocketPermission::Diff(const APIPermission* rhs) const {
  CHECK(rhs->info() == info());
  const SocketPermission* perm = static_cast<const SocketPermission*>(rhs);
  scoped_ptr<SocketPermission> result(new SocketPermission(info()));
  std::set_difference(data_set_.begin(), data_set_.end(),
                      perm->data_set_.begin(), perm->data_set_.end(),
                      std::inserter<std::set<SocketPermissionData> >(
                          result->data_set_, result->data_set_.begin()));
  return result->data_set_.empty() ? NULL : result.release();
}

APIPermission* SocketPermission::Union(const APIPermission* rhs) const {
  CHECK(rhs->info() == info());
  const SocketPermission* perm = static_cast<const SocketPermission*>(rhs);
  scoped_ptr<SocketPermission> result(new SocketPermission(info()));
  std::set_union(data_set_.begin(), data_set_.end(),
                 perm->data_set_.begin(), perm->data_set_.end(),
                 std::inserter<std::set<SocketPermissionData> >(
                     result->data_set_, result->data_set_.begin()));
  return result->data_set_.empty() ? NULL : result.release();
}

APIPermission* SocketPermission::Intersect(const APIPermission* rhs) const {
  CHECK(rhs->info() == info());
  const SocketPermission* perm = static_cast<const SocketPermission*>(rhs);
  scoped_ptr<SocketPermission> result(new SocketPermission(info()));
  std::set_intersection(data_set_.begin(), data_set_.end(),
                        perm->data_set_.begin(), perm->data_set_.end(),
                        std::inserter<std::set<SocketPermissionData> >(
                            result->data_set_, result->data_set_.begin()));
  return result->data_set_.empty() ? NULL : result.release();
}

void SocketPermission::Write(IPC::Message* m) const {
  IPC::WriteParam(m, data_set_);
}

bool SocketPermission::Read(const IPC::Message* m, PickleIterator* iter) {
  return IPC::ReadParam(m, iter, &data_set_);
}

void SocketPermission::Log(std::string* log) const {
  IPC::LogParam(data_set_, log);
}

}  // namespace extensions

