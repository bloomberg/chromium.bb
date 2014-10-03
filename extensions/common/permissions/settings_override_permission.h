// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_SETTINGS_OVERRIDE_PERMISSION_H_
#define EXTENSIONS_COMMON_PERMISSIONS_SETTINGS_OVERRIDE_PERMISSION_H_

#include <string>

#include "extensions/common/permissions/api_permission.h"

namespace extensions {

// Takes care of creating custom permission messages for extensions that
// override settings.
class SettingsOverrideAPIPermission : public APIPermission {
 public:
  SettingsOverrideAPIPermission(const APIPermissionInfo* permission,
                                const std::string& setting_value);
  virtual ~SettingsOverrideAPIPermission();

  // APIPermission overrides.
  virtual bool HasMessages() const override;
  virtual PermissionMessages GetMessages() const override;
  virtual bool Check(const APIPermission::CheckParam* param) const override;
  virtual bool Contains(const APIPermission* rhs) const override;
  virtual bool Equal(const APIPermission* rhs) const override;
  virtual bool FromValue(
      const base::Value* value,
      std::string* error,
      std::vector<std::string>* unhandled_permissions) override;
  virtual scoped_ptr<base::Value> ToValue() const override;
  virtual APIPermission* Clone() const override;
  virtual APIPermission* Diff(const APIPermission* rhs) const override;
  virtual APIPermission* Union(const APIPermission* rhs) const override;
  virtual APIPermission* Intersect(const APIPermission* rhs) const override;
  virtual void Write(IPC::Message* m) const override;
  virtual bool Read(const IPC::Message* m, PickleIterator* iter) override;
  virtual void Log(std::string* log) const override;

 private:
  std::string setting_value_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_SETTINGS_OVERRIDE_PERMISSION_H_
