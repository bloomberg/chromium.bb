// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_PRINTER_CONFIGURATION_H_
#define CHROMEOS_PRINTING_PRINTER_CONFIGURATION_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
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

    // True if the printer should be auto-configured and a PPD is unnecessary.
    bool autoconf = false;

    // Explicitly support equivalence, to detect if a reference has changed.
    bool operator==(const PpdReference& other) const;
  };

  // The location where the printer is stored.
  enum Source {
    SRC_USER_PREFS,
    SRC_POLICY,
  };

  // An enumeration of printer protocols.
  // These values are written to logs.  New enum values can be added, but
  // existing enums must never be renumbered or deleted and reused.
  enum PrinterProtocol {
    kUnknown = 0,
    kUsb = 1,
    kIpp = 2,
    kIpps = 3,
    kHttp = 4,
    kHttps = 5,
    kSocket = 6,
    kLpd = 7,
    kProtocolMax
  };

  // Constructs a printer object that is completely empty.
  Printer();

  // Constructs a printer object with an |id| and a |last_updated| timestamp.
  explicit Printer(const std::string& id, const base::Time& last_updated = {});

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

  // Returns the |manufacturer| of the printer.
  // DEPRECATED(skau@chromium.org): Use make_and_model() instead.
  const std::string& manufacturer() const { return manufacturer_; }
  void set_manufacturer(const std::string& manufacturer) {
    manufacturer_ = manufacturer;
  }

  // Returns the |model| of the printer.
  // DEPRECATED(skau@chromium.org): Use make_and_model() instead.
  const std::string& model() const { return model_; }
  void set_model(const std::string& model) { model_ = model; }

  const std::string& make_and_model() const { return make_and_model_; }
  void set_make_and_model(const std::string& make_and_model) {
    make_and_model_ = make_and_model;
  }

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

  // Returns the printer protocol the printer is configured with.
  Printer::PrinterProtocol GetProtocol() const;

  Source source() const { return source_; }
  void set_source(const Source source) { source_ = source; }

  // Returns the timestamp for the most recent update.  Returns 0 if the
  // printer was not created with a valid timestamp.
  base::Time last_updated() const { return last_updated_; }

 private:
  // Globally unique identifier. Empty indicates a new printer.
  std::string id_;

  // User defined string for printer identification.
  std::string display_name_;

  // User defined string for additional printer information.
  std::string description_;

  // The manufacturer of the printer, e.g. HP
  // DEPRECATED(skau@chromium.org): Migrating to make_and_model.  This is kept
  // for backward compatibility until migration is complete.
  std::string manufacturer_;

  // The model of the printer, e.g. OfficeJet 415
  // DEPRECATED(skau@chromium.org): Migrating to make_and_model.  This is kept
  // for backward compatibility until migration is complete.
  std::string model_;

  // The manufactuer and model of the printer in one string. e.g. HP OfficeJet
  // 415. This is either read from or derived from printer information and is
  // not necessarily suitable for display.
  std::string make_and_model_;

  // The full path for the printer. Suitable for configuration in CUPS.
  // Contains protocol, hostname, port, and queue.
  std::string uri_;

  // How to find the associated postscript printer description.
  PpdReference ppd_reference_;

  // The UUID from an autoconf protocol for deduplication. Could be empty.
  std::string uuid_;

  // The datastore which holds this printer.
  Source source_;

  // Timestamp of most recent change.
  base::Time last_updated_;
};

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PRINTER_CONFIGURATION_H_
