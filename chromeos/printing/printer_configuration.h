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
  // Represents a Postscript Printer Description with which the printer was
  // setup.
  struct PPDFile {
    // Identifier from the quirks server. -1 otherwise.
    int id = -1;

    std::string file_name;

    // If there is a file with a later version on the quirks server, that file
    // should be used. The default value is 0.
    int version_number = 0;

    // This will be true if the file was retrived from the quirks server.
    // Otherwise, the file was saved to disk by the user.
    bool from_quirks_server = false;
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

  const PPDFile& ppd() const { return ppd_; }
  void SetPPD(std::unique_ptr<PPDFile> ppd);

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

  // The associated postscript printer description.
  PPDFile ppd_;

  // The UUID from an autoconf protocol for deduplication. Could be empty.
  std::string uuid_;
};

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PRINTER_CONFIGURATION_H_
