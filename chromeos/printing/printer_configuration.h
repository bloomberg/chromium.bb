// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_PRINTER_CONFIGURATION_H_
#define CHROMEOS_PRINTING_PRINTER_CONFIGURATION_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

class CHROMEOS_EXPORT Printer {
 public:
  // Information needed to find the PPD file for this printer.
  //
  // If you add fields to this struct, you almost certainly will
  // want to update PpdResolver and PpdCache::GetCachePath.
  //
  // At resolution time, we look for a cached PPD that used the same
  // PpdReference before.
  //
  // If one is not found and user_supplied_ppd_url is set, we'll fail
  // out with NOT FOUND
  //
  // Otherwise, we'll hit QuirksServer to see if we can resolve a Ppd
  // using manufacturer/model
  struct PpdReference {
    // If non-empty, this is the url of a specific PPD the user has specified
    // for use with this printer.
    std::string user_supplied_ppd_url;

    // The *effective* manufacturer and model of this printer, in other words,
    // the manufacturer and model of the printer for which we grab a PPD.  This
    // doesn’t have to (but can) be the actual manufacturer/model of the
    // printer.  We should always try to fill these fields in, even if we don’t
    // think they’ll be needed, as they provide a fallback mechanism for
    // finding a PPD.
    std::string effective_manufacturer;
    std::string effective_model;
  };

  // Constructs a printer object that is completely empty.
  Printer();

  // Constructs a printer object with an id.
  explicit Printer(const std::string& id);

  // Copy constructor and assignment.
  explicit Printer(const Printer& printer);
  Printer& operator=(const Printer& printer);

  ~Printer();

  const std::string& id() const { return id_; }
  void set_id(const std::string& id) { id_ = id; }

  const std::string& display_name() const { return display_name_; }
  void set_display_name(const std::string& display_name) {
    display_name_ = display_name;
  }

  const std::string& description() const { return description_; }
  void set_description(const std::string& description) {
    description_ = description;
  }

  const std::string& manufacturer() const { return manufacturer_; }
  void set_manufacturer(const std::string& manufacturer) {
    manufacturer_ = manufacturer;
  }

  const std::string& model() const { return model_; }
  void set_model(const std::string& model) { model_ = model; }

  const std::string& uri() const { return uri_; }
  void set_uri(const std::string& uri) { uri_ = uri; }

  const PpdReference& ppd_reference() const { return ppd_reference_; }
  PpdReference* mutable_ppd_reference() { return &ppd_reference_; }

  const std::string& uuid() const { return uuid_; }
  void set_uuid(const std::string& uuid) { uuid_ = uuid; }

 private:
  // Globally unique identifier. Empty indicates a new printer.
  std::string id_;

  // User defined string for printer identification.
  std::string display_name_;

  // User defined string for additional printer information.
  std::string description_;

  // The manufacturer of the printer, e.g. HP
  std::string manufacturer_;

  // The model of the printer, e.g. OfficeJet 415
  std::string model_;

  // The full path for the printer. Suitable for configuration in CUPS.
  // Contains protocol, hostname, port, and queue.
  std::string uri_;

  // How to find the associated postscript printer description.
  PpdReference ppd_reference_;

  // The UUID from an autoconf protocol for deduplication. Could be empty.
  std::string uuid_;
};

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PRINTER_CONFIGURATION_H_
