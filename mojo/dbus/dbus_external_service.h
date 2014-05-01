// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "mojo/common/channel_init.h"
#include "mojo/public/cpp/bindings/interface.h"
#include "mojo/public/cpp/bindings/remote_ptr.h"
#include "mojo/public/cpp/shell/application.h"
#include "mojo/public/interfaces/shell/shell.mojom.h"
#include "mojo/shell/external_service.mojom.h"

namespace mojo {
const char kMojoDBusImplPath[] = "/org/chromium/MojoImpl";
const char kMojoDBusInterface[] = "org.chromium.Mojo";
const char kMojoDBusConnectMethod[] = "ConnectChannel";

class DBusExternalServiceBase : public mojo::ErrorHandler {
 public:
  explicit DBusExternalServiceBase(const std::string& service_name);
  virtual ~DBusExternalServiceBase();

  void Start();

 protected:
  // TODO(cmasone): Enable multiple peers to connect/disconnect
  virtual void Connect(ScopedExternalServiceHostHandle client_handle) = 0;
  virtual void Disconnect() = 0;

 private:
  virtual void OnError() OVERRIDE;

  // Implementation of org.chromium.Mojo.ConnectChannel, exported over DBus.
  // Takes a file descriptor and uses it to create a MessagePipe that is then
  // hooked to a RemotePtr<mojo::ExternalServiceHost>.
  void ConnectChannel(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender sender);

  void ExportMethods();
  void InitializeDBus();
  void TakeDBusServiceOwnership();

  const std::string service_name_;
  scoped_refptr<dbus::Bus> bus_;
  dbus::ExportedObject* exported_object_;  // Owned by bus_;
  scoped_ptr<mojo::common::ChannelInit> channel_init_;
  DISALLOW_COPY_AND_ASSIGN(DBusExternalServiceBase);
};

template <class ServiceImpl>
class DBusExternalService : public DBusExternalServiceBase,
                            public mojo::ExternalService {
 public:
  explicit DBusExternalService(const std::string& service_name)
      : DBusExternalServiceBase(service_name) {
  }
  virtual ~DBusExternalService() {}

 protected:
  virtual void Connect(ScopedExternalServiceHostHandle client_handle) OVERRIDE {
    external_service_host_.reset(client_handle.Pass(), this, this);
  }

  virtual void Disconnect() OVERRIDE {
    app_.reset();
    external_service_host_.reset();
  }

 private:
  virtual void Activate(mojo::ScopedMessagePipeHandle client_handle) OVERRIDE {
    mojo::ScopedShellHandle shell_handle(
        mojo::ShellHandle(client_handle.release().value()));
    app_.reset(new mojo::Application(shell_handle.Pass()));
    app_->AddServiceConnector(new mojo::ServiceConnector<ServiceImpl>());
  }

  mojo::RemotePtr<mojo::ExternalServiceHost> external_service_host_;
  scoped_ptr<mojo::Application> app_;
};

}  // namespace mojo
