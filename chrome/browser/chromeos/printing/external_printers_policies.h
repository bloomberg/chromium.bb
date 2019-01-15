// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_EXTERNAL_PRINTERS_POLICIES_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_EXTERNAL_PRINTERS_POLICIES_H_

#include <string>

// A collection of preference names representing the external printer fields.
struct ExternalPrinterPolicies {
  std::string access_mode;
  std::string blacklist;
  std::string whitelist;
};

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_EXTERNAL_PRINTERS_POLICIES_H_
