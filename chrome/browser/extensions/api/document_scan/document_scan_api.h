// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/document_scan/document_scan_interface.h"
#include "chrome/common/extensions/api/document_scan.h"
#include "extensions/browser/api/async_api_function.h"

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_FUNCTION_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_FUNCTION_H_

namespace extensions {

namespace api {

class DocumentScanScanFunction : public AsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("documentScan.scan",
                             DOCUMENT_SCAN_SCAN)
  DocumentScanScanFunction();

 protected:
  ~DocumentScanScanFunction();

  // AsyncApiFunction:
  virtual bool Prepare() override;
  virtual void AsyncWorkStart() override;
  virtual bool Respond() override;

 private:
  friend class DocumentScanScanFunctionTest;

  void OnScannerListReceived(
      const std::vector<DocumentScanInterface::ScannerDescription>&
          scanner_descriptions,
      const std::string& error);
  void OnResultsReceived(const std::string& scanned_image,
                         const std::string& mime_type,
                         const std::string& error);

  scoped_ptr<document_scan::Scan::Params> params_;
  scoped_ptr<DocumentScanInterface> document_scan_interface_;

  DISALLOW_COPY_AND_ASSIGN(DocumentScanScanFunction);
};

}  // namespace api

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_FUNCTION_H_
