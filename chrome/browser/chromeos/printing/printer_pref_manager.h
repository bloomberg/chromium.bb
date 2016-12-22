// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_PREF_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_PREF_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/printer_translator.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chromeos {

class PrinterPrefManager : public KeyedService {
 public:
  explicit PrinterPrefManager(Profile* profile);
  ~PrinterPrefManager() override;

  // Register the printing preferences with the |registry|.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns the printers that are saved in preferences.
  std::vector<std::unique_ptr<Printer>> GetPrinters() const;

  // Returns printers from enterprise policy.
  std::vector<std::unique_ptr<Printer>> GetRecommendedPrinters() const;

  // Returns the printer with id |printer_id|.
  std::unique_ptr<Printer> GetPrinter(const std::string& printer_id) const;

  // Adds or updates a printer. Printers are identified by the id field.  Use an
  // empty id to add a new printer.
  void RegisterPrinter(std::unique_ptr<Printer> printer);

  // Remove printer from preferences with the id |printer_id|.  Returns true if
  // the printer was successfully removed.
  bool RemovePrinter(const std::string& printer_id);

 private:
  // Updates the in-memory recommended printer list.
  void UpdateRecommendedPrinters();

  Profile* profile_;
  PrefChangeRegistrar pref_change_registrar_;

  // Contains the keys for all recommended printers in order so we can return
  // the list of recommended printers in the order they were received.
  std::vector<std::string> recommended_printer_ids_;
  std::map<std::string, std::unique_ptr<base::DictionaryValue>>
      recommended_printers_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_PREF_MANAGER_H_
