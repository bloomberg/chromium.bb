// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_PREF_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_PREF_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/printer_translator.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chromeos {

class PrinterPrefManager : public KeyedService {
 public:
  explicit PrinterPrefManager(Profile* profile);

  // Register the printing preferences with the |registry|.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns the printers that are saved in preferences.
  std::vector<std::unique_ptr<Printer>> GetPrinters() const;

  // Adds or updates a printer. Printers are identified by the id field.  Use an
  // empty id to add a new printer.
  void RegisterPrinter(std::unique_ptr<Printer> printer);

  // Remove printer from preferences with the id |printer_id|.  Returns true if
  // the printer was successfully removed.
  bool RemovePrinter(const std::string& printer_id);

 private:
  Profile* profile_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_PREF_MANAGER_H_
