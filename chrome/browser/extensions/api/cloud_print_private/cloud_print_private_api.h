// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CLOUD_PRINT_PRIVATE_CLOUD_PRINT_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_CLOUD_PRINT_PRIVATE_CLOUD_PRINT_PRIVATE_API_H_

#include <string>
#include <vector>

#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

// For use only in tests.
class CloudPrintTestsDelegate {
 public:
  CloudPrintTestsDelegate();
  virtual ~CloudPrintTestsDelegate();

  virtual void SetupConnector(
      const std::string& user_email,
      const std::string& robot_email,
      const std::string& credentials,
      bool connect_new_printers,
      const std::vector<std::string>& printer_blacklist) = 0;

  virtual std::string GetHostName() = 0;

  virtual std::vector<std::string> GetPrinters() = 0;

  static CloudPrintTestsDelegate* instance();

 private:
  // Points to single instance of this class during testing.
  static CloudPrintTestsDelegate* instance_;
};

class CloudPrintSetupConnectorFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("cloudPrintPrivate.setupConnector");

  CloudPrintSetupConnectorFunction();

 protected:
  virtual ~CloudPrintSetupConnectorFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class CloudPrintGetHostNameFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("cloudPrintPrivate.getHostName");

  CloudPrintGetHostNameFunction();

 protected:
  virtual ~CloudPrintGetHostNameFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class CloudPrintGetPrintersFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("cloudPrintPrivate.getPrinters");

  CloudPrintGetPrintersFunction();

 protected:
  virtual ~CloudPrintGetPrintersFunction();

  void CollectPrinters();
  void ReturnResult(const base::ListValue* printers);

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CLOUD_PRINT_PRIVATE_CLOUD_PRINT_PRIVATE_API_H_
