// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/printer_provider/usb_printer_manifest_handler.h"

#include "extensions/common/extension.h"

namespace extensions {

UsbPrinterManifestHandler::UsbPrinterManifestHandler() {
}

UsbPrinterManifestHandler::~UsbPrinterManifestHandler() {
}

bool UsbPrinterManifestHandler::Parse(Extension* extension,
                                      base::string16* error) {
  return false;
}

const std::vector<std::string> UsbPrinterManifestHandler::Keys() const {
  return std::vector<std::string>();
}

}  // namespace extensions
