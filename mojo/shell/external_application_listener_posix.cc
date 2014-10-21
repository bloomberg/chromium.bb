// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/external_application_listener_posix.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "base/tracked_objects.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/edk/embedder/channel_init.h"
#include "mojo/public/cpp/bindings/error_handler.h"
#include "mojo/shell/external_application_registrar.mojom.h"
#include "mojo/shell/incoming_connection_listener_posix.h"
#include "net/base/net_errors.h"
#include "net/socket/socket_descriptor.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

namespace {
const char kDefaultListenSocketPath[] = "/var/run/mojo/system_socket";
}  // namespace

// static
base::FilePath ExternalApplicationListener::ConstructDefaultSocketPath() {
  return base::FilePath(FILE_PATH_LITERAL(kDefaultListenSocketPath));
}

// static
scoped_ptr<ExternalApplicationListener> ExternalApplicationListener::Create(
    const scoped_refptr<base::SequencedTaskRunner>& shell_runner,
    const scoped_refptr<base::SequencedTaskRunner>& io_runner) {
  return make_scoped_ptr(
      new ExternalApplicationListenerPosix(shell_runner, io_runner));
}

class ExternalApplicationListenerPosix::RegistrarImpl
    : public InterfaceImpl<ExternalApplicationRegistrar> {
 public:
  explicit RegistrarImpl(const RegisterCallback& callback);
  ~RegistrarImpl() override;

  void OnConnectionError() override;

  embedder::ChannelInit channel_init;

 private:
  void Register(const String& app_url,
                InterfaceRequest<Shell> shell,
                const mojo::Closure& callback) override;

  const RegisterCallback register_callback_;
};

ExternalApplicationListenerPosix::ExternalApplicationListenerPosix(
    const scoped_refptr<base::SequencedTaskRunner>& shell_runner,
    const scoped_refptr<base::SequencedTaskRunner>& io_runner)
    : shell_runner_(shell_runner),
      io_runner_(io_runner),
      signal_on_listening_(true, false),
      weak_ptr_factory_(this) {
  DCHECK(shell_runner_.get() && shell_runner_->RunsTasksOnCurrentThread());
  DCHECK(io_runner_.get());
  listener_thread_checker_.DetachFromThread();  // Will attach in StartListener.
}

ExternalApplicationListenerPosix::~ExternalApplicationListenerPosix() {
  DCHECK(register_thread_checker_.CalledOnValidThread());
  weak_ptr_factory_.InvalidateWeakPtrs();

  // listener_ needs to be destroyed on io_runner_, and it has to die before
  // this object does, as it holds a pointer back to this instance.
  base::WaitableEvent stop_listening_event(true, false);
  io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ExternalApplicationListenerPosix::StopListening,
                 base::Unretained(this),
                 &stop_listening_event));
  stop_listening_event.Wait();
}

void ExternalApplicationListenerPosix::ListenInBackground(
    const base::FilePath& listen_socket_path,
    const RegisterCallback& register_callback) {
  DCHECK(register_thread_checker_.CalledOnValidThread());
  ListenInBackgroundWithErrorCallback(
      listen_socket_path, register_callback, ErrorCallback());
}

void ExternalApplicationListenerPosix::ListenInBackgroundWithErrorCallback(
    const base::FilePath& listen_socket_path,
    const RegisterCallback& register_callback,
    const ErrorCallback& error_callback) {
  DCHECK(register_thread_checker_.CalledOnValidThread());
  register_callback_ = register_callback;
  error_callback_ = error_callback;

  io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ExternalApplicationListenerPosix::StartListening,
                 base::Unretained(this),
                 listen_socket_path));
}

void ExternalApplicationListenerPosix::WaitForListening() {
  DCHECK(register_thread_checker_.CalledOnValidThread());
  signal_on_listening_.Wait();
}

void ExternalApplicationListenerPosix::StartListening(
    const base::FilePath& listen_socket_path) {
  CHECK_EQ(base::MessageLoop::current()->type(), base::MessageLoop::TYPE_IO);
  DCHECK(listener_thread_checker_.CalledOnValidThread());
  listener_.reset(
      new IncomingConnectionListenerPosix(listen_socket_path, this));
  listener_->StartListening();
}

void ExternalApplicationListenerPosix::StopListening(
    base::WaitableEvent* event) {
  DCHECK(listener_thread_checker_.CalledOnValidThread());
  listener_.reset();
  event->Signal();
}

void ExternalApplicationListenerPosix::OnListening(int rv) {
  DCHECK(listener_thread_checker_.CalledOnValidThread());
  signal_on_listening_.Signal();
  shell_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &ExternalApplicationListenerPosix::RunErrorCallbackIfListeningFailed,
          weak_ptr_factory_.GetWeakPtr(),
          rv));
}

void ExternalApplicationListenerPosix::OnConnection(
    net::SocketDescriptor incoming) {
  DCHECK(listener_thread_checker_.CalledOnValidThread());
  shell_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &ExternalApplicationListenerPosix::CreatePipeAndBindToRegistrarImpl,
          weak_ptr_factory_.GetWeakPtr(),
          incoming));
}

void ExternalApplicationListenerPosix::RunErrorCallbackIfListeningFailed(
    int rv) {
  DCHECK(register_thread_checker_.CalledOnValidThread());
  if (rv != net::OK && !error_callback_.is_null()) {
    error_callback_.Run(rv);
  }
}

void ExternalApplicationListenerPosix::CreatePipeAndBindToRegistrarImpl(
    net::SocketDescriptor incoming_socket) {
  DCHECK(register_thread_checker_.CalledOnValidThread());

  DVLOG(1) << "Accepted incoming connection";
  scoped_ptr<RegistrarImpl> registrar(new RegistrarImpl(register_callback_));
  // Passes ownership of incoming_socket to registrar->channel_init.
  mojo::ScopedMessagePipeHandle message_pipe =
      registrar->channel_init.Init(incoming_socket, io_runner_);
  CHECK(message_pipe.is_valid());

  BindToPipe(registrar.release(), message_pipe.Pass());
}

ExternalApplicationListenerPosix::RegistrarImpl::RegistrarImpl(
    const RegisterCallback& callback)
    : register_callback_(callback) {
}

ExternalApplicationListenerPosix::RegistrarImpl::~RegistrarImpl() {
}

void ExternalApplicationListenerPosix::RegistrarImpl::OnConnectionError() {
  channel_init.WillDestroySoon();
}

void ExternalApplicationListenerPosix::RegistrarImpl::Register(
    const String& app_url,
    InterfaceRequest<Shell> shell,
    const mojo::Closure& callback) {
  register_callback_.Run(app_url.To<GURL>(), shell.PassMessagePipe());
  callback.Run();
}

}  // namespace shell
}  // namespace mojo
