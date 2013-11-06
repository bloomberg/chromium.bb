// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_SOCKETS_SOCKETS_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_API_SOCKETS_SOCKETS_HANDLER_H_

#include "base/strings/string16.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/permissions/socket_permission_data.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

// Parses the "sockets" manifest key.
class SocketsHandler : public ManifestHandler {
 public:
  SocketsHandler();
  virtual ~SocketsHandler();

  virtual bool Parse(Extension* extension, string16* error) OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(SocketsHandler);
};

// The parsed form of the "sockets" manifest entry.
class SocketsManifestData : public Extension::ManifestData {
 public:
  SocketsManifestData();
  virtual ~SocketsManifestData();

  // Gets the ExternallyConnectableInfo for |extension|, or NULL if none was
  // specified.
  static SocketsManifestData* Get(const Extension* extension);

  static bool CheckRequest(const Extension* extension,
                           const content::SocketPermissionRequest& request);

  // Tries to construct the info based on |value|, as it would have appeared in
  // the manifest. Sets |error| and returns an empty scoped_ptr on failure.
  static scoped_ptr<SocketsManifestData> FromValue(
      const base::Value& value,
      std::vector<InstallWarning>* install_warnings,
      string16* error);

 private:
  typedef std::set<SocketPermissionEntry> PermissionSet;

  static bool ParseHostPattern(
      SocketsManifestData* manifest_data,
      content::SocketPermissionRequest::OperationType operation_type,
      const scoped_ptr<std::string>& value,
      string16* error);

  void AddPermission(const SocketPermissionEntry& entry);

  bool CheckRequestImpl(const Extension* extension,
                        const content::SocketPermissionRequest& request);

  PermissionSet permissions_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_SOCKETS_SOCKETS_HANDLER_H_
