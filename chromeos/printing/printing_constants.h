// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_PRINTING_CONSTANTS_H_
#define CHROMEOS_PRINTING_PRINTING_CONSTANTS_H_

namespace chromeos {

// Maximum size of a PPD file that we will accept, currently 250k.  This number
// is relatively
// arbitrary, we just don't want to try to handle ridiculously huge files.
constexpr size_t kMaxPpdSizeBytes = 250 * 1024;

// Printing protocol schemes.
constexpr char kIppScheme[] = "ipp";
constexpr char kIppsScheme[] = "ipps";
constexpr char kUsbScheme[] = "usb";
constexpr char kHttpScheme[] = "http";
constexpr char kHttpsScheme[] = "https";
constexpr char kSocketScheme[] = "socket";
constexpr char kLpdScheme[] = "lpd";

constexpr int kIppPort = 631;
// IPPS commonly uses the HTTPS port despite the spec saying it should use the
// IPP port.
constexpr int kIppsPort = 443;

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PRINTING_CONSTANTS_H_
