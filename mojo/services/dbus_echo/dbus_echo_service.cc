// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/file_descriptor.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "mojo/common/channel_init.h"
#include "mojo/embedder/embedder.h"
#include "mojo/public/cpp/bindings/interface.h"
#include "mojo/public/cpp/bindings/remote_ptr.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/shell/application.h"
#include "mojo/public/interfaces/shell/shell.mojom.h"
#include "mojo/services/dbus_echo/echo.mojom.h"
#include "mojo/shell/external_service.mojom.h"

class EchoServiceImpl : public mojo::ServiceConnection<mojo::EchoService,
                                                       EchoServiceImpl> {
 public:
  EchoServiceImpl() {}
  virtual ~EchoServiceImpl() {}

 protected:
  virtual void Echo(
      const mojo::String& in_to_echo,
      const mojo::Callback<void(mojo::String)>& callback) OVERRIDE {
    DVLOG(1) << "Asked to echo " << in_to_echo.To<std::string>();
    callback.Run(in_to_echo);
  }
};


class DBusEchoService : public mojo::ExternalService {
 public:
  static const char kMojoImplPath[];
  static const char kMojoInterface[];
  static const char kServiceName[];

  DBusEchoService() : exported_object_(NULL) {
  }
  ~DBusEchoService() {}

  void Start() {
    InitializeDBus();
    ExportMethods();
    TakeDBusServiceOwnership();
    DVLOG(1) << "Echo service started";
  }

 private:
  // TODO(cmasone): Use a Delegate pattern to allow the ConnectChannel
  // code to be shared among all Mojo services that wish to be
  // connected to over DBus.
  virtual void Activate(mojo::ScopedMessagePipeHandle client_handle) OVERRIDE {
    mojo::ScopedShellHandle shell_handle(
        mojo::ShellHandle(client_handle.release().value()));
    app_.reset(new mojo::Application(shell_handle.Pass()));
    app_->AddServiceConnector(new mojo::ServiceConnector<EchoServiceImpl>());
  }

  // Implementation of org.chromium.Mojo.ConnectChannel, exported over DBus.
  // Takes a file descriptor and uses it to create a MessagePipe that is then
  // hooked to a RemotePtr<mojo::ExternalServiceHost>.
  void ConnectChannel(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender sender) {
    dbus::MessageReader reader(method_call);
    dbus::FileDescriptor wrapped_fd;
    if (!reader.PopFileDescriptor(&wrapped_fd)) {
      sender.Run(
          dbus::ErrorResponse::FromMethodCall(
              method_call,
              "org.chromium.Mojo.BadHandle",
              "Invalid FD.").PassAs<dbus::Response>());
      return;
    }
    wrapped_fd.CheckValidity();
    channel_init_.reset(new mojo::common::ChannelInit);
    mojo::ScopedMessagePipeHandle message_pipe =
        channel_init_->Init(wrapped_fd.TakeValue(),
                            base::MessageLoopProxy::current());
    CHECK(message_pipe.is_valid());

    // TODO(cmasone): Figure out how to correctly clean up the channel when
    // the peer disappears. Passing an ErrorHandler when constructing the
    // RemotePtr<> below didn't seem to work. OnError wasn't called.
    external_service_host_.reset(
        mojo::ScopedExternalServiceHostHandle::From(message_pipe.Pass()),
        this);

    sender.Run(dbus::Response::FromMethodCall(method_call));
  }

  void ExportMethods() {
    CHECK(exported_object_);
    CHECK(exported_object_->ExportMethodAndBlock(
        kMojoInterface, "ConnectChannel",
        base::Bind(&DBusEchoService::ConnectChannel,
                   base::Unretained(this))));
  }

  void InitializeDBus() {
    CHECK(!bus_);
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SESSION;
    bus_ = new dbus::Bus(options);
    CHECK(bus_->Connect());
    CHECK(bus_->SetUpAsyncOperations());

    exported_object_ = bus_->GetExportedObject(dbus::ObjectPath(kMojoImplPath));
  }

  void TakeDBusServiceOwnership() {
    CHECK(bus_->RequestOwnershipAndBlock(
        kServiceName,
        dbus::Bus::REQUIRE_PRIMARY_ALLOW_REPLACEMENT))
        << "Unable to take ownership of " << kServiceName;
  }

  scoped_refptr<dbus::Bus> bus_;
  dbus::ExportedObject* exported_object_;  // Owned by bus_;
  scoped_ptr<mojo::common::ChannelInit> channel_init_;
  mojo::RemotePtr<mojo::ExternalServiceHost> external_service_host_;
  scoped_ptr<mojo::Application> app_;
};

// TODO(cmasone): Figure out where these "well-known" names should be defined
// so they can be shared with DBusServiceLoader and such.
const char DBusEchoService::kMojoImplPath[] = "/org/chromium/MojoImpl";
const char DBusEchoService::kMojoInterface[] = "org.chromium.Mojo";
const char DBusEchoService::kServiceName[] = "org.chromium.EchoService";

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);
  logging::SetLogItems(false,    // Process ID
                       false,    // Thread ID
                       false,    // Timestamp
                       false);   // Tick count

  mojo::Environment env;
  mojo::embedder::Init();

  base::MessageLoopForIO message_loop;
  base::RunLoop run_loop;

  DBusEchoService echo_service;
  echo_service.Start();

  run_loop.Run();
  return 0;
}
