// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/printer_configuration.h"

#include <string>

#include "base/guid.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "net/base/ip_endpoint.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_constants.h"

#include "chromeos/printing/printing_constants.h"

namespace chromeos {

base::Optional<UriComponents> ParseUri(const std::string& printer_uri) {
  const char* uri_ptr = printer_uri.c_str();
  url::Parsed parsed;
  url::ParseStandardURL(uri_ptr, printer_uri.length(), &parsed);
  if (!parsed.scheme.is_valid() || !parsed.host.is_valid() ||
      !parsed.path.is_valid()) {
    return {};
  }
  base::StringPiece scheme(&uri_ptr[parsed.scheme.begin], parsed.scheme.len);
  base::StringPiece host(&uri_ptr[parsed.host.begin], parsed.host.len);
  base::StringPiece path(&uri_ptr[parsed.path.begin], parsed.path.len);

  bool encrypted = scheme != kIppScheme;
  int port = ParsePort(uri_ptr, parsed.port);
  // Port not specified.
  if (port == url::SpecialPort::PORT_UNSPECIFIED ||
      port == url::SpecialPort::PORT_INVALID) {
    if (scheme == kIppScheme) {
      port = kIppPort;
    } else if (scheme == kIppsScheme) {
      port = kIppsPort;
    }
  }

  return base::Optional<UriComponents>(base::in_place, encrypted,
                                       scheme.as_string(), host.as_string(),
                                       port, path.as_string());
}

namespace {

// Returns the index of the first character representing the hostname in |uri|.
// Returns npos if the start of the hostname is not found.
//
// We should use GURL to do this except that uri could start with ipp:// which
// is not a standard url scheme (according to GURL).
size_t HostnameStart(base::StringPiece uri) {
  size_t scheme_separator_start = uri.find(url::kStandardSchemeSeparator);
  if (scheme_separator_start == base::StringPiece::npos) {
    return base::StringPiece::npos;
  }
  return scheme_separator_start + strlen(url::kStandardSchemeSeparator);
}

base::StringPiece HostAndPort(base::StringPiece uri) {
  size_t hostname_start = HostnameStart(uri);
  if (hostname_start == base::StringPiece::npos) {
    return "";
  }

  size_t hostname_end = uri.find("/", hostname_start);
  if (hostname_end == base::StringPiece::npos) {
    // No trailing slash.  Use end of string.
    hostname_end = uri.size();
  }

  CHECK_GT(hostname_end, hostname_start);
  return uri.substr(hostname_start, hostname_end - hostname_start);
}

}  // namespace

Printer::Printer() : source_(SRC_USER_PREFS) {
  id_ = base::GenerateGUID();
}

Printer::Printer(const std::string& id) : id_(id), source_(SRC_USER_PREFS) {
  if (id_.empty())
    id_ = base::GenerateGUID();
}

Printer::Printer(const Printer& other) = default;

Printer& Printer::operator=(const Printer& other) = default;

Printer::~Printer() = default;

bool Printer::IsIppEverywhere() const {
  return ppd_reference_.autoconf;
}

bool Printer::RequiresIpResolution() const {
  return effective_uri_.empty() &&
         base::StartsWith(id_, "zeroconf-", base::CompareCase::SENSITIVE);
}

net::HostPortPair Printer::GetHostAndPort() const {
  if (uri_.empty()) {
    return net::HostPortPair();
  }

  return net::HostPortPair::FromString(HostAndPort(uri_).as_string());
}

std::string Printer::ReplaceHostAndPort(const net::IPEndPoint& ip) const {
  if (uri_.empty()) {
    return "";
  }

  size_t hostname_start = HostnameStart(uri_);
  if (hostname_start == base::StringPiece::npos) {
    return "";
  }
  size_t host_port_len = HostAndPort(uri_).length();
  return base::JoinString({uri_.substr(0, hostname_start), ip.ToString(),
                           uri_.substr(hostname_start + host_port_len)},
                          "");
}

Printer::PrinterProtocol Printer::GetProtocol() const {
  const base::StringPiece uri(uri_);

  if (uri.starts_with("usb:"))
    return PrinterProtocol::kUsb;

  if (uri.starts_with("ipp:"))
    return PrinterProtocol::kIpp;

  if (uri.starts_with("ipps:"))
    return PrinterProtocol::kIpps;

  if (uri.starts_with("http:"))
    return PrinterProtocol::kHttp;

  if (uri.starts_with("https:"))
    return PrinterProtocol::kHttps;

  if (uri.starts_with("socket:"))
    return PrinterProtocol::kSocket;

  if (uri.starts_with("lpd:"))
    return PrinterProtocol::kLpd;

  return PrinterProtocol::kUnknown;
}

std::string Printer::UriForCups() const {
  if (!effective_uri_.empty()) {
    return effective_uri_;
  } else {
    return uri_;
  }
}

base::Optional<UriComponents> Printer::GetUriComponents() const {
  return chromeos::ParseUri(uri_);
}

bool Printer::PpdReference::operator==(
    const Printer::PpdReference& other) const {
  return user_supplied_ppd_url == other.user_supplied_ppd_url &&
         effective_make_and_model == other.effective_make_and_model &&
         autoconf == other.autoconf;
}

}  // namespace chromeos
