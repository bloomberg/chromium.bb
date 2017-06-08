// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_INFO_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_INFO_H_

#include <string>

#include "base/callback_forward.h"

namespace chromeos {

// Callback for basic printer information.  |success| indicates if the request
// succeeded at all.  |make| represents the printer manufacturer.  |model| is
// the printer model.  |autoconf| indicates if we think we can compute the
// printer capabilites without a PPD.
using PrinterInfoCallback = base::Callback<void(bool success,
                                                const std::string& make,
                                                const std::string& model,
                                                bool autoconf)>;

// Dispatch an IPP request to |host| on |port| for |path| to obtain
// basic printer information.
void QueryIppPrinter(const std::string& host,
                     const int port,
                     const std::string& path,
                     const PrinterInfoCallback& callback);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_INFO_H_
