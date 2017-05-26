// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/printer_configuration.h"

#include <string>

#include "base/guid.h"
#include "base/strings/string_piece.h"

namespace chromeos {

Printer::Printer() : source_(SRC_USER_PREFS) {
  id_ = base::GenerateGUID();
}

Printer::Printer(const std::string& id, const base::Time& last_updated)
    : id_(id), source_(SRC_USER_PREFS), last_updated_(last_updated) {
  if (id_.empty())
    id_ = base::GenerateGUID();
}

Printer::Printer(const Printer& other) = default;

Printer& Printer::operator=(const Printer& other) = default;

Printer::~Printer() {}

bool Printer::IsIppEverywhere() const {
  return ppd_reference_.autoconf;
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

}  // namespace chromeos
