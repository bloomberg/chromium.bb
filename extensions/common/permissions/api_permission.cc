// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/api_permission.h"

#include "ui/base/l10n/l10n_util.h"

namespace {

using extensions::APIPermission;
using extensions::APIPermissionInfo;
using extensions::PermissionMessage;
using extensions::PermissionMessages;

class SimpleAPIPermission : public APIPermission {
 public:
  explicit SimpleAPIPermission(const APIPermissionInfo* permission)
    : APIPermission(permission) { }

  virtual ~SimpleAPIPermission() { }

  virtual bool HasMessages() const OVERRIDE {
    return info()->message_id() > PermissionMessage::kNone;
  }

  virtual PermissionMessages GetMessages() const OVERRIDE {
    DCHECK(HasMessages());
    PermissionMessages result;
    result.push_back(GetMessage_());
    return result;
  }

  virtual bool Check(
      const APIPermission::CheckParam* param) const OVERRIDE {
    return !param;
  }

  virtual bool Contains(const APIPermission* rhs) const OVERRIDE {
    CHECK_EQ(info(), rhs->info());
    return true;
  }

  virtual bool Equal(const APIPermission* rhs) const OVERRIDE {
    if (this != rhs)
      CHECK_EQ(info(), rhs->info());
    return true;
  }

  virtual bool FromValue(
      const base::Value* value,
      std::string* /*error*/,
      std::vector<std::string>* /*unhandled_permissions*/) OVERRIDE {
    return (value == NULL);
  }

  virtual scoped_ptr<base::Value> ToValue() const OVERRIDE {
    return scoped_ptr<base::Value>();
  }

  virtual APIPermission* Clone() const OVERRIDE {
    return new SimpleAPIPermission(info());
  }

  virtual APIPermission* Diff(const APIPermission* rhs) const OVERRIDE {
    CHECK_EQ(info(), rhs->info());
    return NULL;
  }

  virtual APIPermission* Union(const APIPermission* rhs) const OVERRIDE {
    CHECK_EQ(info(), rhs->info());
    return new SimpleAPIPermission(info());
  }

  virtual APIPermission* Intersect(const APIPermission* rhs) const OVERRIDE {
    CHECK_EQ(info(), rhs->info());
    return new SimpleAPIPermission(info());
  }

  virtual void Write(IPC::Message* m) const OVERRIDE { }

  virtual bool Read(const IPC::Message* m, PickleIterator* iter) OVERRIDE {
    return true;
  }

  virtual void Log(std::string* log) const OVERRIDE { }
};

}  // namespace

namespace extensions {

APIPermission::APIPermission(const APIPermissionInfo* info)
  : info_(info) {
  DCHECK(info_);
}

APIPermission::~APIPermission() { }

APIPermission::ID APIPermission::id() const {
  return info()->id();
}

const char* APIPermission::name() const {
  return info()->name();
}

PermissionMessage APIPermission::GetMessage_() const {
  return info()->GetMessage_();
}

//
// APIPermissionInfo
//

APIPermissionInfo::APIPermissionInfo(const APIPermissionInfo::InitInfo& info)
    : id_(info.id),
      name_(info.name),
      flags_(info.flags),
      l10n_message_id_(info.l10n_message_id),
      message_id_(info.message_id ? info.message_id : PermissionMessage::kNone),
      api_permission_constructor_(info.constructor) {
}

APIPermissionInfo::~APIPermissionInfo() { }

APIPermission* APIPermissionInfo::CreateAPIPermission() const {
  return api_permission_constructor_ ?
    api_permission_constructor_(this) : new SimpleAPIPermission(this);
}

PermissionMessage APIPermissionInfo::GetMessage_() const {
  return PermissionMessage(
      message_id_, l10n_util::GetStringUTF16(l10n_message_id_));
}

}  // namespace extensions
