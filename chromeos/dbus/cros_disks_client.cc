// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cros_disks_client.h"

#include <map>

#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char* kDefaultMountOptions[] = {
  "rw",
  "nodev",
  "noexec",
  "nosuid",
};

const char* kDefaultUnmountOptions[] = {
  "force",
};

const char kLazyUnmountOption[] = "lazy";

const char kMountLabelOption[] = "mountlabel";

// Checks if retrieved media type is in boundaries of DeviceMediaType.
bool IsValidMediaType(uint32 type) {
  return type < static_cast<uint32>(cros_disks::DEVICE_MEDIA_NUM_VALUES);
}


// Translates enum used in cros-disks to enum used in Chrome.
// Note that we could just do static_cast, but this is less sensitive to
// changes in cros-disks.
DeviceType DeviceMediaTypeToDeviceType(uint32 media_type_uint32) {
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

// Pops a bool value when |reader| is not NULL.
// Returns true when a value is popped, false otherwise.
bool MaybePopBool(dbus::MessageReader* reader, bool* value) {
  if (!reader)
    return false;
  return reader->PopBool(value);
}

// Pops a string value when |reader| is not NULL.
// Returns true when a value is popped, false otherwise.
bool MaybePopString(dbus::MessageReader* reader, std::string* value) {
  if (!reader)
    return false;
  return reader->PopString(value);
}

// Pops a uint32 value when |reader| is not NULL.
// Returns true when a value is popped, false otherwise.
bool MaybePopUint32(dbus::MessageReader* reader, uint32* value) {
  if (!reader)
    return false;

  return reader->PopUint32(value);
}

// Pops a uint64 value when |reader| is not NULL.
// Returns true when a value is popped, false otherwise.
bool MaybePopUint64(dbus::MessageReader* reader, uint64* value) {
  if (!reader)
    return false;
  return reader->PopUint64(value);
}

// Pops an array of strings when |reader| is not NULL.
// Returns true when an array is popped, false otherwise.
bool MaybePopArrayOfStrings(dbus::MessageReader* reader,
                            std::vector<std::string>* value) {
  if (!reader)
    return false;
  return reader->PopArrayOfStrings(value);
}

// The CrosDisksClient implementation.
class CrosDisksClientImpl : public CrosDisksClient {
 public:
  explicit CrosDisksClientImpl(dbus::Bus* bus)
      : proxy_(bus->GetObjectProxy(
          cros_disks::kCrosDisksServiceName,
          dbus::ObjectPath(cros_disks::kCrosDisksServicePath))),
        weak_ptr_factory_(this) {
  }

  // CrosDisksClient override.
  virtual void Mount(const std::string& source_path,
                     const std::string& source_format,
                     const std::string& mount_label,
                     MountType type,
                     const MountCallback& callback,
                     const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(cros_disks::kCrosDisksInterface,
                                 cros_disks::kMount);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(source_path);
    writer.AppendString(source_format);
    std::vector<std::string> mount_options(kDefaultMountOptions,
                                           kDefaultMountOptions +
                                           arraysize(kDefaultMountOptions));
    if (!mount_label.empty()) {
      std::string mount_label_option = base::StringPrintf("%s=%s",
                                                          kMountLabelOption,
                                                          mount_label.c_str());
      mount_options.push_back(mount_label_option);
    }
    writer.AppendArrayOfStrings(mount_options);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CrosDisksClientImpl::OnMount,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback,
                                  error_callback));
  }

  // CrosDisksClient override.
  virtual void Unmount(const std::string& device_path,
                       UnmountOptions options,
                       const UnmountCallback& callback,
                       const UnmountCallback& error_callback) OVERRIDE {
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
                                  device_path,
                                  callback,
                                  error_callback));
  }

  // CrosDisksClient override.
  virtual void EnumerateAutoMountableDevices(
      const EnumerateAutoMountableDevicesCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {
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
  virtual void FormatDevice(const std::string& device_path,
                            const std::string& filesystem,
                            const FormatDeviceCallback& callback,
                            const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(cros_disks::kCrosDisksInterface,
                                 cros_disks::kFormatDevice);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(device_path);
    writer.AppendString(filesystem);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&CrosDisksClientImpl::OnFormatDevice,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  device_path,
                                  callback,
                                  error_callback));
  }

  // CrosDisksClient override.
  virtual void GetDeviceProperties(
      const std::string& device_path,
      const GetDevicePropertiesCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {
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
  virtual void SetUpConnections(
      const MountEventHandler& mount_event_handler,
      const MountCompletedHandler& mount_completed_handler) OVERRIDE {
    static const SignalEventTuple kSignalEventTuples[] = {
      { cros_disks::kDeviceAdded, CROS_DISKS_DEVICE_ADDED },
      { cros_disks::kDeviceScanned, CROS_DISKS_DEVICE_SCANNED },
      { cros_disks::kDeviceRemoved, CROS_DISKS_DEVICE_REMOVED },
      { cros_disks::kDiskAdded, CROS_DISKS_DISK_ADDED },
      { cros_disks::kDiskChanged, CROS_DISKS_DISK_CHANGED },
      { cros_disks::kDiskRemoved, CROS_DISKS_DISK_REMOVED },
      { cros_disks::kFormattingFinished, CROS_DISKS_FORMATTING_FINISHED },
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
    proxy_->ConnectToSignal(
        cros_disks::kCrosDisksInterface,
        cros_disks::kMountCompleted,
        base::Bind(&CrosDisksClientImpl::OnMountCompleted,
                   weak_ptr_factory_.GetWeakPtr(),
                   mount_completed_handler),
        base::Bind(&CrosDisksClientImpl::OnSignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // A struct to contain a pair of signal name and mount event type.
  // Used by SetUpConnections.
  struct SignalEventTuple {
    const char *signal_name;
    MountEventType event_type;
  };

  // Handles the result of Mount and calls |callback| or |error_callback|.
  void OnMount(const MountCallback& callback,
               const ErrorCallback& error_callback,
               dbus::Response* response) {
    if (!response) {
      error_callback.Run();
      return;
    }
    callback.Run();
  }

  // Handles the result of Unount and calls |callback| or |error_callback|.
  void OnUnmount(const std::string& device_path,
                 const UnmountCallback& callback,
                 const UnmountCallback& error_callback,
                 dbus::Response* response) {
    if (!response) {
      error_callback.Run(device_path);
      return;
    }
    callback.Run(device_path);
  }

  // Handles the result of EnumerateAutoMountableDevices and calls |callback| or
  // |error_callback|.
  void OnEnumerateAutoMountableDevices(
      const EnumerateAutoMountableDevicesCallback& callback,
      const ErrorCallback& error_callback,
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

  // Handles the result of FormatDevice and calls |callback| or
  // |error_callback|.
  void OnFormatDevice(const std::string& device_path,
                      const FormatDeviceCallback& callback,
                      const ErrorCallback& error_callback,
                      dbus::Response* response) {
    if (!response) {
      error_callback.Run();
      return;
    }
    dbus::MessageReader reader(response);
    bool success = false;
    if (!reader.PopBool(&success)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      error_callback.Run();
      return;
    }
    callback.Run(device_path, success);
  }

  // Handles the result of GetDeviceProperties and calls |callback| or
  // |error_callback|.
  void OnGetDeviceProperties(const std::string& device_path,
                             const GetDevicePropertiesCallback& callback,
                             const ErrorCallback& error_callback,
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
    unsigned int error_code = 0;
    std::string source_path;
    unsigned int mount_type = 0;
    std::string mount_path;
    if (!reader.PopUint32(&error_code) ||
        !reader.PopString(&source_path) ||
        !reader.PopUint32(&mount_type) ||
        !reader.PopString(&mount_path)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    handler.Run(static_cast<MountError>(error_code), source_path,
                static_cast<MountType>(mount_type), mount_path);
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

// A stub implementaion of CrosDisksClient.
class CrosDisksClientStubImpl : public CrosDisksClient {
 public:
  CrosDisksClientStubImpl()
      : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

  virtual ~CrosDisksClientStubImpl() {}

  // CrosDisksClient overrides:
  virtual void Mount(const std::string& source_path,
                     const std::string& source_format,
                     const std::string& mount_label,
                     MountType type,
                     const MountCallback& callback,
                     const ErrorCallback& error_callback) OVERRIDE {
    // Already mounted path.
    if (mounted_paths_.count(source_path) != 0) {
      FinishMount(MOUNT_ERROR_PATH_ALREADY_MOUNTED, source_path, type,
                  std::string(), callback);
      return;
    }

    // This stub implementation only accepts archive mount requests.
    if (type != MOUNT_TYPE_ARCHIVE) {
      FinishMount(MOUNT_ERROR_INTERNAL, source_path, type, std::string(),
                  callback);
      return;
    }

    const base::FilePath mounted_path = GetArchiveMountPoint().Append(
        base::FilePath::FromUTF8Unsafe(mount_label));

    // Perform fake mount.
    base::PostTaskAndReplyWithResult(
        base::WorkerPool::GetTaskRunner(true /* task_is_slow */),
        FROM_HERE,
        base::Bind(&PerformFakeMount,
                   source_path,
                   mounted_path),
        base::Bind(&CrosDisksClientStubImpl::ContinueMount,
                   weak_ptr_factory_.GetWeakPtr(),
                   source_path,
                   type,
                   callback,
                   mounted_path));
  }

  virtual void Unmount(const std::string& device_path,
                       UnmountOptions options,
                       const UnmountCallback& callback,
                       const UnmountCallback& error_callback) OVERRIDE {
    // Not mounted.
    if (mounted_paths_.count(device_path) == 0) {
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE, base::Bind(error_callback, device_path));
      return;
    }

    const base::FilePath mounted_path = mounted_paths_[device_path];
    mounted_paths_.erase(device_path);

    // Remove the directory created in Mount().
    base::WorkerPool::PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&file_util::Delete),
                   mounted_path,
                   true /* recursive */),
        true /* task_is_slow */);

    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(callback, device_path));
  }

  virtual void EnumerateAutoMountableDevices(
      const EnumerateAutoMountableDevicesCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    std::vector<std::string> device_paths;
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(callback, device_paths));
  }

  virtual void FormatDevice(const std::string& device_path,
                            const std::string& filesystem,
                            const FormatDeviceCallback& callback,
                            const ErrorCallback& error_callback) OVERRIDE {
    base::MessageLoopProxy::current()->PostTask(FROM_HERE, error_callback);
  }

  virtual void GetDeviceProperties(
      const std::string& device_path,
      const GetDevicePropertiesCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    base::MessageLoopProxy::current()->PostTask(FROM_HERE, error_callback);
  }

  virtual void SetUpConnections(
      const MountEventHandler& mount_event_handler,
      const MountCompletedHandler& mount_completed_handler) OVERRIDE {
    mount_event_handler_ = mount_event_handler;
    mount_completed_handler_ = mount_completed_handler;
  }

 private:
  // Performs file actions for Mount().
  static MountError PerformFakeMount(const std::string& source_path,
                                     const base::FilePath& mounted_path) {
    // Check the source path exists.
    if (!file_util::PathExists(base::FilePath::FromUTF8Unsafe(source_path))) {
      DLOG(ERROR) << "Source does not exist at " << source_path;
      return MOUNT_ERROR_INVALID_PATH;
    }

    // Just create an empty directory and shows it as the mounted directory.
    if (!file_util::CreateDirectory(mounted_path)) {
      DLOG(ERROR) << "Failed to create directory at " << mounted_path.value();
      return MOUNT_ERROR_DIRECTORY_CREATION_FAILED;
    }

    // Put a dummy file.
    const base::FilePath dummy_file_path =
        mounted_path.Append("SUCCESSFULLY_PERFORMED_FAKE_MOUNT.txt");
    const std::string dummy_file_content = "This is a dummy file.";
    const int write_result = file_util::WriteFile(
        dummy_file_path, dummy_file_content.data(), dummy_file_content.size());
    if (write_result != static_cast<int>(dummy_file_content.size())) {
      DLOG(ERROR) << "Failed to put a dummy file at "
                  << dummy_file_path.value();
      return MOUNT_ERROR_MOUNT_PROGRAM_FAILED;
    }

    return MOUNT_ERROR_NONE;
  }

  // Part of Mount() implementation.
  void ContinueMount(const std::string& source_path,
                     MountType type,
                     const MountCallback& callback,
                     const base::FilePath& mounted_path,
                     MountError mount_error) {
    if (mount_error != MOUNT_ERROR_NONE) {
      FinishMount(mount_error, source_path, type, std::string(), callback);
      return;
    }
    mounted_paths_[source_path] = mounted_path;
    FinishMount(MOUNT_ERROR_NONE, source_path, type,
                mounted_path.AsUTF8Unsafe(), callback);
  }

  // Runs |callback| and sends MountCompleted signal.
  // Part of Mount() implementation.
  void FinishMount(MountError error,
                   const std::string& source_path,
                   MountType type,
                   const std::string& mounted_path,
                   const MountCallback& callback) {
    base::MessageLoopProxy::current()->PostTask(FROM_HERE, callback);
    if (!mount_completed_handler_.is_null()) {
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE,
          base::Bind(mount_completed_handler_,
                     error, source_path, type, mounted_path));
    }
  }

  // Source path to mounted path map.
  std::map<std::string, base::FilePath> mounted_paths_;

  MountEventHandler mount_event_handler_;
  MountCompletedHandler mount_completed_handler_;

  base::WeakPtrFactory<CrosDisksClientStubImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrosDisksClientStubImpl);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DiskInfo

DiskInfo::DiskInfo(const std::string& device_path, dbus::Response* response)
    : device_path_(device_path),
      is_drive_(false),
      has_media_(false),
      on_boot_device_(false),
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
//     string "DeviceIsReadOnly"
//     variant       bool false
//   }
//   dict entry {
//     string "DeviceIsVirtual"
//     variant       bool false
//   }
//   dict entry {
//     string "DeviceMediaType"
//     variant       uint32 1
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
//     variant       uint64 7998537728
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
  dbus::MessageReader response_reader(response);
  dbus::MessageReader array_reader(response);
  if (!response_reader.PopArray(&array_reader)) {
    LOG(ERROR) << "Invalid response: " << response->ToString();
    return;
  }
  // TODO(satorux): Rework this code using Protocol Buffers. crosbug.com/22626
  typedef std::map<std::string, dbus::MessageReader*> PropertiesMap;
  PropertiesMap properties;
  STLValueDeleter<PropertiesMap> properties_value_deleter(&properties);
  while (array_reader.HasMoreData()) {
    dbus::MessageReader* value_reader = new dbus::MessageReader(response);
    dbus::MessageReader dict_entry_reader(response);
    std::string key;
    if (!array_reader.PopDictEntry(&dict_entry_reader) ||
        !dict_entry_reader.PopString(&key) ||
        !dict_entry_reader.PopVariant(value_reader)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      return;
    }
    properties[key] = value_reader;
  }
  MaybePopBool(properties[cros_disks::kDeviceIsDrive], &is_drive_);
  MaybePopBool(properties[cros_disks::kDeviceIsReadOnly], &is_read_only_);
  MaybePopBool(properties[cros_disks::kDevicePresentationHide], &is_hidden_);
  MaybePopBool(properties[cros_disks::kDeviceIsMediaAvailable], &has_media_);
  MaybePopBool(properties[cros_disks::kDeviceIsOnBootDevice],
               &on_boot_device_);
  MaybePopString(properties[cros_disks::kNativePath], &system_path_);
  MaybePopString(properties[cros_disks::kDeviceFile], &file_path_);
  MaybePopString(properties[cros_disks::kVendorId], &vendor_id_);
  MaybePopString(properties[cros_disks::kVendorName], &vendor_name_);
  MaybePopString(properties[cros_disks::kProductId], &product_id_);
  MaybePopString(properties[cros_disks::kProductName], &product_name_);
  MaybePopString(properties[cros_disks::kDriveModel], &drive_model_);
  MaybePopString(properties[cros_disks::kIdLabel], &label_);
  MaybePopString(properties[cros_disks::kIdUuid], &uuid_);
  MaybePopUint64(properties[cros_disks::kDeviceSize], &total_size_in_bytes_);

  uint32 media_type_uint32 = 0;
  if (MaybePopUint32(properties[cros_disks::kDeviceMediaType],
                     &media_type_uint32)) {
    device_type_ = DeviceMediaTypeToDeviceType(media_type_uint32);
  }

  std::vector<std::string> mount_paths;
  if (MaybePopArrayOfStrings(properties[cros_disks::kDeviceMountPaths],
                             &mount_paths) && !mount_paths.empty())
    mount_path_ = mount_paths[0];
}

////////////////////////////////////////////////////////////////////////////////
// CrosDisksClient

CrosDisksClient::CrosDisksClient() {}

CrosDisksClient::~CrosDisksClient() {}

// static
CrosDisksClient* CrosDisksClient::Create(DBusClientImplementationType type,
                                         dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new CrosDisksClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new CrosDisksClientStubImpl();
}

// static
base::FilePath CrosDisksClient::GetArchiveMountPoint() {
  return base::FilePath(base::chromeos::IsRunningOnChromeOS() ?
                        FILE_PATH_LITERAL("/media/archive") :
                        FILE_PATH_LITERAL("/tmp/chromeos/media/archive"));
}

// static
base::FilePath CrosDisksClient::GetRemovableDiskMountPoint() {
  return base::FilePath(base::chromeos::IsRunningOnChromeOS() ?
                        FILE_PATH_LITERAL("/media/removable") :
                        FILE_PATH_LITERAL("/tmp/chromeos/media/removable"));
}

}  // namespace chromeos
