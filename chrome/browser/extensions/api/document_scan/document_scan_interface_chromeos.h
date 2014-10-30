// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_INTERFACE_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_INTERFACE_CHROMEOS_H_

#include "chrome/browser/extensions/api/document_scan/document_scan_interface.h"
#include "chromeos/dbus/lorgnette_manager_client.h"
#include "chromeos/dbus/pipe_reader.h"

namespace extensions {

namespace api {

class DocumentScanInterfaceChromeos : public DocumentScanInterface {
 public:
  DocumentScanInterfaceChromeos();
  virtual ~DocumentScanInterfaceChromeos();

  virtual void ListScanners(
      const ListScannersResultsCallback& callback) override;
  virtual void Scan(const std::string& scanner_name,
                    ScanMode mode,
                    int resolution_dpi,
                    const ScanResultsCallback& callback) override;

 private:
  friend class DocumentScanInterfaceChromeosTest;

  void OnScannerListReceived(
      const ListScannersResultsCallback& callback,
      bool succeeded,
      const chromeos::LorgnetteManagerClient::ScannerTable &scanners);
  void OnScanCompleted(const ScanResultsCallback &callback, bool succeeded);
  void OnScanDataCompleted();
  std::string GetImageURL(std::string image_data);
  chromeos::LorgnetteManagerClient* GetLorgnetteManagerClient();

  scoped_ptr<chromeos::PipeReaderForString> pipe_reader_;
  std::string scanned_image_data_;
  chromeos::LorgnetteManagerClient* lorgnette_manager_client_;

  DISALLOW_COPY_AND_ASSIGN(DocumentScanInterfaceChromeos);
};

}  // namespace api

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_INTERFACE_CHROMEOS_H_
