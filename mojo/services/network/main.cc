// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"
#include "mojo/services/network/network_context.h"
#include "mojo/services/network/network_service_impl.h"
#include "mojo/services/public/interfaces/profile/profile_service.mojom.h"

namespace {

void OnPathReceived(base::FilePath* path, const mojo::String& path_as_string) {
  DCHECK(!path_as_string.is_null());
#if defined(OS_POSIX)
  *path = base::FilePath(path_as_string);
#elif defined(OS_WIN)
  *path = base::FilePath::FromUTF8Unsafe(path_as_string);
#else
#error Not implemented
#endif
}

}  // namespace

class Delegate : public mojo::ApplicationDelegate {
 public:
  Delegate() {}

  virtual void Initialize(mojo::ApplicationImpl* app) MOJO_OVERRIDE {
    mojo::InterfacePtr<mojo::ProfileService> profile_service;
    app->ConnectToService("mojo:profile_service", &profile_service);
    base::FilePath base_path;
    profile_service->GetPath(
        mojo::ProfileService::PATH_KEY_DIR_TEMP,
        base::Bind(&OnPathReceived, base::Unretained(&base_path)));
    profile_service.WaitForIncomingMethodCall();
    DCHECK(!base_path.value().empty());
    base_path = base_path.Append(FILE_PATH_LITERAL("network_service"));
    context_.reset(new mojo::NetworkContext(base_path));
  }

  virtual bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) MOJO_OVERRIDE {
    DCHECK(context_);
    connection->AddService<mojo::NetworkServiceImpl>(context_.get());
    return true;
  }

 private:
  scoped_ptr<mojo::NetworkContext> context_;
};

extern "C" APPLICATION_EXPORT MojoResult CDECL MojoMain(
    MojoHandle shell_handle) {
  base::CommandLine::Init(0, NULL);
#if !defined(COMPONENT_BUILD)
  base::AtExitManager at_exit;
#endif

  // The IO message loop allows us to use net::URLRequest on this thread.
  base::MessageLoopForIO loop;

  Delegate delegate;
  mojo::ApplicationImpl app(
      &delegate, mojo::MakeScopedHandle(mojo::MessagePipeHandle(shell_handle)));

  loop.Run();
  return MOJO_RESULT_OK;
}
