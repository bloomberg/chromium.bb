// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/arc_obb_mounter_client.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

namespace chromeos {

namespace {

// TODO(hashimoto): Share these constants with the service.
// D-Bus service constants.
const char kArcObbMounterInterface[] = "org.chromium.ArcObbMounterInterface";
const char kArcObbMounterServicePath[] = "/org/chromium/ArcObbMounter";
const char kArcObbMounterServiceName[] = "org.chromium.ArcObbMounter";

// Method names.
const char kMountObbMethod[] = "MountObb";
const char kUnmountObbMethod[] = "UnmountObb";

class ArcObbMounterClientImpl : public ArcObbMounterClient {
 public:
  ArcObbMounterClientImpl() : weak_ptr_factory_(this) {}
  ~ArcObbMounterClientImpl() override {}

  // ArcObbMounterClient override:
  void MountObb(const std::string& obb_file,
                const std::string& mount_path,
                int32_t owner_gid,
                VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(kArcObbMounterInterface, kMountObbMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(obb_file);
    writer.AppendString(mount_path);
    writer.AppendInt32(owner_gid);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ArcObbMounterClientImpl::OnVoidDBusMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UnmountObb(const std::string& mount_path,
                  VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(kArcObbMounterInterface, kUnmountObbMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(mount_path);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ArcObbMounterClientImpl::OnVoidDBusMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 protected:
  // DBusClient override.
  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(kArcObbMounterServiceName,
                                 dbus::ObjectPath(kArcObbMounterServicePath));
  }

 private:
  // Runs the callback with the method call result.
  void OnVoidDBusMethod(VoidDBusMethodCallback callback,
                        dbus::Response* response) {
    std::move(callback).Run(response != nullptr);
  }

  dbus::ObjectProxy* proxy_ = nullptr;

  base::WeakPtrFactory<ArcObbMounterClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcObbMounterClientImpl);
};

}  // namespace

ArcObbMounterClient::ArcObbMounterClient() {}

ArcObbMounterClient::~ArcObbMounterClient() {}

// static
ArcObbMounterClient* ArcObbMounterClient::Create() {
  return new ArcObbMounterClientImpl();
}

}  // namespace chromeos
