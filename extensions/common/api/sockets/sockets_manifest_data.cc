// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/sockets/sockets_manifest_data.h"

#include "extensions/common/api/sockets/sockets_manifest_permission.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

SocketsManifestData::SocketsManifestData(
    scoped_ptr<SocketsManifestPermission> permission)
    : permission_(permission.Pass()) {
  DCHECK(permission_);
}

SocketsManifestData::~SocketsManifestData() {}

// static
SocketsManifestData* SocketsManifestData::Get(const Extension* extension) {
  return static_cast<SocketsManifestData*>(
      extension->GetManifestData(manifest_keys::kSockets));
}

// static
bool SocketsManifestData::CheckRequest(
    const Extension* extension,
    const content::SocketPermissionRequest& request) {
  const SocketsManifestData* data = SocketsManifestData::Get(extension);
  if (data)
    return data->permission()->CheckRequest(extension, request);

  return false;
}

// static
scoped_ptr<SocketsManifestData> SocketsManifestData::FromValue(
    const base::Value& value,
    base::string16* error) {
  scoped_ptr<SocketsManifestPermission> permission =
      SocketsManifestPermission::FromValue(value, error);
  if (!permission)
    return scoped_ptr<SocketsManifestData>();

  return scoped_ptr<SocketsManifestData>(
             new SocketsManifestData(permission.Pass())).Pass();
}

}  // namespace extensions
