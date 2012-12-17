// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_SET_DISJUNCTION_PERMISSION_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_SET_DISJUNCTION_PERMISSION_H_

#include <algorithm>
#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"

namespace extensions {

// An abstract base class for permissions that are represented by the
// disjunction of a set of conditions.  Each condition is represented by a
// |PermissionDataType| (e.g. SocketPermissionData).  If an
// APIPermission::CheckParam matches any of the conditions in the set, the
// permission is granted.
//
// For an example of how to use this class, see SocketPermission.
template <class PermissionDataType, class DerivedType>
class SetDisjunctionPermission : public APIPermission {
 public:
  explicit SetDisjunctionPermission(const APIPermissionInfo* info)
    : APIPermission(info) {
  }

  ~SetDisjunctionPermission() {
  }

  // APIPermission overrides
  virtual bool HasMessages() const OVERRIDE {
    return !data_set_.empty();
  }

  virtual bool Check(const APIPermission::CheckParam* param) const OVERRIDE {
    for (typename std::set<PermissionDataType>::const_iterator i =
        data_set_.begin(); i != data_set_.end(); ++i) {
      if (i->Check(param))
        return true;
    }
    return false;
  }

  virtual bool Contains(const APIPermission* rhs) const OVERRIDE {
    CHECK(rhs->info() == info());
    const SetDisjunctionPermission* perm =
        static_cast<const SetDisjunctionPermission*>(rhs);
    return std::includes(
        data_set_.begin(), data_set_.end(),
        perm->data_set_.begin(), perm->data_set_.end());
  }

  virtual bool Equal(const APIPermission* rhs) const OVERRIDE {
    CHECK(rhs->info() == info());
    const SetDisjunctionPermission* perm =
        static_cast<const SetDisjunctionPermission*>(rhs);
    return data_set_ == perm->data_set_;
  }

  virtual APIPermission* Clone() const OVERRIDE {
    SetDisjunctionPermission* result = new DerivedType(info());
    result->data_set_ = data_set_;
    return result;
  }

  virtual APIPermission* Diff(const APIPermission* rhs) const OVERRIDE {
    CHECK(rhs->info() == info());
    const SetDisjunctionPermission* perm =
        static_cast<const SetDisjunctionPermission*>(rhs);
    scoped_ptr<SetDisjunctionPermission> result(new DerivedType(info()));
    std::set_difference(
        data_set_.begin(), data_set_.end(),
        perm->data_set_.begin(), perm->data_set_.end(),
        std::inserter<std::set<PermissionDataType> >(
            result->data_set_, result->data_set_.begin()));
    return result->data_set_.empty() ? NULL : result.release();
  }

  virtual APIPermission* Union(const APIPermission* rhs) const OVERRIDE {
    CHECK(rhs->info() == info());
    const SetDisjunctionPermission* perm =
        static_cast<const SetDisjunctionPermission*>(rhs);
    scoped_ptr<SetDisjunctionPermission> result(new DerivedType(info()));
    std::set_union(
        data_set_.begin(), data_set_.end(),
        perm->data_set_.begin(), perm->data_set_.end(),
        std::inserter<std::set<PermissionDataType> >(
            result->data_set_, result->data_set_.begin()));
    return result.release();
  }

  virtual APIPermission* Intersect(const APIPermission* rhs) const OVERRIDE {
    CHECK(rhs->info() == info());
    const SetDisjunctionPermission* perm =
        static_cast<const SetDisjunctionPermission*>(rhs);
    scoped_ptr<SetDisjunctionPermission> result(new DerivedType(info()));
    std::set_intersection(
        data_set_.begin(), data_set_.end(),
        perm->data_set_.begin(), perm->data_set_.end(),
        std::inserter<std::set<PermissionDataType> >(
            result->data_set_, result->data_set_.begin()));
    return result->data_set_.empty() ? NULL : result.release();
  }

  virtual bool FromValue(const base::Value* value) OVERRIDE {
    data_set_.clear();
    const base::ListValue* list = NULL;

    if (!value)
      return false;

    if (!value->GetAsList(&list) || list->GetSize() == 0)
      return false;

    for (size_t i = 0; i < list->GetSize(); ++i) {
      const base::Value* item_value;
      if (!list->Get(i, &item_value))
        return false;

      PermissionDataType data;
      if (!data.FromValue(item_value))
        return false;

      data_set_.insert(data);
    }
    return true;
  }

  virtual scoped_ptr<base::Value> ToValue() const OVERRIDE {
    base::ListValue* list = new ListValue();
    typename std::set<PermissionDataType>::const_iterator i;
    for (i = data_set_.begin(); i != data_set_.end(); ++i) {
      scoped_ptr<base::Value> item_value(i->ToValue());
      list->Append(item_value.release());
    }
    return scoped_ptr<base::Value>(list);
  }

  virtual void Write(IPC::Message* m) const OVERRIDE {
    IPC::WriteParam(m, data_set_);
  }

  virtual bool Read(const IPC::Message* m, PickleIterator* iter) OVERRIDE {
    return IPC::ReadParam(m, iter, &data_set_);
  }

  virtual void Log(std::string* log) const OVERRIDE {
    IPC::LogParam(data_set_, log);
  }

 protected:
  std::set<PermissionDataType> data_set_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_SET_DISJUNCTION_PERMISSION_H_
