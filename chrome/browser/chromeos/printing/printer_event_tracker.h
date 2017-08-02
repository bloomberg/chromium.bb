// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_EVENT_TRACKER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_EVENT_TRACKER_H_

#include "base/macros.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/metrics/proto/printer_event.pb.h"

namespace chromeos {

struct UsbPrinter {
  UsbPrinter();
  ~UsbPrinter();

  // USB MFG string
  std::string manufacturer;
  // USB MDL string
  std::string model;
  // IEEE1284 Vendor ID
  int32_t vendor_id;
  // IEEE1284 Product ID
  int32_t model_id;
  // Printer configuration
  Printer printer;
};

// Aggregates printer events for logging.
class PrinterEventTracker : public KeyedService {
 public:
  enum SetupMode {
    // Device configured by the user.
    kUser,
    // Configuration detected by the system.
    kAutomatic
  };

  PrinterEventTracker();
  ~PrinterEventTracker() override;

  // If |logging| is true, logging is enabled. If |logging| is false, logging is
  // disabled and the Record* functions are nops.
  void set_logging(bool logging);

  // Store a succesful USB printer installation. |mode| indicates if
  // the PPD was selected automatically or chosen by the user.
  void RecordUsbPrinterInstalled(const UsbPrinter& printer, SetupMode mode);

  // Store a succesful network printer installation. |mode| indicates if
  // the PPD was selected automatically or chosen by the user.
  void RecordIppPrinterInstalled(const Printer& printer, SetupMode mode);

  // Record an abandoned setup.
  void RecordSetupAbandoned(const Printer& printer);

  // Record an abandoned setup for a USB printer.
  void RecordUsbSetupAbandoned(const UsbPrinter& printer);

  // Store a printer removal.
  void RecordPrinterRemoved(const Printer& printer);

  // Writes stored events to |events|.
  void FlushPrinterEvents(std::vector<metrics::PrinterEventProto>* events);

 private:
  // Records logs if true.  Discards logs if false.
  bool logging_;
  std::vector<metrics::PrinterEventProto> events_;

  DISALLOW_COPY_AND_ASSIGN(PrinterEventTracker);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_EVENT_TRACKER_H_
