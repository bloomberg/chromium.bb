// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINTERS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINTERS_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/printing/printers_sync_bridge.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/printer_translator.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chromeos {

// Manages printer information.  Provides an interface to a user's printers and
// printers provided by policy.  User printers are backed by the
// PrintersSyncBridge.
class PrintersManager : public KeyedService {
 public:
  class Observer {
   public:
    virtual void OnPrinterAdded(const Printer& printer) = 0;
    virtual void OnPrinterUpdated(const Printer& printer) = 0;
    virtual void OnPrinterRemoved(const Printer& printer) = 0;
  };

  PrintersManager(Profile* profile,
                  std::unique_ptr<PrintersSyncBridge> sync_bridge);
  ~PrintersManager() override;

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

  // Attach |observer| for notification of events.  |observer| is expected to
  // live on the same thread (UI) as this object.  OnPrinter* methods are
  // invoked inline so calling RegisterPrinter in response to OnPrinterAdded is
  // forbidden.
  void AddObserver(PrintersManager::Observer* observer);

  // Remove |observer| so that it no longer receives notifications.  After the
  // completion of this method, the |observer| can be safely destroyed.
  void RemoveObserver(PrintersManager::Observer* observer);

  // Returns a ModelTypeSyncBridge for the sync client.
  PrintersSyncBridge* GetSyncBridge();

 private:
  // Updates the in-memory recommended printer list.
  void UpdateRecommendedPrinters();

  Profile* profile_;
  PrefChangeRegistrar pref_change_registrar_;

  // The backend for profile printers.
  std::unique_ptr<PrintersSyncBridge> sync_bridge_;

  // Contains the keys for all recommended printers in order so we can return
  // the list of recommended printers in the order they were received.
  std::vector<std::string> recommended_printer_ids_;
  std::map<std::string, std::unique_ptr<base::DictionaryValue>>
      recommended_printers_;

  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(PrintersManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINTERS_MANAGER_H_
