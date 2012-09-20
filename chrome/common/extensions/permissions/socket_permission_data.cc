// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/socket_permission_data.h"

#include <cstdlib>
#include <sstream>
#include <vector>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "googleurl/src/url_canon.h"

namespace {

using extensions::SocketPermissionData;

const char kColon = ':';
const char kDot = '.';
const char kWildcard[] = "*";
const char kInvalid[] = "invalid";
const char kTCPConnect[] = "tcp-connect";
const char kTCPListen[] = "tcp-listen";
const char kUDPBind[] = "udp-bind";
const char kUDPSendTo[] = "udp-send-to";
const int kAnyPort = 0;
const int kInvalidPort = -1;

SocketPermissionData::OperationType StringToType(const std::string& s) {
  if (s == kTCPConnect)
    return SocketPermissionData::TCP_CONNECT;
  if (s == kTCPListen)
    return SocketPermissionData::TCP_LISTEN;
  if (s == kUDPBind)
    return SocketPermissionData::UDP_BIND;
  if (s == kUDPSendTo)
    return SocketPermissionData::UDP_SEND_TO;
  return SocketPermissionData::NONE;
}

const char* TypeToString(SocketPermissionData::OperationType type) {
  switch (type) {
    case SocketPermissionData::TCP_CONNECT:
      return kTCPConnect;
    case SocketPermissionData::TCP_LISTEN:
      return kTCPListen;
    case SocketPermissionData::UDP_BIND:
      return kUDPBind;
    case SocketPermissionData::UDP_SEND_TO:
      return kUDPSendTo;
    default:
      return kInvalid;
  }
}

bool StartsOrEndsWithWhitespace(const std::string& str) {
  if (str.find_first_not_of(kWhitespaceASCII) != 0)
    return true;
  if (str.find_last_not_of(kWhitespaceASCII) != str.length() - 1)
    return true;
  return false;
}

}  // namespace

namespace extensions {

SocketPermissionData::SocketPermissionData() {
  Reset();
}

SocketPermissionData::~SocketPermissionData() {
}

bool SocketPermissionData::operator<(const SocketPermissionData& rhs) const {
  if (type_ < rhs.type_)
    return true;
  if (type_ > rhs.type_)
    return false;

  if (host_ < rhs.host_)
    return true;
  if (host_ > rhs.host_)
    return false;

  if (match_subdomains_ < rhs.match_subdomains_)
    return true;
  if (match_subdomains_ > rhs.match_subdomains_)
    return false;

  if (port_ < rhs.port_)
    return true;
  return false;
}

bool SocketPermissionData::operator==(const SocketPermissionData& rhs) const {
  return (type_ == rhs.type_) && (host_ == rhs.host_) &&
      (match_subdomains_ == rhs.match_subdomains_) &&
      (port_ == rhs.port_);
}

bool SocketPermissionData::Match(
    OperationType type, const std::string& host, int port) const {
  if (type_ != type)
    return false;

  std::string lhost = StringToLowerASCII(host);
  if (host_ != lhost) {
    if (!match_subdomains_)
      return false;

    if (!host_.empty()) {
      // Do not wildcard part of IP address.
      url_parse::Component component(0, lhost.length());
      url_canon::RawCanonOutputT<char, 128> ignored_output;
      url_canon::CanonHostInfo host_info;
      url_canon::CanonicalizeIPAddress(lhost.c_str(), component,
                                       &ignored_output, &host_info);
      if (host_info.IsIPAddress())
        return false;

      // host should equal one or more chars + "." +  host_.
      int i = lhost.length() - host_.length();
      if (i < 2)
        return false;

      if (lhost.compare(i, host_.length(), host_) != 0)
        return false;

      if (lhost[i - 1] != kDot)
        return false;
    }
  }

  if (port_ != port && port_ != kAnyPort)
    return false;

  return true;
}

bool SocketPermissionData::Parse(const std::string& permission) {
  do {
    host_.clear();
    match_subdomains_ = true;
    port_ = kAnyPort;
    spec_.clear();

    std::vector<std::string> tokens;
    base::SplitStringDontTrim(permission, kColon, &tokens);

    if (tokens.empty() || tokens.size() > 3)
      break;

    type_ = StringToType(tokens[0]);
    if (type_ == NONE)
      break;

    if (tokens.size() == 1)
      return true;

    host_ = tokens[1];
    if (!host_.empty()) {
      if (StartsOrEndsWithWhitespace(host_))
        break;
      host_ = StringToLowerASCII(host_);

      // The first component can optionally be '*' to match all subdomains.
      std::vector<std::string> host_components;
      base::SplitString(host_, kDot, &host_components);
      DCHECK(!host_components.empty());

      if (host_components[0] == kWildcard || host_components[0].empty()) {
        host_components.erase(host_components.begin(),
                              host_components.begin() + 1);
      } else {
        match_subdomains_ = false;
      }
      host_ = JoinString(host_components, kDot);
    }

    if (tokens.size() == 2 || tokens[2].empty() || tokens[2] == kWildcard)
      return true;

    if (StartsOrEndsWithWhitespace(tokens[2]))
      break;

    if (!base::StringToInt(tokens[2], &port_) ||
        port_ < 1 || port_ > 65535)
      break;
    return true;
  } while (false);

  Reset();
  return false;
}

SocketPermissionData::HostType SocketPermissionData::GetHostType() const {
  return host_.empty() ?     SocketPermissionData::ANY_HOST :
         match_subdomains_ ? SocketPermissionData::HOSTS_IN_DOMAINS :
                             SocketPermissionData::SPECIFIC_HOSTS;
}

const std::string SocketPermissionData::GetHost() const {
  return host_;
}

const std::string& SocketPermissionData::GetAsString() const {
  if (!spec_.empty())
    return spec_;

  spec_.reserve(64);
  spec_.append(TypeToString(type_));

  if (match_subdomains_) {
    spec_.append(1, kColon).append(kWildcard);
    if (!host_.empty())
      spec_.append(1, kDot).append(host_);
  } else {
     spec_.append(1, kColon).append(host_);
  }

  if (port_ == kAnyPort)
    spec_.append(1, kColon).append(kWildcard);
  else
    spec_.append(1, kColon).append(base::IntToString(port_));

  return spec_;
}

void SocketPermissionData::Reset() {
  type_ = NONE;
  host_.clear();
  match_subdomains_ = false;
  port_ = kInvalidPort;
  spec_.clear();
}

}  // namespace extensions
