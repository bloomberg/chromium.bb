// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_interface_chromeos.h"

#include "base/base64.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/lorgnette_manager_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

const char kImageScanFailedError[] = "Image scan failed";
const char kScannerImageMimeTypePng[] = "image/png";
const char kPngImageDataUrlPrefix[] = "data:image/png;base64,";

}  // namespace

namespace extensions {

namespace api {

DocumentScanInterfaceChromeos::DocumentScanInterfaceChromeos()
    : lorgnette_manager_client_(nullptr) {}

DocumentScanInterfaceChromeos::~DocumentScanInterfaceChromeos() {}

void DocumentScanInterfaceChromeos::ListScanners(
    const ListScannersResultsCallback& callback) {
  GetLorgnetteManagerClient()->ListScanners(base::Bind(
           &DocumentScanInterfaceChromeos::OnScannerListReceived,
           base::Unretained(this),
           callback));
}

void DocumentScanInterfaceChromeos::OnScannerListReceived(
    const ListScannersResultsCallback& callback,
    bool succeeded,
    const chromeos::LorgnetteManagerClient::ScannerTable &scanners) {
  std::vector<ScannerDescription> scanner_descriptions;
  for (chromeos::LorgnetteManagerClient::ScannerTable::const_iterator iter =
           scanners.begin();
       iter != scanners.end();
       ++iter) {
    ScannerDescription description;
    description.name = iter->first;
    const chromeos::LorgnetteManagerClient::ScannerTableEntry &entry =
        iter->second;
    chromeos::LorgnetteManagerClient::ScannerTableEntry::const_iterator info_it;
    info_it = entry.find(lorgnette::kScannerPropertyManufacturer);
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
  callback.Run(scanner_descriptions, "");
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

    default:
      // Leave the mode parameter empty, thereby using the default.
      break;
  }

  if (resolution_dpi != 0) {
    properties.resolution_dpi = resolution_dpi;
  }

  const bool kTasksAreSlow = true;
  scoped_refptr<base::TaskRunner> task_runner =
      base::WorkerPool::GetTaskRunner(kTasksAreSlow);

  pipe_reader_.reset(new chromeos::PipeReaderForString(
      task_runner,
      base::Bind(&DocumentScanInterfaceChromeos::OnScanDataCompleted,
                 base::Unretained(this))));
  base::File file = pipe_reader_->StartIO();
  base::PlatformFile platform_file = file.TakePlatformFile();
  VLOG(1) << "ScanImage platform_file is " << platform_file;
  GetLorgnetteManagerClient()->ScanImage(
      scanner_name, platform_file, properties,
      base::Bind(&DocumentScanInterfaceChromeos::OnScanCompleted,
                 base::Unretained(this),
                 callback));
}

void DocumentScanInterfaceChromeos::OnScanCompleted(
    const ScanResultsCallback &callback, bool succeeded) {
  VLOG(1) << "ScanImage returns " << succeeded;
  if (pipe_reader_.get()) {
    pipe_reader_->OnDataReady(-1);  // terminate data stream
  }

  std::string error_string;
  if (!succeeded) {
    error_string = kImageScanFailedError;
  }

  callback.Run(GetImageURL(scanned_image_data_), kScannerImageMimeTypePng,
                           error_string);
}

std::string DocumentScanInterfaceChromeos::GetImageURL(std::string image_data) {
  std::string image_data_base64;
  base::Base64Encode(image_data, &image_data_base64);
  return std::string(kPngImageDataUrlPrefix) + image_data_base64;
}

void DocumentScanInterfaceChromeos::OnScanDataCompleted() {
  pipe_reader_->GetData(&scanned_image_data_);
  pipe_reader_.reset();
}

chromeos::LorgnetteManagerClient*
    DocumentScanInterfaceChromeos::GetLorgnetteManagerClient() {
  if (!lorgnette_manager_client_) {
    lorgnette_manager_client_ =
        chromeos::DBusThreadManager::Get()->GetLorgnetteManagerClient();
    CHECK(lorgnette_manager_client_);
  }
  return lorgnette_manager_client_;
}

// static
DocumentScanInterface *DocumentScanInterface::CreateInstance() {
  return new DocumentScanInterfaceChromeos();
}

}  // namespace api

}  // namespace extensions
