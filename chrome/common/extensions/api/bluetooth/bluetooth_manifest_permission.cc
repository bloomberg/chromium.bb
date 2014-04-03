// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/bluetooth/bluetooth_manifest_permission.h"

#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/api/bluetooth/bluetooth_manifest_data.h"
#include "chrome/common/extensions/api/manifest_types.h"
#include "device/bluetooth/bluetooth_utils.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_constants.h"
#include "grit/generated_resources.h"
#include "ipc/ipc_message.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace bluetooth_errors {
const char kErrorInvalidUuid[] = "Invalid UUID '*'";
}

namespace errors = bluetooth_errors;

namespace {

bool ParseUuid(BluetoothManifestPermission* permission,
               const std::string& uuid,
               base::string16* error) {
  std::string canonical_uuid = device::bluetooth_utils::CanonicalUuid(uuid);
  if (canonical_uuid.empty()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kErrorInvalidUuid, uuid);
    return false;
  }
  permission->AddPermission(uuid);
  return true;
}

bool ParseUuidArray(BluetoothManifestPermission* permission,
                    const scoped_ptr<std::vector<std::string> >& uuids,
                    base::string16* error) {
  for (std::vector<std::string>::const_iterator it = uuids->begin();
       it != uuids->end();
       ++it) {
    if (!ParseUuid(permission, *it, error)) {
      return false;
    }
  }
  return true;
}

}  // namespace

BluetoothManifestPermission::BluetoothManifestPermission() {}

BluetoothManifestPermission::~BluetoothManifestPermission() {}

// static
scoped_ptr<BluetoothManifestPermission> BluetoothManifestPermission::FromValue(
    const base::Value& value,
    base::string16* error) {
  scoped_ptr<api::manifest_types::Bluetooth> bluetooth =
      api::manifest_types::Bluetooth::FromValue(value, error);
  if (!bluetooth)
    return scoped_ptr<BluetoothManifestPermission>();

  scoped_ptr<BluetoothManifestPermission> result(
      new BluetoothManifestPermission());
  if (bluetooth->uuids) {
    if (!ParseUuidArray(result.get(), bluetooth->uuids, error)) {
      return scoped_ptr<BluetoothManifestPermission>();
    }
  }
  return result.Pass();
}

bool BluetoothManifestPermission::CheckRequest(
    const Extension* extension,
    const BluetoothPermissionRequest& request) const {

  std::string canonical_param_uuid =
      device::bluetooth_utils::CanonicalUuid(request.uuid);
  for (BluetoothUuidSet::const_iterator it = uuids_.begin();
       it != uuids_.end();
       ++it) {
    std::string canonical_uuid = device::bluetooth_utils::CanonicalUuid(*it);
    if (canonical_uuid == canonical_param_uuid)
      return true;
  }
  return false;
}

std::string BluetoothManifestPermission::name() const {
  return manifest_keys::kBluetooth;
}

std::string BluetoothManifestPermission::id() const { return name(); }

bool BluetoothManifestPermission::HasMessages() const { return true; }

PermissionMessages BluetoothManifestPermission::GetMessages() const {
  DCHECK(HasMessages());
  PermissionMessages result;

  result.push_back(PermissionMessage(
      PermissionMessage::kBluetooth,
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH)));

  if (!uuids_.empty()) {
    result.push_back(
        PermissionMessage(PermissionMessage::kBluetoothDevices,
                          l10n_util::GetStringUTF16(
                              IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH_DEVICES)));
  }

  return result;
}

bool BluetoothManifestPermission::FromValue(const base::Value* value) {
  if (!value)
    return false;
  base::string16 error;
  scoped_ptr<BluetoothManifestPermission> manifest_permission(
      BluetoothManifestPermission::FromValue(*value, &error));

  if (!manifest_permission)
    return false;

  uuids_ = manifest_permission->uuids_;
  return true;
}

scoped_ptr<base::Value> BluetoothManifestPermission::ToValue() const {
  api::manifest_types::Bluetooth bluetooth;
  bluetooth.uuids.reset(new std::vector<std::string>(uuids_.begin(),
                                                     uuids_.end()));
  return bluetooth.ToValue().PassAs<base::Value>();
}

ManifestPermission* BluetoothManifestPermission::Clone() const {
  scoped_ptr<BluetoothManifestPermission> result(
      new BluetoothManifestPermission());
  result->uuids_ = uuids_;
  return result.release();
}

ManifestPermission* BluetoothManifestPermission::Diff(
    const ManifestPermission* rhs) const {
  const BluetoothManifestPermission* other =
      static_cast<const BluetoothManifestPermission*>(rhs);

  scoped_ptr<BluetoothManifestPermission> result(
      new BluetoothManifestPermission());
  result->uuids_ = base::STLSetDifference<BluetoothUuidSet>(
      uuids_, other->uuids_);
  return result.release();
}

ManifestPermission* BluetoothManifestPermission::Union(
    const ManifestPermission* rhs) const {
  const BluetoothManifestPermission* other =
      static_cast<const BluetoothManifestPermission*>(rhs);

  scoped_ptr<BluetoothManifestPermission> result(
      new BluetoothManifestPermission());
  result->uuids_ = base::STLSetUnion<BluetoothUuidSet>(
      uuids_, other->uuids_);
  return result.release();
}

ManifestPermission* BluetoothManifestPermission::Intersect(
    const ManifestPermission* rhs) const {
  const BluetoothManifestPermission* other =
      static_cast<const BluetoothManifestPermission*>(rhs);

  scoped_ptr<BluetoothManifestPermission> result(
      new BluetoothManifestPermission());
  result->uuids_ = base::STLSetIntersection<BluetoothUuidSet>(
      uuids_, other->uuids_);
  return result.release();
}

bool BluetoothManifestPermission::Contains(const ManifestPermission* rhs)
    const {
  const BluetoothManifestPermission* other =
      static_cast<const BluetoothManifestPermission*>(rhs);

  return base::STLIncludes(uuids_, other->uuids_);
}

bool BluetoothManifestPermission::Equal(const ManifestPermission* rhs) const {
  const BluetoothManifestPermission* other =
      static_cast<const BluetoothManifestPermission*>(rhs);

  return (uuids_ == other->uuids_);
}

void BluetoothManifestPermission::Write(IPC::Message* m) const {
  IPC::WriteParam(m, uuids_);
}

bool BluetoothManifestPermission::Read(const IPC::Message* m,
                                       PickleIterator* iter) {
  return IPC::ReadParam(m, iter, &uuids_);
}

void BluetoothManifestPermission::Log(std::string* log) const {
  IPC::LogParam(uuids_, log);
}

void BluetoothManifestPermission::AddPermission(const std::string& uuid) {
  uuids_.insert(uuid);
}

}  // namespace extensions
