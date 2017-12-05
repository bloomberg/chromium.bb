// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_EXTERNAL_PRINTERS_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_EXTERNAL_PRINTERS_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_policy_observer.h"
#include "chromeos/printing/printer_configuration.h"

namespace chromeos {

// Manages download and parsing of the external policy printer configuration and
// enforces restrictions.
class ExternalPrinters {
 public:
  // Choose the policy for printer access.
  enum AccessMode {
    UNSET,
    // Printers in the blacklist are disallowed.  Others are allowed.
    BLACKLIST_ONLY,
    // Only printers in the whitelist are allowed.
    WHITELIST_ONLY,
    // All printers in the policy are allowed.
    ALL_ACCESS
  };

  // Observer is notified when the computed set of printers change.  It is
  // assumed that the observer is on the same sequence as the object it is
  // observing.
  class Observer {
   public:
    // Called when the computed set of printers changes.
    virtual void OnPrintersChanged() = 0;
  };

  // Creates a handler for the external printer policies.
  static std::unique_ptr<ExternalPrinters> Create();

  virtual ~ExternalPrinters() = default;

  // Parses |data| which is the contents of the bulk printes file and extracts
  // printer information.  The file format is assumed to be JSON.
  virtual void SetData(std::unique_ptr<std::string> data) = 0;
  // Removes all printer data and invalidates the configuration.
  virtual void ClearData() = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual void SetAccessMode(AccessMode mode) = 0;
  virtual void SetBlacklist(const std::vector<std::string>& blacklist) = 0;
  virtual void SetWhitelist(const std::vector<std::string>& whitelist) = 0;

  // Returns true if the printer configuration has been downloaded and parsed.
  virtual bool IsPolicySet() const = 0;

  // Returns all the printers available from the policy.
  virtual const std::vector<Printer>& GetPrinters() const = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_EXTERNAL_PRINTERS_H_
