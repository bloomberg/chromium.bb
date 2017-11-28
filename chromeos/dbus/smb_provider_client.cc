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

class SmbProviderClientImpl : public SmbProviderClient {
 public:
  SmbProviderClientImpl() : weak_ptr_factory_(this) {}

  ~SmbProviderClientImpl() override {}

  void Mount(const std::string& share_path, MountCallback callback) override {
    dbus::MethodCall method_call(smbprovider::kSmbProviderInterface,
                                 smbprovider::kMountMethod);
    dbus::MessageWriter writer(&method_call);
    smbprovider::MountOptions mount_options;
    mount_options.set_path(share_path);
    writer.AppendProtoAsArrayOfBytes(mount_options);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&SmbProviderClientImpl::HandleMountCallback,
                   weak_ptr_factory_.GetWeakPtr(), base::Passed(&callback)));
  }

  void Unmount(int32_t mount_id, UnmountCallback callback) override {
    dbus::MethodCall method_call(smbprovider::kSmbProviderInterface,
                                 smbprovider::kUnmountMethod);
    dbus::MessageWriter writer(&method_call);
    smbprovider::UnmountOptions unmount_options;
    unmount_options.set_mount_id(mount_id);
    writer.AppendProtoAsArrayOfBytes(unmount_options);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&SmbProviderClientImpl::HandleUnmountCallback,
                   weak_ptr_factory_.GetWeakPtr(), base::Passed(&callback)));
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
    if (!reader.PopInt32(&mount_id) || mount_id <= 0) {
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

  dbus::ObjectProxy* proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<SmbProviderClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SmbProviderClientImpl);
};

}  // namespace

SmbProviderClient::SmbProviderClient() {}

SmbProviderClient::~SmbProviderClient() {}

// static
SmbProviderClient* SmbProviderClient::Create() {
  return new SmbProviderClientImpl();
}

}  // namespace chromeos
