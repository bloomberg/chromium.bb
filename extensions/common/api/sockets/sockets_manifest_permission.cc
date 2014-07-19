// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/sockets/sockets_manifest_permission.h"

#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/api/extensions_manifest_types.h"
#include "extensions/common/api/sockets/sockets_manifest_data.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_constants.h"
#include "grit/extensions_strings.h"
#include "ipc/ipc_message.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace sockets_errors {
const char kErrorInvalidHostPattern[] = "Invalid host:port pattern '*'";
}

namespace errors = sockets_errors;
using core_api::extensions_manifest_types::Sockets;
using core_api::extensions_manifest_types::SocketHostPatterns;
using content::SocketPermissionRequest;

namespace {

static bool ParseHostPattern(
    SocketsManifestPermission* permission,
    content::SocketPermissionRequest::OperationType operation_type,
    const std::string& host_pattern,
    base::string16* error) {
  SocketPermissionEntry entry;
  if (!SocketPermissionEntry::ParseHostPattern(
          operation_type, host_pattern, &entry)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kErrorInvalidHostPattern, host_pattern);
    return false;
  }
  permission->AddPermission(entry);
  return true;
}

static bool ParseHostPatterns(
    SocketsManifestPermission* permission,
    content::SocketPermissionRequest::OperationType operation_type,
    const scoped_ptr<SocketHostPatterns>& host_patterns,
    base::string16* error) {
  if (!host_patterns)
    return true;

  if (host_patterns->as_string) {
    return ParseHostPattern(
        permission, operation_type, *host_patterns->as_string, error);
  }

  CHECK(host_patterns->as_strings);
  for (std::vector<std::string>::const_iterator it =
           host_patterns->as_strings->begin();
       it != host_patterns->as_strings->end();
       ++it) {
    if (!ParseHostPattern(permission, operation_type, *it, error)) {
      return false;
    }
  }
  return true;
}

static void SetHostPatterns(
    scoped_ptr<SocketHostPatterns>& host_patterns,
    const SocketsManifestPermission* permission,
    content::SocketPermissionRequest::OperationType operation_type) {
  host_patterns.reset(new SocketHostPatterns());
  host_patterns->as_strings.reset(new std::vector<std::string>());
  for (SocketsManifestPermission::SocketPermissionEntrySet::const_iterator it =
           permission->entries().begin();
       it != permission->entries().end();
       ++it) {
    if (it->pattern().type == operation_type) {
      host_patterns->as_strings->push_back(it->GetHostPatternAsString());
    }
  }
}

}  // namespace

SocketsManifestPermission::SocketsManifestPermission() {}

SocketsManifestPermission::~SocketsManifestPermission() {}

// static
scoped_ptr<SocketsManifestPermission> SocketsManifestPermission::FromValue(
    const base::Value& value,
    base::string16* error) {
  scoped_ptr<Sockets> sockets = Sockets::FromValue(value, error);
  if (!sockets)
    return scoped_ptr<SocketsManifestPermission>();

  scoped_ptr<SocketsManifestPermission> result(new SocketsManifestPermission());
  if (sockets->udp) {
    if (!ParseHostPatterns(result.get(),
                           SocketPermissionRequest::UDP_BIND,
                           sockets->udp->bind,
                           error)) {
      return scoped_ptr<SocketsManifestPermission>();
    }
    if (!ParseHostPatterns(result.get(),
                           SocketPermissionRequest::UDP_SEND_TO,
                           sockets->udp->send,
                           error)) {
      return scoped_ptr<SocketsManifestPermission>();
    }
    if (!ParseHostPatterns(result.get(),
                           SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP,
                           sockets->udp->multicast_membership,
                           error)) {
      return scoped_ptr<SocketsManifestPermission>();
    }
  }
  if (sockets->tcp) {
    if (!ParseHostPatterns(result.get(),
                           SocketPermissionRequest::TCP_CONNECT,
                           sockets->tcp->connect,
                           error)) {
      return scoped_ptr<SocketsManifestPermission>();
    }
  }
  if (sockets->tcp_server) {
    if (!ParseHostPatterns(result.get(),
                           SocketPermissionRequest::TCP_LISTEN,
                           sockets->tcp_server->listen,
                           error)) {
      return scoped_ptr<SocketsManifestPermission>();
    }
  }
  return result.Pass();
}

bool SocketsManifestPermission::CheckRequest(
    const Extension* extension,
    const SocketPermissionRequest& request) const {
  for (SocketPermissionEntrySet::const_iterator it = permissions_.begin();
       it != permissions_.end();
       ++it) {
    if (it->Check(request))
      return true;
  }
  return false;
}

std::string SocketsManifestPermission::name() const {
  return manifest_keys::kSockets;
}

std::string SocketsManifestPermission::id() const { return name(); }

bool SocketsManifestPermission::HasMessages() const {
  bool is_empty = permissions_.empty();
  return !is_empty;
}

PermissionMessages SocketsManifestPermission::GetMessages() const {
  // TODO(rpaquay): This function and callees is (almost) a copy/paste
  // from extensions::SocketPermissiona.
  PermissionMessages result;
  if (!AddAnyHostMessage(result)) {
    AddSpecificHostMessage(result);
    AddSubdomainHostMessage(result);
  }
  AddNetworkListMessage(result);
  return result;
}

bool SocketsManifestPermission::FromValue(const base::Value* value) {
  if (!value)
    return false;
  base::string16 error;
  scoped_ptr<SocketsManifestPermission> manifest_permission(
      SocketsManifestPermission::FromValue(*value, &error));

  if (!manifest_permission)
    return false;

  permissions_ = manifest_permission->permissions_;
  return true;
}

scoped_ptr<base::Value> SocketsManifestPermission::ToValue() const {
  Sockets sockets;

  sockets.udp.reset(new Sockets::Udp());
  SetHostPatterns(sockets.udp->bind, this, SocketPermissionRequest::UDP_BIND);
  SetHostPatterns(
      sockets.udp->send, this, SocketPermissionRequest::UDP_SEND_TO);
  SetHostPatterns(sockets.udp->multicast_membership,
                  this,
                  SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP);
  if (sockets.udp->bind->as_strings->size() == 0 &&
      sockets.udp->send->as_strings->size() == 0 &&
      sockets.udp->multicast_membership->as_strings->size() == 0) {
    sockets.udp.reset(NULL);
  }

  sockets.tcp.reset(new Sockets::Tcp());
  SetHostPatterns(
      sockets.tcp->connect, this, SocketPermissionRequest::TCP_CONNECT);
  if (sockets.tcp->connect->as_strings->size() == 0) {
    sockets.tcp.reset(NULL);
  }

  sockets.tcp_server.reset(new Sockets::TcpServer());
  SetHostPatterns(
      sockets.tcp_server->listen, this, SocketPermissionRequest::TCP_LISTEN);
  if (sockets.tcp_server->listen->as_strings->size() == 0) {
    sockets.tcp_server.reset(NULL);
  }

  return scoped_ptr<base::Value>(sockets.ToValue().release()).Pass();
}

ManifestPermission* SocketsManifestPermission::Diff(
    const ManifestPermission* rhs) const {
  const SocketsManifestPermission* other =
      static_cast<const SocketsManifestPermission*>(rhs);

  scoped_ptr<SocketsManifestPermission> result(new SocketsManifestPermission());
  result->permissions_ = base::STLSetDifference<SocketPermissionEntrySet>(
      permissions_, other->permissions_);
  return result.release();
}

ManifestPermission* SocketsManifestPermission::Union(
    const ManifestPermission* rhs) const {
  const SocketsManifestPermission* other =
      static_cast<const SocketsManifestPermission*>(rhs);

  scoped_ptr<SocketsManifestPermission> result(new SocketsManifestPermission());
  result->permissions_ = base::STLSetUnion<SocketPermissionEntrySet>(
      permissions_, other->permissions_);
  return result.release();
}

ManifestPermission* SocketsManifestPermission::Intersect(
    const ManifestPermission* rhs) const {
  const SocketsManifestPermission* other =
      static_cast<const SocketsManifestPermission*>(rhs);

  scoped_ptr<SocketsManifestPermission> result(new SocketsManifestPermission());
  result->permissions_ = base::STLSetIntersection<SocketPermissionEntrySet>(
      permissions_, other->permissions_);
  return result.release();
}

void SocketsManifestPermission::AddPermission(
    const SocketPermissionEntry& entry) {
  permissions_.insert(entry);
}

bool SocketsManifestPermission::AddAnyHostMessage(
    PermissionMessages& messages) const {
  for (SocketPermissionEntrySet::const_iterator it = permissions_.begin();
       it != permissions_.end();
       ++it) {
    if (it->IsAddressBoundType() &&
        it->GetHostType() == SocketPermissionEntry::ANY_HOST) {
      messages.push_back(
          PermissionMessage(PermissionMessage::kSocketAnyHost,
                            l10n_util::GetStringUTF16(
                                IDS_EXTENSION_PROMPT_WARNING_SOCKET_ANY_HOST)));
      return true;
    }
  }
  return false;
}

void SocketsManifestPermission::AddSubdomainHostMessage(
    PermissionMessages& messages) const {
  std::set<base::string16> domains;
  for (SocketPermissionEntrySet::const_iterator it = permissions_.begin();
       it != permissions_.end();
       ++it) {
    if (it->GetHostType() == SocketPermissionEntry::HOSTS_IN_DOMAINS)
      domains.insert(base::UTF8ToUTF16(it->pattern().host));
  }
  if (!domains.empty()) {
    int id = (domains.size() == 1)
                 ? IDS_EXTENSION_PROMPT_WARNING_SOCKET_HOSTS_IN_DOMAIN
                 : IDS_EXTENSION_PROMPT_WARNING_SOCKET_HOSTS_IN_DOMAINS;
    messages.push_back(PermissionMessage(
        PermissionMessage::kSocketDomainHosts,
        l10n_util::GetStringFUTF16(
            id,
            JoinString(
                std::vector<base::string16>(domains.begin(), domains.end()),
                ' '))));
  }
}

void SocketsManifestPermission::AddSpecificHostMessage(
    PermissionMessages& messages) const {
  std::set<base::string16> hostnames;
  for (SocketPermissionEntrySet::const_iterator it = permissions_.begin();
       it != permissions_.end();
       ++it) {
    if (it->GetHostType() == SocketPermissionEntry::SPECIFIC_HOSTS)
      hostnames.insert(base::UTF8ToUTF16(it->pattern().host));
  }
  if (!hostnames.empty()) {
    int id = (hostnames.size() == 1)
                 ? IDS_EXTENSION_PROMPT_WARNING_SOCKET_SPECIFIC_HOST
                 : IDS_EXTENSION_PROMPT_WARNING_SOCKET_SPECIFIC_HOSTS;
    messages.push_back(PermissionMessage(
        PermissionMessage::kSocketSpecificHosts,
        l10n_util::GetStringFUTF16(
            id,
            JoinString(
                std::vector<base::string16>(hostnames.begin(), hostnames.end()),
                ' '))));
  }
}

void SocketsManifestPermission::AddNetworkListMessage(
    PermissionMessages& messages) const {
  for (SocketPermissionEntrySet::const_iterator it = permissions_.begin();
       it != permissions_.end();
       ++it) {
    if (it->pattern().type == SocketPermissionRequest::NETWORK_STATE) {
      messages.push_back(
          PermissionMessage(PermissionMessage::kNetworkState,
                            l10n_util::GetStringUTF16(
                                IDS_EXTENSION_PROMPT_WARNING_NETWORK_STATE)));
    }
  }
}

}  // namespace extensions
