// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/media_transfer_protocol_daemon_client.h"

#include <map>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Pops a string value when |reader| is not NULL.
// Returns true when a value is popped, false otherwise.
bool MaybePopString(dbus::MessageReader* reader, std::string* value) {
  if (!reader)
    return false;
  return reader->PopString(value);
}

// Pops a uint16 value when |reader| is not NULL.
// Returns true when a value is popped, false otherwise.
bool MaybePopUint16(dbus::MessageReader* reader, uint16* value) {
  if (!reader)
    return false;
  return reader->PopUint16(value);
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

// Pops a int64 value when |reader| is not NULL.
// Returns true when a value is popped, false otherwise.
bool MaybePopInt64(dbus::MessageReader* reader, int64* value) {
  if (!reader)
    return false;
  return reader->PopInt64(value);
}

// The MediaTransferProtocolDaemonClient implementation.
class MediaTransferProtocolDaemonClientImpl
    : public MediaTransferProtocolDaemonClient {
 public:
  explicit MediaTransferProtocolDaemonClientImpl(dbus::Bus* bus)
      : proxy_(bus->GetObjectProxy(
          mtpd::kMtpdServiceName,
          dbus::ObjectPath(mtpd::kMtpdServicePath))),
        weak_ptr_factory_(this) {
  }

  // MediaTransferProtocolDaemonClient override.
  virtual void EnumerateStorage(const EnumerateStorageCallback& callback,
                                const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(mtpd::kMtpdInterface, mtpd::kEnumerateStorage);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&MediaTransferProtocolDaemonClientImpl::OnEnumerateStorage,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback,
                   error_callback));
  }

  // MediaTransferProtocolDaemonClient override.
  virtual void GetStorageInfo(const std::string& storage_name,
                              const GetStorageInfoCallback& callback,
                              const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(mtpd::kMtpdInterface, mtpd::kGetStorageInfo);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(storage_name);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&MediaTransferProtocolDaemonClientImpl::OnGetStorageInfo,
                   weak_ptr_factory_.GetWeakPtr(),
                   storage_name,
                   callback,
                   error_callback));
  }

  // MediaTransferProtocolDaemonClient override.
  virtual void OpenStorage(const std::string& storage_name,
                           OpenStorageMode mode,
                           const OpenStorageCallback& callback,
                           const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(mtpd::kMtpdInterface, mtpd::kOpenStorage);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(storage_name);
    DCHECK_EQ(OPEN_STORAGE_MODE_READ_ONLY, mode);
    writer.AppendString(mtpd::kReadOnlyMode);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&MediaTransferProtocolDaemonClientImpl::OnOpenStorage,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback,
                   error_callback));
  }

  // MediaTransferProtocolDaemonClient override.
  virtual void CloseStorage(const std::string& handle,
                            const CloseStorageCallback& callback,
                            const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(mtpd::kMtpdInterface, mtpd::kCloseStorage);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(handle);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&MediaTransferProtocolDaemonClientImpl::OnCloseStorage,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback,
                   error_callback));
  }

  // MediaTransferProtocolDaemonClient override.
  virtual void ReadDirectoryByPath(
      const std::string& handle,
      const std::string& path,
      const ReadDirectoryCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(mtpd::kMtpdInterface,
                                 mtpd::kReadDirectoryByPath);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(handle);
    writer.AppendString(path);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&MediaTransferProtocolDaemonClientImpl::OnReadDirectory,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback,
                   error_callback));
  }

  // MediaTransferProtocolDaemonClient override.
  virtual void ReadDirectoryById(
      const std::string& handle,
      uint32 file_id,
      const ReadDirectoryCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(mtpd::kMtpdInterface,
                                 mtpd::kReadDirectoryById);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(handle);
    writer.AppendUint32(file_id);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&MediaTransferProtocolDaemonClientImpl::OnReadDirectory,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback,
                   error_callback));
  }

  // MediaTransferProtocolDaemonClient override.
  virtual void ReadFileByPath(const std::string& handle,
                              const std::string& path,
                              const ReadFileCallback& callback,
                              const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(mtpd::kMtpdInterface, mtpd::kReadFileByPath);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(handle);
    writer.AppendString(path);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&MediaTransferProtocolDaemonClientImpl::OnReadFile,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback,
                   error_callback));
  }

  // MediaTransferProtocolDaemonClient override.
  virtual void ReadFileById(const std::string& handle,
                            uint32 file_id,
                            const ReadFileCallback& callback,
                            const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(mtpd::kMtpdInterface, mtpd::kReadFileById);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(handle);
    writer.AppendUint32(file_id);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&MediaTransferProtocolDaemonClientImpl::OnReadFile,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback,
                   error_callback));
  }

  // MediaTransferProtocolDaemonClient override.
  virtual void GetFileInfoByPath(const std::string& handle,
                                 const std::string& path,
                                 const GetFileInfoCallback& callback,
                                 const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(mtpd::kMtpdInterface, "GetFileInfoByPath");
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(handle);
    writer.AppendString(path);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&MediaTransferProtocolDaemonClientImpl::OnGetFileInfo,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback,
                   error_callback));
  }

  // MediaTransferProtocolDaemonClient override.
  virtual void GetFileInfoById(const std::string& handle,
                               uint32 file_id,
                               const GetFileInfoCallback& callback,
                               const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(mtpd::kMtpdInterface, "GetFileInfoById");
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(handle);
    writer.AppendUint32(file_id);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&MediaTransferProtocolDaemonClientImpl::OnGetFileInfo,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback,
                   error_callback));
  }

  // MediaTransferProtocolDaemonClient override.
  virtual void SetUpConnections(
      const MTPStorageEventHandler& handler) OVERRIDE {
    static const SignalEventTuple kSignalEventTuples[] = {
      { mtpd::kMTPStorageAttached, true },
      { mtpd::kMTPStorageDetached, false },
    };
    const size_t kNumSignalEventTuples = arraysize(kSignalEventTuples);

    for (size_t i = 0; i < kNumSignalEventTuples; ++i) {
      proxy_->ConnectToSignal(
          mtpd::kMtpdInterface,
          kSignalEventTuples[i].signal_name,
          base::Bind(&MediaTransferProtocolDaemonClientImpl::OnMTPStorageSignal,
                     weak_ptr_factory_.GetWeakPtr(),
                     handler,
                     kSignalEventTuples[i].is_attach),
          base::Bind(&MediaTransferProtocolDaemonClientImpl::OnSignalConnected,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }

 private:
  // A struct to contain a pair of signal name and attachment event type.
  // Used by SetUpConnections.
  struct SignalEventTuple {
    const char *signal_name;
    bool is_attach;
  };

  // Handles the result of EnumerateStorage and calls |callback| or
  // |error_callback|.
  void OnEnumerateStorage(const EnumerateStorageCallback& callback,
                          const ErrorCallback& error_callback,
                          dbus::Response* response) {
    if (!response) {
      error_callback.Run();
      return;
    }
    dbus::MessageReader reader(response);
    std::vector<std::string> storage_names;
    if (!reader.PopArrayOfStrings(&storage_names)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      error_callback.Run();
      return;
    }
    callback.Run(storage_names);
  }

  // Handles the result of GetStorageInfo and calls |callback| or
  // |error_callback|.
  void OnGetStorageInfo(const std::string& storage_name,
                        const GetStorageInfoCallback& callback,
                        const ErrorCallback& error_callback,
                        dbus::Response* response) {
    if (!response) {
      error_callback.Run();
      return;
    }
    StorageInfo storage_info(storage_name, response);
    callback.Run(storage_info);
  }

  // Handles the result of OpenStorage and calls |callback| or |error_callback|.
  void OnOpenStorage(const OpenStorageCallback& callback,
                     const ErrorCallback& error_callback,
                     dbus::Response* response) {
    if (!response) {
      error_callback.Run();
      return;
    }
    dbus::MessageReader reader(response);
    std::string handle;
    if (!reader.PopString(&handle)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      error_callback.Run();
      return;
    }
    callback.Run(handle);
  }

  // Handles the result of CloseStorage and calls |callback| or
  // |error_callback|.
  void OnCloseStorage(const CloseStorageCallback& callback,
                      const ErrorCallback& error_callback,
                      dbus::Response* response) {
    if (!response) {
      error_callback.Run();
      return;
    }
    callback.Run();
  }

  // Handles the result of ReadDirectoryByPath/Id and calls |callback| or
  // |error_callback|.
  void OnReadDirectory(const ReadDirectoryCallback& callback,
                       const ErrorCallback& error_callback,
                       dbus::Response* response) {
    if (!response) {
      error_callback.Run();
      return;
    }

    std::vector<FileEntry> file_entries;
    dbus::MessageReader response_reader(response);
    dbus::MessageReader array_reader(response);
    if (!response_reader.PopArray(&array_reader)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      error_callback.Run();
      return;
    }
    while (array_reader.HasMoreData()) {
      FileEntry entry(response);
      file_entries.push_back(entry);
    }
    callback.Run(file_entries);
  }

  // Handles the result of ReadFileByPath/Id and calls |callback| or
  // |error_callback|.
  void OnReadFile(const ReadFileCallback& callback,
                  const ErrorCallback& error_callback,
                  dbus::Response* response) {
    if (!response) {
      error_callback.Run();
      return;
    }

    uint8* data_bytes = NULL;
    size_t data_length = 0;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytes(&data_bytes, &data_length)) {
      error_callback.Run();
      return;
    }
    std::string data(reinterpret_cast<const char*>(data_bytes), data_length);
    callback.Run(data);
  }

  // Handles the result of GetFileInfoByPath/Id and calls |callback| or
  // |error_callback|.
  void OnGetFileInfo(const GetFileInfoCallback& callback,
                     const ErrorCallback& error_callback,
                     dbus::Response* response) {
    if (!response) {
      error_callback.Run();
      return;
    }

    FileEntry file_entry(response);
    callback.Run(file_entry);
  }

  // Handles MTPStorageAttached/Dettached signals and calls |handler|.
  void OnMTPStorageSignal(MTPStorageEventHandler handler,
                          bool is_attach,
                          dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    std::string storage_name;
    if (!reader.PopString(&storage_name)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    DCHECK(!storage_name.empty());
    handler.Run(is_attach, storage_name);
  }


  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool successed) {
    LOG_IF(ERROR, !successed) << "Connect to " << interface << " "
                              << signal << " failed.";
  }

  dbus::ObjectProxy* proxy_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<MediaTransferProtocolDaemonClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaTransferProtocolDaemonClientImpl);
};

// A stub implementaion of MediaTransferProtocolDaemonClient.
class MediaTransferProtocolDaemonClientStubImpl
    : public MediaTransferProtocolDaemonClient {
 public:
  MediaTransferProtocolDaemonClientStubImpl() {}
  virtual ~MediaTransferProtocolDaemonClientStubImpl() {}

  virtual void EnumerateStorage(
      const EnumerateStorageCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {}
  virtual void GetStorageInfo(
      const std::string& storage_name,
      const GetStorageInfoCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {}
  virtual void OpenStorage(const std::string& storage_name,
                           OpenStorageMode mode,
                           const OpenStorageCallback& callback,
                           const ErrorCallback& error_callback) OVERRIDE {}
  virtual void CloseStorage(const std::string& handle,
                            const CloseStorageCallback& callback,
                            const ErrorCallback& error_callback) OVERRIDE {}
  virtual void ReadDirectoryByPath(
      const std::string& handle,
      const std::string& path,
      const ReadDirectoryCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {}
  virtual void ReadDirectoryById(
      const std::string& handle,
      uint32 file_id,
      const ReadDirectoryCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {}
  virtual void ReadFileByPath(const std::string& handle,
                              const std::string& path,
                              const ReadFileCallback& callback,
                              const ErrorCallback& error_callback) OVERRIDE {}
  virtual void ReadFileById(const std::string& handle,
                            uint32 file_id,
                            const ReadFileCallback& callback,
                            const ErrorCallback& error_callback) OVERRIDE {}
  virtual void GetFileInfoByPath(
      const std::string& handle,
      const std::string& path,
      const GetFileInfoCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {}
  virtual void GetFileInfoById(const std::string& handle,
                               uint32 file_id,
                               const GetFileInfoCallback& callback,
                               const ErrorCallback& error_callback) OVERRIDE {}
  virtual void SetUpConnections(
      const MTPStorageEventHandler& handler) OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaTransferProtocolDaemonClientStubImpl);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// StorageInfo

StorageInfo::StorageInfo()
    : vendor_id_(0),
      product_id_(0),
      device_flags_(0),
      storage_type_(0),
      filesystem_type_(0),
      access_capability_(0),
      max_capacity_(0),
      free_space_in_bytes_(0),
      free_space_in_objects_(0) {
}

StorageInfo::StorageInfo(const std::string& storage_name,
                         dbus::Response* response)
    : vendor_id_(0),
      product_id_(0),
      device_flags_(0),
      storage_name_(storage_name),
      storage_type_(0),
      filesystem_type_(0),
      access_capability_(0),
      max_capacity_(0),
      free_space_in_bytes_(0),
      free_space_in_objects_(0) {
  InitializeFromResponse(response);
}

StorageInfo::~StorageInfo() {
}

// Initializes |this| from |response| given by the mtpd service.
void StorageInfo::InitializeFromResponse(dbus::Response* response) {
  dbus::MessageReader response_reader(response);
  dbus::MessageReader array_reader(response);
  if (!response_reader.PopArray(&array_reader)) {
    LOG(ERROR) << "Invalid response: " << response->ToString();
    return;
  }
  // TODO(thestig): Rework this code using Protocol Buffers. crosbug.com/22626
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
  // TODO(thestig) Add enums for fields below as appropriate.
  MaybePopString(properties[mtpd::kVendor], &vendor_);
  MaybePopString(properties[mtpd::kProduct], &product_);
  MaybePopString(properties[mtpd::kStorageDescription], &storage_description_);
  MaybePopString(properties[mtpd::kVolumeIdentifier], &volume_identifier_);
  MaybePopUint16(properties[mtpd::kVendorId], &vendor_id_);
  MaybePopUint16(properties[mtpd::kProductId], &product_id_);
  MaybePopUint16(properties[mtpd::kStorageType], &storage_type_);
  MaybePopUint16(properties[mtpd::kFilesystemType], &filesystem_type_);
  MaybePopUint16(properties[mtpd::kAccessCapability], &access_capability_);
  MaybePopUint32(properties[mtpd::kDeviceFlags], &device_flags_);
  MaybePopUint64(properties[mtpd::kMaxCapacity], &max_capacity_);
  MaybePopUint64(properties[mtpd::kFreeSpaceInBytes], &free_space_in_bytes_);
  MaybePopUint64(properties[mtpd::kFreeSpaceInObjects],
                 &free_space_in_objects_);
}

////////////////////////////////////////////////////////////////////////////////
// FileEntry

FileEntry::FileEntry()
    : item_id_(0),
      parent_id_(0),
      file_size_(0),
      file_type_(FILE_TYPE_UNKNOWN) {
}

FileEntry::FileEntry(dbus::Response* response)
    : item_id_(0),
      parent_id_(0),
      file_size_(0),
      file_type_(FILE_TYPE_UNKNOWN) {
  InitializeFromResponse(response);
}

FileEntry::~FileEntry() {
}

// Initializes |this| from |response| given by the mtpd service.
void FileEntry::InitializeFromResponse(dbus::Response* response) {
  dbus::MessageReader response_reader(response);
  dbus::MessageReader array_reader(response);
  if (!response_reader.PopArray(&array_reader)) {
    LOG(ERROR) << "Invalid response: " << response->ToString();
    return;
  }
  // TODO(thestig): Rework this code using Protocol Buffers. crosbug.com/22626
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

  MaybePopString(properties[mtpd::kFileName], &file_name_);
  MaybePopUint32(properties[mtpd::kItemId], &item_id_);
  MaybePopUint32(properties[mtpd::kParentId], &parent_id_);
  MaybePopUint64(properties[mtpd::kFileSize], &file_size_);

  int64 modification_date = -1;
  if (MaybePopInt64(properties[mtpd::kModificationDate], &modification_date))
    modification_date_ = base::Time::FromTimeT(modification_date);

  uint16 file_type = FILE_TYPE_OTHER;
  if (MaybePopUint16(properties[mtpd::kFileType], &file_type)) {
    switch (file_type) {
      case FILE_TYPE_FOLDER:
      case FILE_TYPE_JPEG:
      case FILE_TYPE_JFIF:
      case FILE_TYPE_TIFF:
      case FILE_TYPE_BMP:
      case FILE_TYPE_GIF:
      case FILE_TYPE_PICT:
      case FILE_TYPE_PNG:
      case FILE_TYPE_WINDOWSIMAGEFORMAT:
      case FILE_TYPE_JP2:
      case FILE_TYPE_JPX:
      case FILE_TYPE_UNKNOWN:
        file_type_ = static_cast<FileType>(file_type);
        break;
      default:
        file_type_ = FILE_TYPE_OTHER;
        break;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// MediaTransferProtocolDaemonClient

MediaTransferProtocolDaemonClient::MediaTransferProtocolDaemonClient() {}

MediaTransferProtocolDaemonClient::~MediaTransferProtocolDaemonClient() {}

// static
MediaTransferProtocolDaemonClient*
MediaTransferProtocolDaemonClient::Create(DBusClientImplementationType type,
                                          dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new MediaTransferProtocolDaemonClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new MediaTransferProtocolDaemonClientStubImpl();
}

}  // namespace chromeos
