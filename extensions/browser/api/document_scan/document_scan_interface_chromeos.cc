// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/document_scan/document_scan_interface_chromeos.h"

#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/lorgnette_manager_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

const char kImageScanFailedError[] = "Image scan failed";
const char kScannerImageMimeTypePng[] = "image/png";
const char kPngImageDataUrlPrefix[] = "data:image/png;base64,";

chromeos::LorgnetteManagerClient* GetLorgnetteManagerClient() {
  DCHECK(chromeos::DBusThreadManager::IsInitialized());
  return chromeos::DBusThreadManager::Get()->GetLorgnetteManagerClient();
}

}  // namespace

namespace extensions {

namespace api {

DocumentScanInterfaceChromeos::DocumentScanInterfaceChromeos() = default;

DocumentScanInterfaceChromeos::~DocumentScanInterfaceChromeos() = default;

void DocumentScanInterfaceChromeos::ListScanners(
    const ListScannersResultsCallback& callback) {
  GetLorgnetteManagerClient()->ListScanners(
      base::Bind(&DocumentScanInterfaceChromeos::OnScannerListReceived,
                 base::Unretained(this), callback));
}

void DocumentScanInterfaceChromeos::OnScannerListReceived(
    const ListScannersResultsCallback& callback,
    bool succeeded,
    const chromeos::LorgnetteManagerClient::ScannerTable& scanners) {
  std::vector<ScannerDescription> scanner_descriptions;
  for (const auto& scanner : scanners) {
    ScannerDescription description;
    description.name = scanner.first;
    const auto& entry = scanner.second;
    auto info_it = entry.find(lorgnette::kScannerPropertyManufacturer);
    if (info_it != entry.end()) {
      description.manufacturer = info_it->second;
    }
    info_it = entry.find(lorgnette::kScannerPropertyModel);
    if (info_it != entry.end()) {
      description.model = info_it->second;
    }
    info_it = entry.find(lorgnette::kScannerPropertyType);
    if (info_it != entry.end()) {
      description.scanner_type = info_it->second;
    }
    description.image_mime_type = kScannerImageMimeTypePng;
    scanner_descriptions.push_back(description);
  }
  const std::string kNoError;
  callback.Run(scanner_descriptions, kNoError);
}

void DocumentScanInterfaceChromeos::Scan(const std::string& scanner_name,
                                         ScanMode mode,
                                         int resolution_dpi,
                                         const ScanResultsCallback& callback) {
  VLOG(1) << "Choosing scanner " << scanner_name;
  chromeos::LorgnetteManagerClient::ScanProperties properties;
  switch (mode) {
    case kScanModeColor:
      properties.mode = lorgnette::kScanPropertyModeColor;
      break;

    case kScanModeGray:
      properties.mode = lorgnette::kScanPropertyModeGray;
      break;

    case kScanModeLineart:
      properties.mode = lorgnette::kScanPropertyModeLineart;
      break;
  }

  if (resolution_dpi != 0) {
    properties.resolution_dpi = resolution_dpi;
  }

  GetLorgnetteManagerClient()->ScanImageToString(
      scanner_name, properties,
      base::Bind(&DocumentScanInterfaceChromeos::OnScanCompleted,
                 base::Unretained(this), callback));
}

void DocumentScanInterfaceChromeos::OnScanCompleted(
    const ScanResultsCallback& callback,
    bool succeeded,
    const std::string& image_data) {
  VLOG(1) << "ScanImage returns " << succeeded;
  std::string error_string;
  if (!succeeded) {
    error_string = kImageScanFailedError;
  }

  std::string image_base64;
  base::Base64Encode(image_data, &image_base64);
  std::string image_url(std::string(kPngImageDataUrlPrefix) + image_base64);

  callback.Run(image_url, kScannerImageMimeTypePng, error_string);
}

// static
DocumentScanInterface* DocumentScanInterface::CreateInstance() {
  return new DocumentScanInterfaceChromeos();
}

}  // namespace api

}  // namespace extensions
