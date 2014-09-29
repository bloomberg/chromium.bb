// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_EXTERNAL_APPLICATION_LISTENER_H_
#define MOJO_SHELL_EXTERNAL_APPLICATION_LISTENER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/socket/socket_descriptor.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

// In order to support Mojo apps whose lifetime is managed by
// something other than mojo_shell, mojo_shell needs to support a
// mechanism by which such an application can discover a running shell
// instance, connect to it, and ask to be "registered" at a given
// URL. Registration implies that the app can be connected to at that
// URL from then on out, and that the app has received a usable ShellPtr.
//
// External applications can connect to the shell using the
// ExternalApplicationRegistrarConnection class.
class ExternalApplicationListener {
 public:
  // When run, a RegisterCallback should note that an app has asked to be
  // registered at app_url and Bind the provided pipe handle to a ShellImpl.
  typedef base::Callback<void(const GURL& app_url,
                              ScopedMessagePipeHandle shell)> RegisterCallback;
  typedef base::Callback<void(int rv)> ErrorCallback;

  virtual ~ExternalApplicationListener() {}

  // Implementations of this class may use two threads, an IO thread for
  // listening and accepting incoming sockets, and a "main" thread
  // where all Mojo traffic is processed and provided callbacks are run.
  static scoped_ptr<ExternalApplicationListener> Create(
      const scoped_refptr<base::SequencedTaskRunner>& shell_runner,
      const scoped_refptr<base::SequencedTaskRunner>& io_runner);

  static base::FilePath ConstructDefaultSocketPath();

  // Begin listening (on io_runner) to a socket at listen_socket_path.
  // Incoming registration requests will be forwarded to register_callback.
  // Errors are ignored.
  virtual void ListenInBackground(
      const base::FilePath& listen_socket_path,
      const RegisterCallback& register_callback) = 0;

  // Begin listening (on io_runner) to a socket at listen_socket_path.
  // Incoming registration requests will be forwarded to register_callback.
  // Errors are reported via error_callback.
  virtual void ListenInBackgroundWithErrorCallback(
      const base::FilePath& listen_socket_path,
      const RegisterCallback& register_callback,
      const ErrorCallback& error_callback) = 0;

  // Block the current thread until listening has started on io_runner.
  // If listening has already started, returns immediately.
  virtual void WaitForListening() = 0;
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_EXTERNAL_APPLICATION_LISTENER_H_
