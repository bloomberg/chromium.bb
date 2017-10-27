// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/extension_printer_handler.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_split.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/printing/pwg_raster_converter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/print_preview/printer_capabilities.h"
#include "components/cloud_devices/common/cloud_device_description.h"
#include "components/cloud_devices/common/printer_description.h"
#include "device/base/device_client.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_service.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/browser/api/printer_provider/printer_provider_api.h"
#include "extensions/browser/api/printer_provider/printer_provider_api_factory.h"
#include "extensions/browser/api/printer_provider/printer_provider_print_job.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/printer_provider/usb_printer_manifest_data.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/usb_device_permission.h"
#include "extensions/common/permissions/usb_device_permission_data.h"
#include "extensions/common/value_builder.h"
#include "printing/print_job_constants.h"
#include "printing/pwg_raster_settings.h"

using device::UsbDevice;
using extensions::DevicePermissionsManager;
using extensions::DictionaryBuilder;
using extensions::Extension;
using extensions::ExtensionRegistry;
using extensions::ListBuilder;
using extensions::UsbPrinterManifestData;
using printing::PWGRasterConverter;

namespace {

const char kContentTypePdf[] = "application/pdf";
const char kContentTypePWGRaster[] = "image/pwg-raster";
const char kContentTypeAll[] = "*/*";

const char kInvalidDataPrintError[] = "INVALID_DATA";
const char kInvalidTicketPrintError[] = "INVALID_TICKET";

const char kProvisionalUsbLabel[] = "provisional-usb";

// Updates |job| with raster file path, size and last modification time.
// Returns the updated print job.
std::unique_ptr<extensions::PrinterProviderPrintJob>
UpdateJobFileInfoOnWorkerThread(
    const base::FilePath& raster_path,
    std::unique_ptr<extensions::PrinterProviderPrintJob> job) {
  if (base::GetFileInfo(raster_path, &job->file_info))
    job->document_path = raster_path;
  return job;
}

// Callback to PWG raster conversion.
// Posts a task to update print job with info about file containing converted
// PWG raster data.
void UpdateJobFileInfo(std::unique_ptr<extensions::PrinterProviderPrintJob> job,
                       ExtensionPrinterHandler::PrintJobCallback callback,
                       bool success,
                       const base::FilePath& pwg_file_path) {
  if (!success) {
    std::move(callback).Run(std::move(job));
    return;
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&UpdateJobFileInfoOnWorkerThread, pwg_file_path,
                     std::move(job)),
      std::move(callback));
}

bool HasUsbPrinterProviderPermissions(const Extension* extension) {
  return extension->permissions_data() &&
        extension->permissions_data()->HasAPIPermission(
            extensions::APIPermission::kPrinterProvider) &&
        extension->permissions_data()->HasAPIPermission(
            extensions::APIPermission::kUsb);
}

std::string GenerateProvisionalUsbPrinterId(const Extension* extension,
                                            const UsbDevice* device) {
  return base::StringPrintf("%s:%s:%s", kProvisionalUsbLabel,
                            extension->id().c_str(), device->guid().c_str());
}

bool ParseProvisionalUsbPrinterId(const std::string& printer_id,
                                  std::string* extension_id,
                                  std::string* device_guid) {
  std::vector<std::string> components = base::SplitString(
      printer_id, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (components.size() != 3)
    return false;

  if (components[0] != kProvisionalUsbLabel)
    return false;

  *extension_id = components[1];
  *device_guid = components[2];
  return true;
}

}  // namespace

ExtensionPrinterHandler::ExtensionPrinterHandler(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {}

ExtensionPrinterHandler::~ExtensionPrinterHandler() {
}

void ExtensionPrinterHandler::Reset() {
  // TODO(tbarzic): Keep track of pending request ids issued by |this| and
  // cancel them from here.
  pending_enumeration_count_ = 0;
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void ExtensionPrinterHandler::StartGetPrinters(
    const AddedPrintersCallback& callback,
    GetPrintersDoneCallback done_callback) {
  // Assume that there can only be one printer enumeration occuring at once.
  DCHECK_EQ(pending_enumeration_count_, 0);
  pending_enumeration_count_ = 1;
  done_callback_ = std::move(done_callback);

  bool extension_supports_usb_printers = false;
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  for (const auto& extension : registry->enabled_extensions()) {
    if (UsbPrinterManifestData::Get(extension.get()) &&
        HasUsbPrinterProviderPermissions(extension.get())) {
      extension_supports_usb_printers = true;
      break;
    }
  }

  if (extension_supports_usb_printers) {
    device::UsbService* service = device::DeviceClient::Get()->GetUsbService();
    pending_enumeration_count_++;
    service->GetDevices(
        base::Bind(&ExtensionPrinterHandler::OnUsbDevicesEnumerated,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  extensions::PrinterProviderAPIFactory::GetInstance()
      ->GetForBrowserContext(profile_)
      ->DispatchGetPrintersRequested(
          base::Bind(&ExtensionPrinterHandler::WrapGetPrintersCallback,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void ExtensionPrinterHandler::StartGetCapability(
    const std::string& destination_id,
    GetCapabilityCallback callback) {
  extensions::PrinterProviderAPIFactory::GetInstance()
      ->GetForBrowserContext(profile_)
      ->DispatchGetCapabilityRequested(
          destination_id,
          base::BindOnce(&ExtensionPrinterHandler::WrapGetCapabilityCallback,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ExtensionPrinterHandler::StartPrint(
    const std::string& destination_id,
    const std::string& capability,
    const base::string16& job_title,
    const std::string& ticket_json,
    const gfx::Size& page_size,
    const scoped_refptr<base::RefCountedBytes>& print_data,
    PrintCallback callback) {
  auto print_job = base::MakeUnique<extensions::PrinterProviderPrintJob>();
  print_job->printer_id = destination_id;
  print_job->job_title = job_title;
  print_job->ticket_json = ticket_json;

  cloud_devices::CloudDeviceDescription printer_description;
  printer_description.InitFromString(capability);

  cloud_devices::printer::ContentTypesCapability content_types;
  content_types.LoadFrom(printer_description);

  const bool kUsePdf = content_types.Contains(kContentTypePdf) ||
                       content_types.Contains(kContentTypeAll);

  if (kUsePdf) {
    // TODO(tbarzic): Consider writing larger PDF to disk and provide the data
    // the same way as it's done with PWG raster.
    print_job->content_type = kContentTypePdf;
    print_job->document_bytes = print_data;
    DispatchPrintJob(std::move(callback), std::move(print_job));
    return;
  }

  cloud_devices::CloudDeviceDescription ticket;
  if (!ticket.InitFromString(ticket_json)) {
    WrapPrintCallback(std::move(callback),
                      base::Value(kInvalidTicketPrintError));
    return;
  }

  print_job->content_type = kContentTypePWGRaster;
  ConvertToPWGRaster(
      print_data, printer_description, ticket, page_size, std::move(print_job),
      base::BindOnce(&ExtensionPrinterHandler::DispatchPrintJob,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ExtensionPrinterHandler::StartGrantPrinterAccess(
    const std::string& printer_id,
    GetPrinterInfoCallback callback) {
  std::string extension_id;
  std::string device_guid;
  if (!ParseProvisionalUsbPrinterId(printer_id, &extension_id, &device_guid)) {
    std::move(callback).Run(base::DictionaryValue());
    return;
  }

  device::UsbService* service = device::DeviceClient::Get()->GetUsbService();
  scoped_refptr<UsbDevice> device = service->GetDevice(device_guid);
  if (!device) {
    std::move(callback).Run(base::DictionaryValue());
    return;
  }

  DevicePermissionsManager* permissions_manager =
      DevicePermissionsManager::Get(profile_);
  permissions_manager->AllowUsbDevice(extension_id, device);

  extensions::PrinterProviderAPIFactory::GetInstance()
      ->GetForBrowserContext(profile_)
      ->DispatchGetUsbPrinterInfoRequested(
          extension_id, device,
          base::BindOnce(&ExtensionPrinterHandler::WrapGetPrinterInfoCallback,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ExtensionPrinterHandler::SetPWGRasterConverterForTesting(
    std::unique_ptr<PWGRasterConverter> pwg_raster_converter) {
  pwg_raster_converter_ = std::move(pwg_raster_converter);
}

void ExtensionPrinterHandler::ConvertToPWGRaster(
    const scoped_refptr<base::RefCountedMemory>& data,
    const cloud_devices::CloudDeviceDescription& printer_description,
    const cloud_devices::CloudDeviceDescription& ticket,
    const gfx::Size& page_size,
    std::unique_ptr<extensions::PrinterProviderPrintJob> job,
    PrintJobCallback callback) {
  if (!pwg_raster_converter_) {
    pwg_raster_converter_ = PWGRasterConverter::CreateDefault();
  }
  pwg_raster_converter_->Start(
      data.get(),
      PWGRasterConverter::GetConversionSettings(printer_description, page_size),
      PWGRasterConverter::GetBitmapSettings(printer_description, ticket),
      base::BindOnce(&UpdateJobFileInfo, std::move(job), std::move(callback)));
}

void ExtensionPrinterHandler::DispatchPrintJob(
    PrintCallback callback,
    std::unique_ptr<extensions::PrinterProviderPrintJob> print_job) {
  if (print_job->document_path.empty() && !print_job->document_bytes) {
    WrapPrintCallback(std::move(callback), base::Value(kInvalidDataPrintError));
    return;
  }

  extensions::PrinterProviderAPIFactory::GetInstance()
      ->GetForBrowserContext(profile_)
      ->DispatchPrintRequested(
          *print_job,
          base::BindOnce(&ExtensionPrinterHandler::WrapPrintCallback,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ExtensionPrinterHandler::WrapGetPrintersCallback(
    const AddedPrintersCallback& callback,
    const base::ListValue& printers,
    bool done) {
  DCHECK_GT(pending_enumeration_count_, 0);
  if (!printers.empty())
    callback.Run(printers);

  if (done)
    pending_enumeration_count_--;
  if (pending_enumeration_count_ == 0)
    std::move(done_callback_).Run();
}

void ExtensionPrinterHandler::WrapGetCapabilityCallback(
    GetCapabilityCallback callback,
    const base::DictionaryValue& capability) {
  auto capabilities = std::make_unique<base::DictionaryValue>();
  std::unique_ptr<base::DictionaryValue> cdd =
      printing::ValidateCddForPrintPreview(capability);
  // Leave |capabilities| empty if |cdd| is empty.
  if (!cdd->empty()) {
    capabilities->SetKey(printing::kSettingCapabilities,
                         base::Value::FromUniquePtrValue(std::move(cdd)));
  }
  std::move(callback).Run(std::move(capabilities));
}

void ExtensionPrinterHandler::WrapPrintCallback(PrintCallback callback,
                                                const base::Value& status) {
  std::move(callback).Run(status);
}

void ExtensionPrinterHandler::WrapGetPrinterInfoCallback(
    GetPrinterInfoCallback callback,
    const base::DictionaryValue& printer_info) {
  std::move(callback).Run(printer_info);
}

void ExtensionPrinterHandler::OnUsbDevicesEnumerated(
    const AddedPrintersCallback& callback,
    const std::vector<scoped_refptr<UsbDevice>>& devices) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  DevicePermissionsManager* permissions_manager =
      DevicePermissionsManager::Get(profile_);

  ListBuilder printer_list;

  for (const auto& extension : registry->enabled_extensions()) {
    const UsbPrinterManifestData* manifest_data =
        UsbPrinterManifestData::Get(extension.get());
    if (!manifest_data || !HasUsbPrinterProviderPermissions(extension.get()))
      continue;

    const extensions::DevicePermissions* device_permissions =
        permissions_manager->GetForExtension(extension->id());
    for (const auto& device : devices) {
      if (manifest_data->SupportsDevice(*device)) {
        std::unique_ptr<extensions::UsbDevicePermission::CheckParam> param =
            extensions::UsbDevicePermission::CheckParam::ForUsbDevice(
                extension.get(), device.get());
        if (device_permissions->FindUsbDeviceEntry(device) ||
            extension->permissions_data()->CheckAPIPermissionWithParam(
                extensions::APIPermission::kUsbDevice, param.get())) {
          // Skip devices the extension already has permission to access.
          continue;
        }

        printer_list.Append(
            DictionaryBuilder()
                .Set("id", GenerateProvisionalUsbPrinterId(extension.get(),
                                                           device.get()))
                .Set("name",
                     DevicePermissionsManager::GetPermissionMessage(
                         device->vendor_id(), device->product_id(),
                         device->manufacturer_string(),
                         device->product_string(), base::string16(), false))
                .Set("extensionId", extension->id())
                .Set("extensionName", extension->name())
                .Set("provisional", true)
                .Build());
      }
    }
  }

  DCHECK_GT(pending_enumeration_count_, 0);
  pending_enumeration_count_--;
  std::unique_ptr<base::ListValue> list = printer_list.Build();
  if (!list->empty())
    callback.Run(*list);
  if (pending_enumeration_count_ == 0)
    std::move(done_callback_).Run();
}
