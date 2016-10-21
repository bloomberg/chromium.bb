// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cros_disks_client.h"

#include <stddef.h>
#include <stdint.h>

#include <map>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "base/values.h"
#include "chromeos/dbus/fake_cros_disks_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char* kDefaultMountOptions[] = {
    "nodev", "noexec", "nosuid",
};
const char kReadOnlyOption[] = "ro";
const char kReadWriteOption[] = "rw";
const char kRemountOption[] = "remount";
const char kMountLabelOption[] = "mountlabel";

const char* kDefaultUnmountOptions[] = {
  "force",
};

const char kLazyUnmountOption[] = "lazy";

// Checks if retrieved media type is in boundaries of DeviceMediaType.
bool IsValidMediaType(uint32_t type) {
  return type < static_cast<uint32_t>(cros_disks::DEVICE_MEDIA_NUM_VALUES);
}

// Translates enum used in cros-disks to enum used in Chrome.
// Note that we could just do static_cast, but this is less sensitive to
// changes in cros-disks.
DeviceType DeviceMediaTypeToDeviceType(uint32_t media_type_uint32) {
  if (!IsValidMediaType(media_type_uint32))
    return DEVICE_TYPE_UNKNOWN;

  cros_disks::DeviceMediaType media_type =
      cros_disks::DeviceMediaType(media_type_uint32);

  switch (media_type) {
    case(cros_disks::DEVICE_MEDIA_UNKNOWN):
      return DEVICE_TYPE_UNKNOWN;
    case(cros_disks::DEVICE_MEDIA_USB):
      return DEVICE_TYPE_USB;
    case(cros_disks::DEVICE_MEDIA_SD):
      return DEVICE_TYPE_SD;
    case(cros_disks::DEVICE_MEDIA_OPTICAL_DISC):
      return DEVICE_TYPE_OPTICAL_DISC;
    case(cros_disks::DEVICE_MEDIA_MOBILE):
      return DEVICE_TYPE_MOBILE;
    case(cros_disks::DEVICE_MEDIA_DVD):
      return DEVICE_TYPE_DVD;
    default:
      return DEVICE_TYPE_UNKNOWN;
  }
}

bool ReadMountEntryFromDbus(dbus::MessageReader* reader, MountEntry* entry) {
  uint32_t error_code = 0;
  std::string source_path;
  uint32_t mount_type = 0;
  std::string mount_path;
  if (!reader->PopUint32(&error_code) ||
      !reader->PopString(&source_path) ||
      !reader->PopUint32(&mount_type) ||
      !reader->PopString(&mount_path)) {
    return false;
  }
  *entry = MountEntry(static_cast<MountError>(error_code), source_path,
                      static_cast<MountType>(mount_type), mount_path);
  return true;
}

// The CrosDisksClient implementation.
class CrosDisksClientImpl : public CrosDisksClient {
 public:
  CrosDisksClientImpl() : proxy_(NULL), weak_ptr_factory_(this) {}

  // CrosDisksClient override.
  void Mount(const std::string& source_path,
             const std::string& source_format,
             const std::string& mount_label,
             MountAccessMode access_mode,
             RemountOption remount,
             const base::Closure& callback,
             const base::Closure& error_callback) override {
    dbus::MethodCall method_call(cros_disks::kCrosDisksInterface,
                                 cros_disks::kMount);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(source_path);
    writer.AppendString(source_format);
    std::vector<std::string> mount_options =
        ComposeMountOptions(mount_label, access_mode, remount);
    writer.AppendArrayOfStrings(mount_options);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CrosDisksClientImpl::OnMount,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback,
                                  error_callback));
  }

  // CrosDisksClient override.
  void Unmount(const std::string& device_path,
               UnmountOptions options,
               const base::Closure& callback,
               const base::Closure& error_callback) override {
    dbus::MethodCall method_call(cros_disks::kCrosDisksInterface,
                                 cros_disks::kUnmount);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(device_path);

    std::vector<std::string> unmount_options(
        kDefaultUnmountOptions,
        kDefaultUnmountOptions + arraysize(kDefaultUnmountOptions));
    if (options == UNMOUNT_OPTIONS_LAZY)
      unmount_options.push_back(kLazyUnmountOption);

    writer.AppendArrayOfStrings(unmount_options);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CrosDisksClientImpl::OnUnmount,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback,
                                  error_callback));
  }

  // CrosDisksClient override.
  void EnumerateAutoMountableDevices(
      const EnumerateAutoMountableDevicesCallback& callback,
      const base::Closure& error_callback) override {
    dbus::MethodCall method_call(cros_disks::kCrosDisksInterface,
                                 cros_disks::kEnumerateAutoMountableDevices);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&CrosDisksClientImpl::OnEnumerateAutoMountableDevices,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback,
                   error_callback));
  }

  // CrosDisksClient override.
  void EnumerateMountEntries(const EnumerateMountEntriesCallback& callback,
                             const base::Closure& error_callback) override {
    dbus::MethodCall method_call(cros_disks::kCrosDisksInterface,
                                 cros_disks::kEnumerateMountEntries);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&CrosDisksClientImpl::OnEnumerateMountEntries,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback,
                   error_callback));
  }

  // CrosDisksClient override.
  void Format(const std::string& device_path,
              const std::string& filesystem,
              const base::Closure& callback,
              const base::Closure& error_callback) override {
    dbus::MethodCall method_call(cros_disks::kCrosDisksInterface,
                                 cros_disks::kFormat);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(device_path);
    writer.AppendString(filesystem);
    // No format option is currently specified, but we can later use this
    // argument to specify options for the format operation.
    std::vector<std::string> format_options;
    writer.AppendArrayOfStrings(format_options);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CrosDisksClientImpl::OnFormat,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback,
                                  error_callback));
  }

  // CrosDisksClient override.
  void GetDeviceProperties(const std::string& device_path,
                           const GetDevicePropertiesCallback& callback,
                           const base::Closure& error_callback) override {
    dbus::MethodCall method_call(cros_disks::kCrosDisksInterface,
                                 cros_disks::kGetDeviceProperties);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(device_path);
    proxy_->CallMethod(&method_call,
                       dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CrosDisksClientImpl::OnGetDeviceProperties,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  device_path,
                                  callback,
                                  error_callback));
  }

  // CrosDisksClient override.
  void SetMountEventHandler(
      const MountEventHandler& mount_event_handler) override {
    static const SignalEventTuple kSignalEventTuples[] = {
      { cros_disks::kDeviceAdded, CROS_DISKS_DEVICE_ADDED },
      { cros_disks::kDeviceScanned, CROS_DISKS_DEVICE_SCANNED },
      { cros_disks::kDeviceRemoved, CROS_DISKS_DEVICE_REMOVED },
      { cros_disks::kDiskAdded, CROS_DISKS_DISK_ADDED },
      { cros_disks::kDiskChanged, CROS_DISKS_DISK_CHANGED },
      { cros_disks::kDiskRemoved, CROS_DISKS_DISK_REMOVED },
    };
    const size_t kNumSignalEventTuples = arraysize(kSignalEventTuples);

    for (size_t i = 0; i < kNumSignalEventTuples; ++i) {
      proxy_->ConnectToSignal(
          cros_disks::kCrosDisksInterface,
          kSignalEventTuples[i].signal_name,
          base::Bind(&CrosDisksClientImpl::OnMountEvent,
                     weak_ptr_factory_.GetWeakPtr(),
                     kSignalEventTuples[i].event_type,
                     mount_event_handler),
          base::Bind(&CrosDisksClientImpl::OnSignalConnected,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }

  // CrosDisksClient override.
  void SetMountCompletedHandler(
      const MountCompletedHandler& mount_completed_handler) override {
    proxy_->ConnectToSignal(
        cros_disks::kCrosDisksInterface,
        cros_disks::kMountCompleted,
        base::Bind(&CrosDisksClientImpl::OnMountCompleted,
                   weak_ptr_factory_.GetWeakPtr(),
                   mount_completed_handler),
        base::Bind(&CrosDisksClientImpl::OnSignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // CrosDisksClient override.
  void SetFormatCompletedHandler(
      const FormatCompletedHandler& format_completed_handler) override {
    proxy_->ConnectToSignal(
        cros_disks::kCrosDisksInterface,
        cros_disks::kFormatCompleted,
        base::Bind(&CrosDisksClientImpl::OnFormatCompleted,
                   weak_ptr_factory_.GetWeakPtr(),
                   format_completed_handler),
        base::Bind(&CrosDisksClientImpl::OnSignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(
        cros_disks::kCrosDisksServiceName,
        dbus::ObjectPath(cros_disks::kCrosDisksServicePath));
  }

 private:
  // A struct to contain a pair of signal name and mount event type.
  // Used by SetMountEventHandler.
  struct SignalEventTuple {
    const char *signal_name;
    MountEventType event_type;
  };

  // Handles the result of Mount and calls |callback| or |error_callback|.
  void OnMount(const base::Closure& callback,
               const base::Closure& error_callback,
               dbus::Response* response) {
    if (!response) {
      error_callback.Run();
      return;
    }
    callback.Run();
  }

  // Handles the result of Unmount and calls |callback| or |error_callback|.
  void OnUnmount(const base::Closure& callback,
                 const base::Closure& error_callback,
                 dbus::Response* response) {
    if (!response) {
      error_callback.Run();
      return;
    }

    // Temporarly allow Unmount method to report failure both by setting dbus
    // error (in which case response is not set) and by returning mount error
    // different from MOUNT_ERROR_NONE. This is done so we can change Unmount
    // method to return mount error (http://crbug.com/288974) without breaking
    // Chrome.
    // TODO(tbarzic): When Unmount implementation is changed on cros disks side,
    // make this fail if reader is not able to read the error code value from
    // the response.
    dbus::MessageReader reader(response);
    uint32_t error_code = 0;
    if (reader.PopUint32(&error_code) &&
        static_cast<MountError>(error_code) != MOUNT_ERROR_NONE) {
      error_callback.Run();
      return;
    }

    callback.Run();
  }

  // Handles the result of EnumerateAutoMountableDevices and calls |callback| or
  // |error_callback|.
  void OnEnumerateAutoMountableDevices(
      const EnumerateAutoMountableDevicesCallback& callback,
      const base::Closure& error_callback,
      dbus::Response* response) {
    if (!response) {
      error_callback.Run();
      return;
    }
    dbus::MessageReader reader(response);
    std::vector<std::string> device_paths;
    if (!reader.PopArrayOfStrings(&device_paths)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      error_callback.Run();
      return;
    }
    callback.Run(device_paths);
  }

  // Handles the result of EnumerateMountEntries and calls |callback| or
  // |error_callback|.
  void OnEnumerateMountEntries(
      const EnumerateMountEntriesCallback& callback,
      const base::Closure& error_callback,
      dbus::Response* response) {
    if (!response) {
      error_callback.Run();
      return;
    }

    dbus::MessageReader reader(response);
    dbus::MessageReader array_reader(NULL);
    if (!reader.PopArray(&array_reader)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      error_callback.Run();
      return;
    }

    std::vector<MountEntry> entries;
    while (array_reader.HasMoreData()) {
      MountEntry entry;
      dbus::MessageReader sub_reader(NULL);
      if (!array_reader.PopStruct(&sub_reader) ||
          !ReadMountEntryFromDbus(&sub_reader, &entry)) {
        LOG(ERROR) << "Invalid response: " << response->ToString();
        error_callback.Run();
        return;
      }
      entries.push_back(entry);
    }
    callback.Run(entries);
  }

  // Handles the result of Format and calls |callback| or |error_callback|.
  void OnFormat(const base::Closure& callback,
                const base::Closure& error_callback,
                dbus::Response* response) {
    if (!response) {
      error_callback.Run();
      return;
    }
    callback.Run();
  }

  // Handles the result of GetDeviceProperties and calls |callback| or
  // |error_callback|.
  void OnGetDeviceProperties(const std::string& device_path,
                             const GetDevicePropertiesCallback& callback,
                             const base::Closure& error_callback,
                             dbus::Response* response) {
    if (!response) {
      error_callback.Run();
      return;
    }
    DiskInfo disk(device_path, response);
    callback.Run(disk);
  }

  // Handles mount event signals and calls |handler|.
  void OnMountEvent(MountEventType event_type,
                    MountEventHandler handler,
                    dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    std::string device;
    if (!reader.PopString(&device)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    handler.Run(event_type, device);
  }

  // Handles MountCompleted signal and calls |handler|.
  void OnMountCompleted(MountCompletedHandler handler, dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    MountEntry entry;
    if (!ReadMountEntryFromDbus(&reader, &entry)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    handler.Run(entry);
  }

  // Handles FormatCompleted signal and calls |handler|.
  void OnFormatCompleted(FormatCompletedHandler handler, dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    uint32_t error_code = 0;
    std::string device_path;
    if (!reader.PopUint32(&error_code) || !reader.PopString(&device_path)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    handler.Run(static_cast<FormatError>(error_code), device_path);
  }

  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool succeeded) {
    LOG_IF(ERROR, !succeeded) << "Connect to " << interface << " " <<
        signal << " failed.";
  }

  dbus::ObjectProxy* proxy_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CrosDisksClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrosDisksClientImpl);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DiskInfo

DiskInfo::DiskInfo(const std::string& device_path, dbus::Response* response)
    : device_path_(device_path),
      is_drive_(false),
      has_media_(false),
      on_boot_device_(false),
      on_removable_device_(false),
      device_type_(DEVICE_TYPE_UNKNOWN),
      total_size_in_bytes_(0),
      is_read_only_(false),
      is_hidden_(true) {
  InitializeFromResponse(response);
}

DiskInfo::~DiskInfo() {
}

// Initializes |this| from |response| given by the cros-disks service.
// Below is an example of |response|'s raw message (long string is ellipsized).
//
//
// message_type: MESSAGE_METHOD_RETURN
// destination: :1.8
// sender: :1.16
// signature: a{sv}
// serial: 96
// reply_serial: 267
//
// array [
//   dict entry {
//     string "DeviceFile"
//     variant       string "/dev/sdb"
//   }
//   dict entry {
//     string "DeviceIsDrive"
//     variant       bool true
//   }
//   dict entry {
//     string "DeviceIsMediaAvailable"
//     variant       bool true
//   }
//   dict entry {
//     string "DeviceIsMounted"
//     variant       bool false
//   }
//   dict entry {
//     string "DeviceIsOnBootDevice"
//     variant       bool false
//   }
//   dict entry {
//     string "DeviceIsOnRemovableDevice"
//     variant       bool true
//   }
//   dict entry {
//     string "DeviceIsReadOnly"
//     variant       bool false
//   }
//   dict entry {
//     string "DeviceIsVirtual"
//     variant       bool false
//   }
//   dict entry {
//     string "DeviceMediaType"
//     variant       uint32_t 1
//   }
//   dict entry {
//     string "DeviceMountPaths"
//     variant       array [
//       ]
//   }
//   dict entry {
//     string "DevicePresentationHide"
//     variant       bool true
//   }
//   dict entry {
//     string "DeviceSize"
//     variant       uint64_t 7998537728
//   }
//   dict entry {
//     string "DriveIsRotational"
//     variant       bool false
//   }
//   dict entry {
//     string "VendorId"
//     variant       string "18d1"
//   }
//   dict entry {
//     string "VendorName"
//     variant       string "Google Inc."
//   }
//   dict entry {
//     string "ProductId"
//     variant       string "4e11"
//   }
//   dict entry {
//     string "ProductName"
//     variant       string "Nexus One"
//   }
//   dict entry {
//     string "DriveModel"
//     variant       string "TransMemory"
//   }
//   dict entry {
//     string "IdLabel"
//     variant       string ""
//   }
//   dict entry {
//     string "IdUuid"
//     variant       string ""
//   }
//   dict entry {
//     string "NativePath"
//     variant       string "/sys/devices/pci0000:00/0000:00:1d.7/usb1/1-4/...
//   }
// ]
void DiskInfo::InitializeFromResponse(dbus::Response* response) {
  dbus::MessageReader reader(response);
  std::unique_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
  base::DictionaryValue* properties = NULL;
  if (!value || !value->GetAsDictionary(&properties))
    return;

  properties->GetBooleanWithoutPathExpansion(
      cros_disks::kDeviceIsDrive, &is_drive_);
  properties->GetBooleanWithoutPathExpansion(
      cros_disks::kDeviceIsReadOnly, &is_read_only_);
  properties->GetBooleanWithoutPathExpansion(
      cros_disks::kDevicePresentationHide, &is_hidden_);
  properties->GetBooleanWithoutPathExpansion(
      cros_disks::kDeviceIsMediaAvailable, &has_media_);
  properties->GetBooleanWithoutPathExpansion(
      cros_disks::kDeviceIsOnBootDevice, &on_boot_device_);
  properties->GetBooleanWithoutPathExpansion(
      cros_disks::kDeviceIsOnRemovableDevice, &on_removable_device_);
  properties->GetStringWithoutPathExpansion(
      cros_disks::kNativePath, &system_path_);
  properties->GetStringWithoutPathExpansion(
      cros_disks::kDeviceFile, &file_path_);
  properties->GetStringWithoutPathExpansion(cros_disks::kVendorId, &vendor_id_);
  properties->GetStringWithoutPathExpansion(
      cros_disks::kVendorName, &vendor_name_);
  properties->GetStringWithoutPathExpansion(
      cros_disks::kProductId, &product_id_);
  properties->GetStringWithoutPathExpansion(
      cros_disks::kProductName, &product_name_);
  properties->GetStringWithoutPathExpansion(
      cros_disks::kDriveModel, &drive_model_);
  properties->GetStringWithoutPathExpansion(cros_disks::kIdLabel, &label_);
  properties->GetStringWithoutPathExpansion(cros_disks::kIdUuid, &uuid_);

  // dbus::PopDataAsValue() pops uint64_t as double.
  // The top 11 bits of uint64_t are dropped by the use of double. But, this
  // works
  // unless the size exceeds 8 PB.
  double device_size_double = 0;
  if (properties->GetDoubleWithoutPathExpansion(cros_disks::kDeviceSize,
                                                &device_size_double))
    total_size_in_bytes_ = device_size_double;

  // dbus::PopDataAsValue() pops uint32_t as double.
  double media_type_double = 0;
  if (properties->GetDoubleWithoutPathExpansion(cros_disks::kDeviceMediaType,
                                                &media_type_double))
    device_type_ = DeviceMediaTypeToDeviceType(media_type_double);

  base::ListValue* mount_paths = NULL;
  if (properties->GetListWithoutPathExpansion(cros_disks::kDeviceMountPaths,
                                              &mount_paths))
    mount_paths->GetString(0, &mount_path_);
}

////////////////////////////////////////////////////////////////////////////////
// CrosDisksClient

CrosDisksClient::CrosDisksClient() {}

CrosDisksClient::~CrosDisksClient() {}

// static
CrosDisksClient* CrosDisksClient::Create(DBusClientImplementationType type) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new CrosDisksClientImpl();
  DCHECK_EQ(FAKE_DBUS_CLIENT_IMPLEMENTATION, type);
  return new FakeCrosDisksClient();
}

// static
base::FilePath CrosDisksClient::GetArchiveMountPoint() {
  return base::FilePath(base::SysInfo::IsRunningOnChromeOS() ?
                        FILE_PATH_LITERAL("/media/archive") :
                        FILE_PATH_LITERAL("/tmp/chromeos/media/archive"));
}

// static
base::FilePath CrosDisksClient::GetRemovableDiskMountPoint() {
  return base::FilePath(base::SysInfo::IsRunningOnChromeOS() ?
                        FILE_PATH_LITERAL("/media/removable") :
                        FILE_PATH_LITERAL("/tmp/chromeos/media/removable"));
}

// static
std::vector<std::string> CrosDisksClient::ComposeMountOptions(
    const std::string& mount_label,
    MountAccessMode access_mode,
    RemountOption remount) {
  std::vector<std::string> mount_options(
      kDefaultMountOptions,
      kDefaultMountOptions + arraysize(kDefaultMountOptions));
  switch (access_mode) {
    case MOUNT_ACCESS_MODE_READ_ONLY:
      mount_options.push_back(kReadOnlyOption);
      break;
    case MOUNT_ACCESS_MODE_READ_WRITE:
      mount_options.push_back(kReadWriteOption);
      break;
  }
  if (remount == REMOUNT_OPTION_REMOUNT_EXISTING_DEVICE) {
    mount_options.push_back(kRemountOption);
  }

  if (!mount_label.empty()) {
    std::string mount_label_option =
        base::StringPrintf("%s=%s", kMountLabelOption, mount_label.c_str());
    mount_options.push_back(mount_label_option);
  }
  return mount_options;
}

}  // namespace chromeos
