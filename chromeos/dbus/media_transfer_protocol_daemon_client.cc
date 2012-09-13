// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/media_transfer_protocol_daemon_client.h"

#include "base/bind.h"
#include "chromeos/dbus/mtp_file_entry.pb.h"
#include "chromeos/dbus/mtp_storage_info.pb.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

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

    dbus::MessageReader reader(response);
    MtpStorageInfo protobuf;
    if (!reader.PopArrayOfBytesAsProto(&protobuf)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      error_callback.Run();
      return;
    }
    callback.Run(protobuf);
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

    std::vector<MtpFileEntry> file_entries;
    dbus::MessageReader reader(response);
    MtpFileEntries entries_protobuf;
    if (!reader.PopArrayOfBytesAsProto(&entries_protobuf)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      error_callback.Run();
      return;
    }

    for (int i = 0; i < entries_protobuf.file_entries_size(); ++i)
      file_entries.push_back(entries_protobuf.file_entries(i));
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

    dbus::MessageReader reader(response);
    MtpFileEntry protobuf;
    if (!reader.PopArrayOfBytesAsProto(&protobuf)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      error_callback.Run();
      return;
    }
    callback.Run(protobuf);
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
