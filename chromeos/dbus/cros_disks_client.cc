// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cros_disks_client.h"

#include <map>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "base/values.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
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

bool ReadMountEntryFromDbus(dbus::MessageReader* reader, MountEntry* entry) {
  uint32 error_code = 0;
  std::string source_path;
  uint32 mount_type = 0;
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
  virtual void Mount(const std::string& source_path,
                     const std::string& source_format,
                     const std::string& mount_label,
                     const base::Closure& callback,
                     const base::Closure& error_callback) OVERRIDE {
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
                       const base::Closure& callback,
                       const base::Closure& error_callback) OVERRIDE {
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
  virtual void EnumerateAutoMountableDevices(
      const EnumerateAutoMountableDevicesCallback& callback,
      const base::Closure& error_callback) OVERRIDE {
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
  virtual void EnumerateMountEntries(
      const EnumerateMountEntriesCallback& callback,
      const base::Closure& error_callback) OVERRIDE {
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
  virtual void Format(const std::string& device_path,
                      const std::string& filesystem,
                      const base::Closure& callback,
                      const base::Closure& error_callback) OVERRIDE {
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
  virtual void GetDeviceProperties(
      const std::string& device_path,
      const GetDevicePropertiesCallback& callback,
      const base::Closure& error_callback) OVERRIDE {
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
  virtual void SetMountEventHandler(
      const MountEventHandler& mount_event_handler) OVERRIDE {
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
  virtual void SetMountCompletedHandler(
      const MountCompletedHandler& mount_completed_handler) OVERRIDE {
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
  virtual void SetFormatCompletedHandler(
      const FormatCompletedHandler& format_completed_handler) OVERRIDE {
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
  virtual void Init(dbus::Bus* bus) OVERRIDE {
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
    uint32 error_code = 0;
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
    uint32 error_code = 0;
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

// A stub implementaion of CrosDisksClient.
class CrosDisksClientStubImpl : public CrosDisksClient {
 public:
  CrosDisksClientStubImpl()
      : weak_ptr_factory_(this) {}

  virtual ~CrosDisksClientStubImpl() {}

  // CrosDisksClient overrides:
  virtual void Init(dbus::Bus* bus) OVERRIDE {}
  virtual void Mount(const std::string& source_path,
                     const std::string& source_format,
                     const std::string& mount_label,
                     const base::Closure& callback,
                     const base::Closure& error_callback) OVERRIDE {
    // This stub implementation only accepts archive mount requests.
    const MountType type = MOUNT_TYPE_ARCHIVE;

    const base::FilePath mounted_path = GetArchiveMountPoint().Append(
        base::FilePath::FromUTF8Unsafe(mount_label));

    // Already mounted path.
    if (mounted_to_source_path_map_.count(mounted_path.value()) != 0) {
      FinishMount(MOUNT_ERROR_PATH_ALREADY_MOUNTED, source_path, type,
                  std::string(), callback);
      return;
    }

    // Perform fake mount.
    base::PostTaskAndReplyWithResult(
        base::WorkerPool::GetTaskRunner(true /* task_is_slow */).get(),
        FROM_HERE,
        base::Bind(&PerformFakeMount, source_path, mounted_path),
        base::Bind(&CrosDisksClientStubImpl::ContinueMount,
                   weak_ptr_factory_.GetWeakPtr(),
                   source_path,
                   type,
                   callback,
                   mounted_path));
  }

  virtual void Unmount(const std::string& device_path,
                       UnmountOptions options,
                       const base::Closure& callback,
                       const base::Closure& error_callback) OVERRIDE {
    // Not mounted.
    if (mounted_to_source_path_map_.count(device_path) == 0) {
      base::MessageLoopProxy::current()->PostTask(FROM_HERE, error_callback);
      return;
    }

    mounted_to_source_path_map_.erase(device_path);

    // Remove the directory created in Mount().
    base::WorkerPool::PostTaskAndReply(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&base::DeleteFile),
                   base::FilePath::FromUTF8Unsafe(device_path),
                   true /* recursive */),
        callback,
        true /* task_is_slow */);
  }

  virtual void EnumerateAutoMountableDevices(
      const EnumerateAutoMountableDevicesCallback& callback,
      const base::Closure& error_callback) OVERRIDE {
    std::vector<std::string> device_paths;
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(callback, device_paths));
  }

  virtual void EnumerateMountEntries(
      const EnumerateMountEntriesCallback& callback,
      const base::Closure& error_callback) OVERRIDE {
    std::vector<MountEntry> entries;
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(callback, entries));
  }

  virtual void Format(const std::string& device_path,
                      const std::string& filesystem,
                      const base::Closure& callback,
                      const base::Closure& error_callback) OVERRIDE {
    base::MessageLoopProxy::current()->PostTask(FROM_HERE, error_callback);
  }

  virtual void GetDeviceProperties(
      const std::string& device_path,
      const GetDevicePropertiesCallback& callback,
      const base::Closure& error_callback) OVERRIDE {
    base::MessageLoopProxy::current()->PostTask(FROM_HERE, error_callback);
  }

  virtual void SetMountEventHandler(
      const MountEventHandler& mount_event_handler) OVERRIDE {
    mount_event_handler_ = mount_event_handler;
  }

  virtual void SetMountCompletedHandler(
      const MountCompletedHandler& mount_completed_handler) OVERRIDE {
    mount_completed_handler_ = mount_completed_handler;
  }

  virtual void SetFormatCompletedHandler(
      const FormatCompletedHandler& format_completed_handler) OVERRIDE {
    format_completed_handler_ = format_completed_handler;
  }

 private:
  // Performs file actions for Mount().
  static MountError PerformFakeMount(const std::string& source_path,
                                     const base::FilePath& mounted_path) {
    // Check the source path exists.
    if (!base::PathExists(base::FilePath::FromUTF8Unsafe(source_path))) {
      DLOG(ERROR) << "Source does not exist at " << source_path;
      return MOUNT_ERROR_INVALID_PATH;
    }

    // Just create an empty directory and shows it as the mounted directory.
    if (!base::CreateDirectory(mounted_path)) {
      DLOG(ERROR) << "Failed to create directory at " << mounted_path.value();
      return MOUNT_ERROR_DIRECTORY_CREATION_FAILED;
    }

    // Put a dummy file.
    const base::FilePath dummy_file_path =
        mounted_path.Append("SUCCESSFULLY_PERFORMED_FAKE_MOUNT.txt");
    const std::string dummy_file_content = "This is a dummy file.";
    const int write_result = base::WriteFile(
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
                     const base::Closure& callback,
                     const base::FilePath& mounted_path,
                     MountError mount_error) {
    if (mount_error != MOUNT_ERROR_NONE) {
      FinishMount(mount_error, source_path, type, std::string(), callback);
      return;
    }
    mounted_to_source_path_map_[mounted_path.value()] = source_path;
    FinishMount(MOUNT_ERROR_NONE, source_path, type,
                mounted_path.AsUTF8Unsafe(), callback);
  }

  // Runs |callback| and sends MountCompleted signal.
  // Part of Mount() implementation.
  void FinishMount(MountError error,
                   const std::string& source_path,
                   MountType type,
                   const std::string& mounted_path,
                   const base::Closure& callback) {
    base::MessageLoopProxy::current()->PostTask(FROM_HERE, callback);
    if (!mount_completed_handler_.is_null()) {
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE,
          base::Bind(mount_completed_handler_,
                     MountEntry(error, source_path, type, mounted_path)));
    }
  }

  // Mounted path to source path map.
  std::map<std::string, std::string> mounted_to_source_path_map_;

  MountEventHandler mount_event_handler_;
  MountCompletedHandler mount_completed_handler_;
  FormatCompletedHandler format_completed_handler_;

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
  dbus::MessageReader reader(response);
  scoped_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
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

  // dbus::PopDataAsValue() pops uint64 as double.
  // The top 11 bits of uint64 are dropped by the use of double. But, this works
  // unless the size exceeds 8 PB.
  double device_size_double = 0;
  if (properties->GetDoubleWithoutPathExpansion(cros_disks::kDeviceSize,
                                                &device_size_double))
    total_size_in_bytes_ = device_size_double;

  // dbus::PopDataAsValue() pops uint32 as double.
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
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new CrosDisksClientStubImpl();
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

}  // namespace chromeos
