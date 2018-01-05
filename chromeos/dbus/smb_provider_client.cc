// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/smb_provider_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/weak_ptr.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

namespace chromeos {

namespace {

smbprovider::ErrorType GetErrorFromReader(dbus::MessageReader* reader) {
  int32_t int_error;
  if (!reader->PopInt32(&int_error) ||
      !smbprovider::ErrorType_IsValid(int_error)) {
    DLOG(ERROR)
        << "SmbProviderClient: Failed to get an error from the response";
    return smbprovider::ERROR_DBUS_PARSE_FAILED;
  }
  return static_cast<smbprovider::ErrorType>(int_error);
}

smbprovider::ErrorType GetErrorAndProto(
    dbus::Response* response,
    google::protobuf::MessageLite* protobuf_out) {
  if (!response) {
    DLOG(ERROR) << "Failed to call smbprovider";
    return smbprovider::ERROR_DBUS_PARSE_FAILED;
  }
  dbus::MessageReader reader(response);
  smbprovider::ErrorType error = GetErrorFromReader(&reader);
  if (error != smbprovider::ERROR_OK) {
    return error;
  }
  if (!reader.PopArrayOfBytesAsProto(protobuf_out)) {
    DLOG(ERROR) << "Failed to parse protobuf.";
    return smbprovider::ERROR_DBUS_PARSE_FAILED;
  }
  return smbprovider::ERROR_OK;
}

class SmbProviderClientImpl : public SmbProviderClient {
 public:
  SmbProviderClientImpl() : weak_ptr_factory_(this) {}

  ~SmbProviderClientImpl() override {}

  void Mount(const base::FilePath& share_path,
             MountCallback callback) override {
    smbprovider::MountOptions options;
    options.set_path(share_path.value());
    CallMethod(smbprovider::kMountMethod, options,
               &SmbProviderClientImpl::HandleMountCallback, &callback);
  }

  void Unmount(int32_t mount_id, UnmountCallback callback) override {
    smbprovider::UnmountOptions options;
    options.set_mount_id(mount_id);
    CallMethod(smbprovider::kUnmountMethod, options,
               &SmbProviderClientImpl::HandleUnmountCallback, &callback);
  }

  void ReadDirectory(int32_t mount_id,
                     const base::FilePath& directory_path,
                     ReadDirectoryCallback callback) override {
    smbprovider::ReadDirectoryOptions options;
    options.set_mount_id(mount_id);
    options.set_directory_path(directory_path.value());
    CallMethod(smbprovider::kReadDirectoryMethod, options,
               &SmbProviderClientImpl::HandleProtoCallback<
                   smbprovider::DirectoryEntryList>,
               &callback);
  }

  void GetMetadataEntry(int32_t mount_id,
                        const base::FilePath& entry_path,
                        GetMetdataEntryCallback callback) override {
    smbprovider::GetMetadataEntryOptions options;
    options.set_mount_id(mount_id);
    options.set_entry_path(entry_path.value());
    CallMethod(smbprovider::kGetMetadataEntryMethod, options,
               &SmbProviderClientImpl::HandleProtoCallback<
                   smbprovider::DirectoryEntry>,
               &callback);
  }

  void OpenFile(int32_t mount_id,
                const base::FilePath& file_path,
                bool writeable,
                OpenFileCallback callback) override {
    smbprovider::OpenFileOptions options;
    options.set_mount_id(mount_id);
    options.set_file_path(file_path.value());
    options.set_writeable(writeable);
    CallMethod(smbprovider::kOpenFileMethod, options,
               &SmbProviderClientImpl::HandleOpenFileCallback, &callback);
  }

  void CloseFile(int32_t mount_id,
                 int32_t file_id,
                 CloseFileCallback callback) override {
    smbprovider::CloseFileOptions options;
    options.set_mount_id(mount_id);
    options.set_file_id(file_id);
    CallMethod(smbprovider::kCloseFileMethod, options,
               &SmbProviderClientImpl::HandleCloseFileCallback, &callback);
  }

 protected:
  // DBusClient override.
  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(
        smbprovider::kSmbProviderServiceName,
        dbus::ObjectPath(smbprovider::kSmbProviderServicePath));
    DCHECK(proxy_);
  }

 private:
  // Calls the DBUS method |name|, passing the |protobuf| as an argument.
  // |handler| is the member function in this class that receives
  // the response and then passes the processed response to |callback|.
  template <typename CallbackHandler, typename Callback>
  void CallMethod(const char* name,
                  const google::protobuf::MessageLite& protobuf,
                  CallbackHandler handler,
                  Callback callback) {
    dbus::MethodCall method_call(smbprovider::kSmbProviderInterface, name);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(protobuf);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(handler, weak_ptr_factory_.GetWeakPtr(),
                                  base::Passed(callback)));
  }

  // Handles D-Bus callback for mount.
  void HandleMountCallback(MountCallback callback, dbus::Response* response) {
    if (!response) {
      DLOG(ERROR) << "Mount: failed to call smbprovider";
      std::move(callback).Run(smbprovider::ERROR_DBUS_PARSE_FAILED, -1);
      return;
    }
    dbus::MessageReader reader(response);
    smbprovider::ErrorType error = GetErrorFromReader(&reader);
    if (error != smbprovider::ERROR_OK) {
      std::move(callback).Run(error, -1);
      return;
    }
    int32_t mount_id = -1;
    if (!reader.PopInt32(&mount_id) || mount_id < 0) {
      DLOG(ERROR) << "Mount: failed to parse mount id";
      std::move(callback).Run(smbprovider::ERROR_DBUS_PARSE_FAILED, -1);
      return;
    }
    std::move(callback).Run(smbprovider::ERROR_OK, mount_id);
  }

  // Handles D-Bus callback for unmount.
  void HandleUnmountCallback(UnmountCallback callback,
                             dbus::Response* response) {
    if (!response) {
      DLOG(ERROR) << "Unmount: failed to call smbprovider";
      std::move(callback).Run(smbprovider::ERROR_DBUS_PARSE_FAILED);
    }
    dbus::MessageReader reader(response);
    std::move(callback).Run(GetErrorFromReader(&reader));
  }

  // Handles D-Bus callback for OpenFile.
  void HandleOpenFileCallback(OpenFileCallback callback,
                              dbus::Response* response) {
    if (!response) {
      DLOG(ERROR) << "OpenFile: failed to call smbprovider";
      std::move(callback).Run(smbprovider::ERROR_DBUS_PARSE_FAILED, -1);
      return;
    }
    dbus::MessageReader reader(response);
    smbprovider::ErrorType error = GetErrorFromReader(&reader);
    if (error != smbprovider::ERROR_OK) {
      std::move(callback).Run(error, -1);
      return;
    }
    int32_t file_id = -1;
    if (!reader.PopInt32(&file_id) || file_id < 0) {
      LOG(ERROR) << "OpenFile: failed to parse mount id";
      std::move(callback).Run(smbprovider::ERROR_DBUS_PARSE_FAILED, -1);
      return;
    }
    std::move(callback).Run(smbprovider::ERROR_OK, file_id);
  }

  // Handles D-Bus callback for CloseFile.
  void HandleCloseFileCallback(CloseFileCallback callback,
                               dbus::Response* response) {
    if (!response) {
      DLOG(ERROR) << "CloseFile: failed to call smbprovider";
      std::move(callback).Run(smbprovider::ERROR_DBUS_PARSE_FAILED);
    }
    dbus::MessageReader reader(response);
    std::move(callback).Run(GetErrorFromReader(&reader));
  }

  // Handles D-Bus responses for methods that return an error and a protobuf
  // object.
  template <class T>
  void HandleProtoCallback(base::OnceCallback<void(smbprovider::ErrorType error,
                                                   const T& response)> callback,
                           dbus::Response* response) {
    T proto;
    smbprovider::ErrorType error(GetErrorAndProto(response, &proto));
    std::move(callback).Run(error, proto);
  }

  dbus::ObjectProxy* proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<SmbProviderClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SmbProviderClientImpl);
};

}  // namespace

SmbProviderClient::SmbProviderClient() = default;

SmbProviderClient::~SmbProviderClient() = default;

// static
SmbProviderClient* SmbProviderClient::Create() {
  return new SmbProviderClientImpl();
}

}  // namespace chromeos
