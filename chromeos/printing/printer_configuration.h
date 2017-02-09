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
  // Exactly one of the fields below should be filled in.
  //
  // At resolution time, we look for a cached PPD that used the same
  // PpdReference before.
  //
  struct PpdReference {
    // If non-empty, this is the url of a specific PPD the user has specified
    // for use with this printer.  The ppd can be gzipped or uncompressed.  This
    // url must use a file:// scheme.
    std::string user_supplied_ppd_url;

    // String that identifies which ppd to use from the ppd server.
    // Where possible, this is the same as the ipp/ldap
    // printer-make-and-model field.
    std::string effective_make_and_model;
  };

  // The location where the printer is stored.
  enum Source {
    SRC_USER_PREFS,
    SRC_POLICY,
  };

  // Constructs a printer object that is completely empty.
  Printer();

  // Constructs a printer object with an id.
  explicit Printer(const std::string& id);

  // Copy constructor and assignment.
  Printer(const Printer& printer);
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

  // Returns true if the printer should be automatically configured using
  // IPP Everywhere.  Computed using information from |ppd_reference_| and
  // |uri_|.
  bool IsIppEverywhere() const;

  Source source() const { return source_; }
  void set_source(const Source source) { source_ = source; }

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

  // The datastore which holds this printer.
  Source source_;
};

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PRINTER_CONFIGURATION_H_
