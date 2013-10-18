// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/sockets/sockets_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/api/manifest_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/permissions/permissions_data.h"
#include "chrome/common/extensions/permissions/socket_permission_data.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/api_permission_set.h"

namespace extensions {

namespace sockets_errors {
const char kErrorInvalidHostPattern[] = "Invalid host:port pattern '*'";
}

namespace keys = extensions::manifest_keys;
namespace errors = sockets_errors;
using api::manifest_types::Sockets;

SocketsHandler::SocketsHandler() {}

SocketsHandler::~SocketsHandler() {}

bool SocketsHandler::Parse(Extension* extension, string16* error) {
  const base::Value* sockets = NULL;
  CHECK(extension->manifest()->Get(keys::kSockets, &sockets));
  std::vector<InstallWarning> install_warnings;
  scoped_ptr<SocketsManifestData> data =
      SocketsManifestData::FromValue(*sockets,
                                     &install_warnings,
                                     error);
  if (!data)
    return false;

  extension->AddInstallWarnings(install_warnings);
  extension->SetManifestData(keys::kSockets, data.release());
  return true;
}

const std::vector<std::string> SocketsHandler::Keys() const {
  return SingleKey(manifest_keys::kSockets);
}

SocketsManifestData::SocketsManifestData() {}
SocketsManifestData::~SocketsManifestData() {}

// static
SocketsManifestData* SocketsManifestData::Get(const Extension* extension) {
  return static_cast<SocketsManifestData*>(
      extension->GetManifestData(keys::kSockets));
}

// static
bool SocketsManifestData::CheckRequest(
    const Extension* extension,
    const content::SocketPermissionRequest& request) {
  SocketsManifestData* data = SocketsManifestData::Get(extension);
  if (data == NULL)
    return false;

  return data->CheckRequestImpl(extension, request);
}

// static
scoped_ptr<SocketsManifestData> SocketsManifestData::FromValue(
    const base::Value& value,
    std::vector<InstallWarning>* install_warnings,
    string16* error) {
  scoped_ptr<Sockets> sockets = Sockets::FromValue(value, error);
  if (!sockets)
    return scoped_ptr<SocketsManifestData>();

  scoped_ptr<SocketsManifestData> result(new SocketsManifestData());
  if (sockets->udp) {
    if (!ParseHostPattern(result.get(),
        content::SocketPermissionRequest::UDP_BIND,
        sockets->udp->bind,
        error)) {
      return scoped_ptr<SocketsManifestData>();
    }
    if (!ParseHostPattern(result.get(),
        content::SocketPermissionRequest::UDP_SEND_TO,
        sockets->udp->send,
        error)) {
      return scoped_ptr<SocketsManifestData>();
    }
    if (!ParseHostPattern(result.get(),
        content::SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP,
        sockets->udp->multicast_membership,
        error)) {
      return scoped_ptr<SocketsManifestData>();
    }
  }
  if (sockets->tcp) {
    if (!ParseHostPattern(result.get(),
        content::SocketPermissionRequest::TCP_CONNECT,
        sockets->tcp->connect,
        error)) {
      return scoped_ptr<SocketsManifestData>();
    }
  }
  if (sockets->tcp_server) {
    if (!ParseHostPattern(result.get(),
        content::SocketPermissionRequest::TCP_LISTEN,
        sockets->tcp_server->listen,
        error)) {
      return scoped_ptr<SocketsManifestData>();
    }
  }
  return result.Pass();
}

// static
bool SocketsManifestData::ParseHostPattern(
    SocketsManifestData* manifest_data,
    content::SocketPermissionRequest::OperationType operation_type,
    const scoped_ptr<std::string>& value,
    string16* error) {
  if (value) {
    SocketPermissionEntry entry;
    if (!SocketPermissionEntry::ParseHostPattern(
          operation_type, *value, &entry)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kErrorInvalidHostPattern, *value);
      return false;
    }
    manifest_data->AddPermission(entry);
  }
  return true;
}

void SocketsManifestData::AddPermission(const SocketPermissionEntry& entry) {
  permissions_.insert(entry);
}

bool SocketsManifestData::CheckRequestImpl(
    const Extension* extension,
    const content::SocketPermissionRequest& request) {
  for (PermissionSet::const_iterator it = permissions_.begin();
       it != permissions_.end(); ++it) {
    if (it->Check(request))
      return true;
  }
  return false;
}

}  // namespace extensions
