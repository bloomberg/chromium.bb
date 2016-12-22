// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_PRINTER_TRANSLATOR_H_
#define CHROMEOS_PRINTING_PRINTER_TRANSLATOR_H_

#include <memory>

#include "chromeos/chromeos_export.h"
#include "chromeos/printing/printer_configuration.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {
namespace printing {

CHROMEOS_EXPORT extern const char kPrinterId[];

// Returns a new Printer populated with fields from |pref|.  Returns a nullptr
// if |pref| does not contain an id field.
CHROMEOS_EXPORT std::unique_ptr<Printer> PrefToPrinter(
    const base::DictionaryValue& pref);

// Returns a dictionary generated from the |printer| fields.
CHROMEOS_EXPORT std::unique_ptr<base::DictionaryValue> PrinterToPref(
    const Printer& printer);

// Returns a new printer populated with the fields from |pref|.  Processes
// dictionaries from policy i.e. cPanel.
CHROMEOS_EXPORT std::unique_ptr<Printer> RecommendedPrinterToPrinter(
    const base::DictionaryValue& pref);

}  // namespace printing
}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PRINTER_TRANSLATOR_H_
