// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CDD_CONVERSION_WIN_H_
#define CHROME_SERVICE_CLOUD_PRINT_CDD_CONVERSION_WIN_H_

#include <string>
#include <windows.h>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"

namespace printing {
struct PrinterSemanticCapsAndDefaults;
}

namespace cloud_print {

bool IsValidCjt(const std::string& print_ticket);

scoped_ptr<DEVMODE[]> CjtToDevMode(const base::string16& printer_name,
                                   const std::string& print_ticket);

std::string CapabilitiesToCdd(
    const printing::PrinterSemanticCapsAndDefaults& semantic_info);

}  // namespace cloud_print

#endif  // CHROME_SERVICE_CLOUD_PRINT_CDD_CONVERSION_WIN_H_
