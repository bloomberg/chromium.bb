// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"
#include "mojo/services/network/network_context.h"
#include "mojo/services/network/network_service_impl.h"
#include "mojo/services/public/interfaces/profile/profile_service.mojom.h"

namespace {

void OnPathReceived(const base::Closure& callback,
                    base::FilePath* path,
                    const mojo::String& path_as_string) {
  DCHECK(!path_as_string.is_null());
#if defined(OS_POSIX)
  *path = base::FilePath(path_as_string);
#elif defined(OS_WIN)
  *path = base::FilePath::FromUTF8Unsafe(path_as_string);
#else
#error Not implemented
#endif
  callback.Run();
}

// Synchronously creates a new context, using the handle to retrieve services.
// The handle must still be valid when this method returns and no message must
// have been read.
scoped_ptr<mojo::NetworkContext> CreateContext(
    mojo::ScopedMessagePipeHandle* handle) {
  // TODO(qsr): Do not bind a new service provider, but use a synchronous call
  // when http://crbug.com/386485 is implemented.
  // Temporarly bind the handle to a service provider to retrieve a profile
  // service.
  mojo::InterfacePtr<mojo::Shell> shell;
  shell.Bind(handle->Pass());
  mojo::InterfacePtr<mojo::ServiceProvider> service_provider;
  shell->ConnectToApplication("mojo:profile_service", Get(&service_provider));
  mojo::InterfacePtr<mojo::ProfileService> profile_service;
  ConnectToService(service_provider.get(), &profile_service);
  // Unbind the handle to prevent any message to be read.
  *handle = shell.PassMessagePipe();

  // Use a nested message loop to synchronously call a method on the profile
  // service.
  base::MessageLoop::ScopedNestableTaskAllower allow(
      base::MessageLoop::current());
  base::RunLoop run_loop;
  base::FilePath base_path;
  profile_service->GetPath(mojo::ProfileService::DIR_TEMP,
                           base::Bind(&OnPathReceived,
                                      run_loop.QuitClosure(),
                                      base::Unretained(&base_path)));
  run_loop.Run();
  base_path = base_path.Append(FILE_PATH_LITERAL("network_service"));
  return scoped_ptr<mojo::NetworkContext>(new mojo::NetworkContext(base_path));
}

}  // namespace

class Delegate : public mojo::ApplicationDelegate {
 public:
  Delegate(scoped_ptr<mojo::NetworkContext> context)
      : context_(context.Pass()) {}

  virtual bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) MOJO_OVERRIDE {
    connection->AddService<mojo::NetworkServiceImpl>(context_.get());
    return true;
  }

 private:
  scoped_ptr<mojo::NetworkContext> context_;
};

extern "C" APPLICATION_EXPORT MojoResult CDECL MojoMain(
    MojoHandle shell_handle) {
  base::CommandLine::Init(0, NULL);
  base::AtExitManager at_exit;

  // The IO message loop allows us to use net::URLRequest on this thread.
  base::MessageLoopForIO loop;

  mojo::ScopedMessagePipeHandle scoped_shell_handle =
      mojo::MakeScopedHandle(mojo::MessagePipeHandle(shell_handle));
  scoped_ptr<mojo::NetworkContext> context =
      CreateContext(&scoped_shell_handle);

  Delegate delegate(context.Pass());
  mojo::ApplicationImpl app(&delegate, scoped_shell_handle.Pass());

  loop.Run();
  return MOJO_RESULT_OK;
}
