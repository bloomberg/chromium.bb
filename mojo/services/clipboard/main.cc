// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/services/clipboard/clipboard_standalone_impl.h"

class Delegate : public mojo::ApplicationDelegate,
                 public mojo::InterfaceFactory<mojo::Clipboard> {
 public:
  Delegate() {}
  virtual ~Delegate() {}

  // mojo::ApplicationDelegate implementation.
  virtual bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) OVERRIDE {
    connection->AddService(this);
    return true;
  }

  // mojo::InterfaceFactory<mojo::Clipboard> implementation.
  virtual void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<mojo::Clipboard> request) OVERRIDE {
    // TODO(erg): Write native implementations of the clipboard. For now, we
    // just build a clipboard which doesn't interact with the system.
    mojo::BindToRequest(new mojo::ClipboardStandaloneImpl(), &request);
  }
};

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new Delegate);
  return runner.Run(shell_handle);
}
