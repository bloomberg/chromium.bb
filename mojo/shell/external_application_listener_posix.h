// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_EXTERNAL_APPLICATION_LISTENER_POSIX_H_
#define MOJO_SHELL_EXTERNAL_APPLICATION_LISTENER_POSIX_H_

#include "mojo/shell/external_application_listener.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "mojo/edk/embedder/channel_init.h"
#include "mojo/public/interfaces/application/shell.mojom.h"
#include "mojo/shell/external_application_registrar.mojom.h"
#include "mojo/shell/incoming_connection_listener_posix.h"
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
// This class implements most of the mojo_shell side of external application
// registration. It handles:
//  1) discoverability - sets up a unix domain socket at a well-known location,
//  2) incoming connections - listens for and accepts incoming connections on
//     that socket, and
//  3) registration requests - forwarded to a RegisterCallback that implements
//     the actual registration logic.
//
// External applications can connect to the shell using the
// ExternalApplicationRegistrarConnection class.
class ExternalApplicationListenerPosix
    : public ExternalApplicationListener,
      public IncomingConnectionListenerPosix::Delegate {
 public:
  // This class uses two threads, an IO thread for listening and accepting
  // incoming sockets, and a "main" thread where all Mojo traffic is processed
  // and provided callbacks are run.
  ExternalApplicationListenerPosix(
      const scoped_refptr<base::SequencedTaskRunner>& shell_runner,
      const scoped_refptr<base::SequencedTaskRunner>& io_runner);

  // Some of this class' internal state needs to be destroyed on io_runner_,
  // so the destructor will post a task to that thread to call StopListening()
  // and then wait for it to complete.
  ~ExternalApplicationListenerPosix() override;

  // Begin listening (on io_runner) to a socket at listen_socket_path.
  // Incoming registration requests will be forwarded to register_callback.
  // Errors are ignored.
  void ListenInBackground(const base::FilePath& listen_socket_path,
                          const RegisterCallback& register_callback) override;

  // Begin listening (on io_runner) to a socket at listen_socket_path.
  // Incoming registration requests will be forwarded to register_callback.
  // Errors are reported via error_callback.
  void ListenInBackgroundWithErrorCallback(
      const base::FilePath& listen_socket_path,
      const RegisterCallback& register_callback,
      const ErrorCallback& error_callback) override;

  // Block the current thread until listening has started on io_runner.
  // If listening has already started, returns immediately.
  void WaitForListening() override;

 private:
  class RegistrarImpl;

  // MUST be called on io_runner.
  // Creates listener_ and tells it to StartListening() on a socket it creates
  // at listen_socket_path.
  void StartListening(const base::FilePath& listen_socket_path);

  // MUST be called on io_runner.
  // Destroys listener_ and signals event when done.
  void StopListening(base::WaitableEvent* event);

  // Implementation of IncomingConnectionListener::Delegate
  void OnListening(int rv) override;
  void OnConnection(net::SocketDescriptor incoming) override;

  // If listener_ fails to start listening, this method is run on shell_runner_
  // to report the error.
  void RunErrorCallbackIfListeningFailed(int rv);

  // When a connection succeeds, it is passed to this method running
  // on shell_runner_, where it is "promoted" to a Mojo MessagePipe and
  // bound to a RegistrarImpl.
  void CreatePipeAndBindToRegistrarImpl(net::SocketDescriptor incoming_socket);

  scoped_refptr<base::SequencedTaskRunner> shell_runner_;
  scoped_refptr<base::SequencedTaskRunner> io_runner_;

  // MUST be created, used, and destroyed on io_runner_.
  scoped_ptr<IncomingConnectionListenerPosix> listener_;

  // Callers can wait on this event, which will be signalled once listening
  // has either successfully begun or definitively failed.
  base::WaitableEvent signal_on_listening_;

  // Locked to thread on which StartListening() is run (should be io_runner_).
  // All methods that touch listener_ check that they're on that same thread.
  base::ThreadChecker listener_thread_checker_;

  ErrorCallback error_callback_;
  RegisterCallback register_callback_;
  base::ThreadChecker register_thread_checker_;

  // Used on shell_runner_.
  base::WeakPtrFactory<ExternalApplicationListenerPosix> weak_ptr_factory_;
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_EXTERNAL_APPLICATION_LISTENER_POSIX_H_
