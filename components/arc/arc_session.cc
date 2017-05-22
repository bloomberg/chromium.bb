// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_session.h"

#include <fcntl.h>
#include <grp.h>
#include <poll.h>
#include <unistd.h>

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/posix/eintr_wrapper.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/arc/arc_bridge_host_impl.h"
#include "components/arc/arc_features.h"
#include "components/user_manager/user_manager.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/named_platform_handle.h"
#include "mojo/edk/embedder/named_platform_handle_utils.h"
#include "mojo/edk/embedder/outgoing_broker_client_invitation.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/platform_channel_utils_posix.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

namespace {

using StartArcInstanceResult =
    chromeos::SessionManagerClient::StartArcInstanceResult;

const base::FilePath::CharType kArcBridgeSocketPath[] =
    FILE_PATH_LITERAL("/var/run/chrome/arc_bridge.sock");

const char kArcBridgeSocketGroup[] = "arc-bridge";

// This is called when StopArcInstance D-Bus method completes. Since we have the
// ArcInstanceStopped() callback and are notified if StartArcInstance fails, we
// don't need to do anything when StopArcInstance completes.
void DoNothingInstanceStopped(bool) {}

chromeos::SessionManagerClient* GetSessionManagerClient() {
  // If the DBusThreadManager or the SessionManagerClient aren't available,
  // there isn't much we can do. This should only happen when running tests.
  if (!chromeos::DBusThreadManager::IsInitialized() ||
      !chromeos::DBusThreadManager::Get() ||
      !chromeos::DBusThreadManager::Get()->GetSessionManagerClient())
    return nullptr;
  return chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
}

// Creates a pipe. Returns true on success, otherwise false.
// On success, |read_fd| will be set to the fd of the read side, and
// |write_fd| will be set to the one of write side.
bool CreatePipe(base::ScopedFD* read_fd, base::ScopedFD* write_fd) {
  int fds[2];
  if (pipe2(fds, O_NONBLOCK | O_CLOEXEC) < 0) {
    PLOG(ERROR) << "pipe2()";
    return false;
  }

  read_fd->reset(fds[0]);
  write_fd->reset(fds[1]);
  return true;
}

// Waits until |raw_socket_fd| is readable.
// The operation may be cancelled originally triggered by user interaction to
// disable ARC, or ARC instance is unexpectedly stopped (e.g. crash).
// To notify such a situation, |raw_cancel_fd| is also passed to here, and the
// write side will be closed in such a case.
bool WaitForSocketReadable(int raw_socket_fd, int raw_cancel_fd) {
  struct pollfd fds[2] = {
      {raw_socket_fd, POLLIN, 0}, {raw_cancel_fd, POLLIN, 0},
  };

  if (HANDLE_EINTR(poll(fds, arraysize(fds), -1)) <= 0) {
    PLOG(ERROR) << "poll()";
    return false;
  }

  if (fds[1].revents) {
    // Notified that Stop() is invoked. Cancel the Mojo connecting.
    VLOG(1) << "Stop() was called during ConnectMojo()";
    return false;
  }

  DCHECK(fds[0].revents);
  return true;
}

// TODO(hidehiko): Refactor more to make this class unittest-able, for at least
// state-machine part.
class ArcSessionImpl : public ArcSession,
                       public chromeos::SessionManagerClient::Observer {
 public:
  // The possible states of the session.  In the normal flow, the state changes
  // in the following sequence:
  //
  // NOT_STARTED
  //   Start() ->
  // CREATING_SOCKET
  //   CreateSocket() -> OnSocketCreated() ->
  // STARTING_INSTANCE
  //   -> OnInstanceStarted() ->
  // CONNECTING_MOJO
  //   ConnectMojo() -> OnMojoConnected() ->
  // RUNNING
  //
  // At any state, Stop() can be called. It does not immediately stop the
  // instance, but will eventually stop it.
  // The actual stop will be notified via ArcSession::Observer::OnStopped().
  //
  // When Stop() is called, it makes various behavior based on the current
  // phase.
  //
  // NOT_STARTED:
  //   Do nothing. Immediately transition to the STOPPED state.
  // CREATING_SOCKET:
  //   The main task of the phase runs on BlockingPool thread. So, Stop() just
  //   sets the flag and return. On the main task completion, a callback
  //   will run on the main (practically UI) thread, and the flag is checked
  //   at the beginning of them. This should work under the assumption that
  //   the main tasks do not block indefinitely.
  // STARTING_INSTANCE:
  //   The ARC instance is starting via SessionManager. So, similar to
  //   CREATING_SOCKET case, Stop() just sets the flag and return. In its
  //   callback, it checks if ARC instance is successfully started or not.
  //   In case of success, a request to stop the ARC instance is sent to
  //   SessionManager. Its completion will be notified via ArcInstanceStopped.
  //   Otherwise, it just turns into STOPPED state.
  // CONNECTING_MOJO:
  //   The main task runs on BlockingPool thread, but it is blocking call.
  //   So, Stop() sends a request to cancel the blocking by closing the pipe
  //   whose read side is also polled. Then, in its callback, similar to
  //   STARTING_INSTANCE, a request to stop the ARC instance is sent to
  //   SessionManager, and ArcInstanceStopped handles remaining procedure.
  // RUNNING:
  //   There is no more callback which runs on normal flow, so Stop() requests
  //   to stop the ARC instance via SessionManager.
  //
  // Another trigger to change the state coming from outside of this class
  // is an event ArcInstanceStopped() sent from SessionManager, when ARC
  // instace unexpectedly terminates. ArcInstanceStopped() turns the state into
  // STOPPED immediately.
  // This happens only when STARTING_INSTANCE, CONNECTING_MOJO or RUNNING
  // state.
  //
  // STARTING_INSTANCE:
  //   In OnInstanceStarted(), |state_| is checked at the beginning. If it is
  //   STOPPED, then ArcInstanceStopped() is called. Do nothing in that case.
  // CONNECTING_MOJO:
  //   Similar to Stop() case above, ArcInstanceStopped() also notifies to
  //   BlockingPool thread to cancel it to unblock the thread. In
  //   OnMojoConnected(), similar to OnInstanceStarted(), check if |state_| is
  //   STOPPED, then do nothing.
  // RUNNING:
  //   It is not necessary to do anything special here.
  //
  // In NOT_STARTED or STOPPED state, the instance can be safely destructed.
  // Specifically, in STOPPED state, there may be inflight operations or
  // pending callbacks. Though, what they do is just do-nothing conceptually
  // and they can be safely ignored.
  //
  // Note: Order of constants below matters. Please make sure to sort them
  // in chronological order.
  enum class State {
    // ARC is not yet started.
    NOT_STARTED,

    // An UNIX socket is being created.
    CREATING_SOCKET,

    // The request to start the instance has been sent.
    STARTING_INSTANCE,

    // The instance has started. Waiting for it to connect to the IPC bridge.
    CONNECTING_MOJO,

    // The instance is fully set up.
    RUNNING,

    // ARC is terminated.
    STOPPED,
  };

  ArcSessionImpl(ArcBridgeService* arc_bridge_service,
                 const scoped_refptr<base::TaskRunner>& blocking_task_runner);
  ~ArcSessionImpl() override;

  // ArcSession overrides:
  void Start() override;
  void Stop() override;
  void OnShutdown() override;

 private:
  // Creates the UNIX socket on a worker pool and then processes its file
  // descriptor.
  static mojo::edk::ScopedPlatformHandle CreateSocket();
  void OnSocketCreated(mojo::edk::ScopedPlatformHandle fd);

  // DBus callback for StartArcInstance().
  void OnInstanceStarted(mojo::edk::ScopedPlatformHandle socket_fd,
                         StartArcInstanceResult result,
                         const std::string& container_instance_id);

  // Synchronously accepts a connection on |socket_fd| and then processes the
  // connected socket's file descriptor.
  static mojo::ScopedMessagePipeHandle ConnectMojo(
      mojo::edk::ScopedPlatformHandle socket_fd,
      base::ScopedFD cancel_fd);
  void OnMojoConnected(mojo::ScopedMessagePipeHandle server_pipe);

  // Request to stop ARC instance via DBus.
  void StopArcInstance();

  // chromeos::SessionManagerClient::Observer:
  void ArcInstanceStopped(bool clean,
                          const std::string& container_instance_id) override;

  // Completes the termination procedure.
  void OnStopped(ArcStopReason reason);

  // Checks whether a function runs on the thread where the instance is
  // created.
  base::ThreadChecker thread_checker_;

  // Owned by ArcServiceManager.
  ArcBridgeService* const arc_bridge_service_;

  // Task runner to run a blocking tasks.
  scoped_refptr<base::TaskRunner> blocking_task_runner_;

  // The state of the session.
  State state_ = State::NOT_STARTED;

  // When Stop() is called, this flag is set.
  bool stop_requested_ = false;

  // Container instance id passed from session_manager.
  // Should be available only after OnInstanceStarted().
  std::string container_instance_id_;

  // In CONNECTING_MOJO state, this is set to the write side of the pipe
  // to notify cancelling of the procedure.
  base::ScopedFD accept_cancel_pipe_;

  // Mojo endpoint.
  std::unique_ptr<mojom::ArcBridgeHost> arc_bridge_host_;

  // WeakPtrFactory to use callbacks.
  base::WeakPtrFactory<ArcSessionImpl> weak_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcSessionImpl);
};

ArcSessionImpl::ArcSessionImpl(
    ArcBridgeService* arc_bridge_service,
    const scoped_refptr<base::TaskRunner>& blocking_task_runner)
    : arc_bridge_service_(arc_bridge_service),
      blocking_task_runner_(blocking_task_runner),
      weak_factory_(this) {
  chromeos::SessionManagerClient* client = GetSessionManagerClient();
  if (client == nullptr)
    return;
  client->AddObserver(this);
}

ArcSessionImpl::~ArcSessionImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(state_ == State::NOT_STARTED || state_ == State::STOPPED);
  chromeos::SessionManagerClient* client = GetSessionManagerClient();
  if (client == nullptr)
    return;
  client->RemoveObserver(this);
}

void ArcSessionImpl::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, State::NOT_STARTED);
  VLOG(2) << "Starting ARC session.";
  VLOG(2) << "Creating socket...";

  state_ = State::CREATING_SOCKET;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::Bind(&ArcSessionImpl::CreateSocket),
      base::Bind(&ArcSessionImpl::OnSocketCreated, weak_factory_.GetWeakPtr()));
}

// static
mojo::edk::ScopedPlatformHandle ArcSessionImpl::CreateSocket() {
  base::FilePath socket_path(kArcBridgeSocketPath);

  mojo::edk::ScopedPlatformHandle socket_fd = mojo::edk::CreateServerHandle(
      mojo::edk::NamedPlatformHandle(socket_path.value()));
  if (!socket_fd.is_valid())
    return socket_fd;

  // Change permissions on the socket.
  struct group arc_bridge_group;
  struct group* arc_bridge_group_res = nullptr;
  char buf[10000];
  if (HANDLE_EINTR(getgrnam_r(kArcBridgeSocketGroup, &arc_bridge_group, buf,
                              sizeof(buf), &arc_bridge_group_res)) < 0) {
    PLOG(ERROR) << "getgrnam_r";
    return mojo::edk::ScopedPlatformHandle();
  }

  if (!arc_bridge_group_res) {
    LOG(ERROR) << "Group '" << kArcBridgeSocketGroup << "' not found";
    return mojo::edk::ScopedPlatformHandle();
  }

  if (HANDLE_EINTR(chown(kArcBridgeSocketPath, -1, arc_bridge_group.gr_gid)) <
      0) {
    PLOG(ERROR) << "chown";
    return mojo::edk::ScopedPlatformHandle();
  }

  if (!base::SetPosixFilePermissions(socket_path, 0660)) {
    PLOG(ERROR) << "Could not set permissions: " << socket_path.value();
    return mojo::edk::ScopedPlatformHandle();
  }

  return socket_fd;
}

void ArcSessionImpl::OnSocketCreated(
    mojo::edk::ScopedPlatformHandle socket_fd) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, State::CREATING_SOCKET);

  if (stop_requested_) {
    VLOG(1) << "Stop() called while connecting";
    OnStopped(ArcStopReason::SHUTDOWN);
    return;
  }

  if (!socket_fd.is_valid()) {
    LOG(ERROR) << "ARC: Error creating socket";
    OnStopped(ArcStopReason::GENERIC_BOOT_FAILURE);
    return;
  }

  VLOG(2) << "Socket is created. Starting ARC instance...";
  state_ = State::STARTING_INSTANCE;
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  DCHECK(user_manager->GetPrimaryUser());
  const cryptohome::Identification cryptohome_id(
      user_manager->GetPrimaryUser()->GetAccountId());

  bool disable_boot_completed_broadcast =
      !base::FeatureList::IsEnabled(arc::kBootCompletedBroadcastFeature);

  // We only enable /vendor/priv-app when voice interaction is enabled because
  // voice interaction service apk would be bundled in this location.
  bool enable_vendor_privileged =
      chromeos::switches::IsVoiceInteractionEnabled();

  chromeos::SessionManagerClient* session_manager_client =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  session_manager_client->StartArcInstance(
      cryptohome_id, disable_boot_completed_broadcast, enable_vendor_privileged,
      base::Bind(&ArcSessionImpl::OnInstanceStarted, weak_factory_.GetWeakPtr(),
                 base::Passed(&socket_fd)));
}

void ArcSessionImpl::OnInstanceStarted(
    mojo::edk::ScopedPlatformHandle socket_fd,
    StartArcInstanceResult result,
    const std::string& container_instance_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, State::STARTING_INSTANCE);
  container_instance_id_ = container_instance_id;

  if (stop_requested_) {
    if (result == StartArcInstanceResult::SUCCESS) {
      // The ARC instance has started to run. Request to stop.
      StopArcInstance();
      return;
    }
    OnStopped(ArcStopReason::SHUTDOWN);
    return;
  }

  if (result != StartArcInstanceResult::SUCCESS) {
    LOG(ERROR) << "Failed to start ARC instance";
    OnStopped(result == StartArcInstanceResult::LOW_FREE_DISK_SPACE
                  ? ArcStopReason::LOW_DISK_SPACE
                  : ArcStopReason::GENERIC_BOOT_FAILURE);
    return;
  }

  VLOG(2) << "ARC instance is successfully started. Connecting Mojo...";
  state_ = State::CONNECTING_MOJO;

  // Prepare a pipe so that AcceptInstanceConnection can be interrupted on
  // Stop().
  base::ScopedFD cancel_fd;
  if (!CreatePipe(&cancel_fd, &accept_cancel_pipe_)) {
    OnStopped(ArcStopReason::GENERIC_BOOT_FAILURE);
    return;
  }

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::Bind(&ArcSessionImpl::ConnectMojo, base::Passed(&socket_fd),
                 base::Passed(&cancel_fd)),
      base::Bind(&ArcSessionImpl::OnMojoConnected, weak_factory_.GetWeakPtr()));
}

// static
mojo::ScopedMessagePipeHandle ArcSessionImpl::ConnectMojo(
    mojo::edk::ScopedPlatformHandle socket_fd,
    base::ScopedFD cancel_fd) {
  if (!WaitForSocketReadable(socket_fd.get().handle, cancel_fd.get())) {
    VLOG(1) << "Mojo connection was cancelled.";
    return mojo::ScopedMessagePipeHandle();
  }

  mojo::edk::ScopedPlatformHandle scoped_fd;
  if (!mojo::edk::ServerAcceptConnection(socket_fd.get(), &scoped_fd,
                                         /* check_peer_user = */ false) ||
      !scoped_fd.is_valid()) {
    return mojo::ScopedMessagePipeHandle();
  }

  // Hardcode pid 0 since it is unused in mojo.
  const base::ProcessHandle kUnusedChildProcessHandle = 0;
  mojo::edk::PlatformChannelPair channel_pair;
  mojo::edk::OutgoingBrokerClientInvitation invitation;

  std::string token = mojo::edk::GenerateRandomToken();
  mojo::ScopedMessagePipeHandle pipe = invitation.AttachMessagePipe(token);

  invitation.Send(
      kUnusedChildProcessHandle,
      mojo::edk::ConnectionParams(mojo::edk::TransportProtocol::kLegacy,
                                  channel_pair.PassServerHandle()));

  mojo::edk::ScopedPlatformHandleVectorPtr handles(
      new mojo::edk::PlatformHandleVector{
          channel_pair.PassClientHandle().release()});

  // We need to send the length of the message as a single byte, so make sure it
  // fits.
  DCHECK_LT(token.size(), 256u);
  uint8_t message_length = static_cast<uint8_t>(token.size());
  struct iovec iov[] = {{&message_length, sizeof(message_length)},
                        {const_cast<char*>(token.c_str()), token.size()}};
  ssize_t result = mojo::edk::PlatformChannelSendmsgWithHandles(
      scoped_fd.get(), iov, sizeof(iov) / sizeof(iov[0]), handles->data(),
      handles->size());
  if (result == -1) {
    PLOG(ERROR) << "sendmsg";
    return mojo::ScopedMessagePipeHandle();
  }

  return pipe;
}

void ArcSessionImpl::OnMojoConnected(
    mojo::ScopedMessagePipeHandle server_pipe) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, State::CONNECTING_MOJO);
  accept_cancel_pipe_.reset();

  if (stop_requested_) {
    StopArcInstance();
    return;
  }

  if (!server_pipe.is_valid()) {
    LOG(ERROR) << "Invalid pipe";
    StopArcInstance();
    return;
  }

  mojom::ArcBridgeInstancePtr instance;
  instance.Bind(mojo::InterfacePtrInfo<mojom::ArcBridgeInstance>(
      std::move(server_pipe), 0u));
  arc_bridge_host_ = base::MakeUnique<ArcBridgeHostImpl>(arc_bridge_service_,
                                                         std::move(instance));

  VLOG(2) << "Mojo is connected. ARC is running.";
  state_ = State::RUNNING;
  for (auto& observer : observer_list_)
    observer.OnSessionReady();
}

void ArcSessionImpl::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(2) << "Stopping ARC session is requested.";

  // For second time or later, just do nothing.
  // It is already in the stopping phase.
  if (stop_requested_)
    return;

  stop_requested_ = true;
  arc_bridge_host_.reset();
  switch (state_) {
    case State::NOT_STARTED:
      OnStopped(ArcStopReason::SHUTDOWN);
      return;

    case State::CREATING_SOCKET:
    case State::STARTING_INSTANCE:
      // Before starting the ARC instance, we do nothing here.
      // At some point, a callback will be invoked on UI thread,
      // and stopping procedure will be run there.
      // On Chrome shutdown, it is not the case because the message loop is
      // already stopped here. Practically, it is not a problem because;
      // - On socket creating, it is ok to simply ignore such cases,
      // because we no-longer continue the bootstrap procedure.
      // - On starting instance, the container instance can be leaked.
      // Practically it is not problematic because the session manager will
      // clean it up.
      return;

    case State::CONNECTING_MOJO:
      // Mojo connection is being waited on a BlockingPool thread.
      // Request to cancel it. Following stopping procedure will run
      // in its callback.
      accept_cancel_pipe_.reset();
      return;

    case State::RUNNING:
      // Now ARC instance is running. Request to stop it.
      StopArcInstance();
      return;

    case State::STOPPED:
      // The instance is already stopped. Do nothing.
      return;
  }
}

void ArcSessionImpl::StopArcInstance() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(state_ == State::STARTING_INSTANCE ||
         state_ == State::CONNECTING_MOJO || state_ == State::RUNNING);

  // Notification will arrive through ArcInstanceStopped().
  VLOG(2) << "Requesting to stop ARC instance";
  chromeos::SessionManagerClient* session_manager_client =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  session_manager_client->StopArcInstance(
      base::Bind(&DoNothingInstanceStopped));
}

void ArcSessionImpl::ArcInstanceStopped(
    bool clean,
    const std::string& container_instance_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "Notified that ARC instance is stopped "
          << (clean ? "cleanly" : "uncleanly");

  if (container_instance_id != container_instance_id_) {
    VLOG(1) << "Container instance id mismatch. Do nothing."
            << container_instance_id << " vs " << container_instance_id_;
    return;
  }

  // Release |container_instance_id_| to avoid duplicate invocation situation.
  container_instance_id_.clear();

  // In case that crash happens during before the Mojo channel is connected,
  // unlock the BlockingPool thread.
  accept_cancel_pipe_.reset();

  ArcStopReason reason;
  if (stop_requested_) {
    // If the ARC instance is stopped after its explicit request,
    // return SHUTDOWN.
    reason = ArcStopReason::SHUTDOWN;
  } else if (clean) {
    // If the ARC instance is stopped, but it is not explicitly requested,
    // then this is triggered by some failure during the starting procedure.
    // Return GENERIC_BOOT_FAILURE for the case.
    reason = ArcStopReason::GENERIC_BOOT_FAILURE;
  } else {
    // Otherwise, this is caused by CRASH occured inside of the ARC instance.
    reason = ArcStopReason::CRASH;
  }
  OnStopped(reason);
}

void ArcSessionImpl::OnStopped(ArcStopReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // OnStopped() should be called once per instance.
  DCHECK_NE(state_, State::STOPPED);
  VLOG(2) << "ARC session is stopped.";
  arc_bridge_host_.reset();
  state_ = State::STOPPED;
  for (auto& observer : observer_list_)
    observer.OnSessionStopped(reason);
}

void ArcSessionImpl::OnShutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());
  stop_requested_ = true;
  if (state_ == State::STOPPED)
    return;

  // Here, the message loop is already stopped, and the Chrome will be soon
  // shutdown. Thus, it is not necessary to take care about restarting case.
  // If ArcSession is waiting for mojo connection, cancels it. The BlockingPool
  // will be joined later.
  accept_cancel_pipe_.reset();

  // Stops the ARC instance to let it graceful shutdown.
  // Note that this may fail if ARC container is not actually running, but
  // ignore an error as described below.
  if (state_ == State::STARTING_INSTANCE ||
      state_ == State::CONNECTING_MOJO || state_ == State::RUNNING)
    StopArcInstance();

  // Directly set to the STOPPED stateby OnStopped(). Note that calling
  // StopArcInstance() may not work well. At least, because the UI thread is
  // already stopped here, ArcInstanceStopped() callback cannot be invoked.
  OnStopped(ArcStopReason::SHUTDOWN);
}

}  // namespace

ArcSession::ArcSession() = default;
ArcSession::~ArcSession() = default;

void ArcSession::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ArcSession::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

// static
std::unique_ptr<ArcSession> ArcSession::Create(
    ArcBridgeService* arc_bridge_service,
    const scoped_refptr<base::TaskRunner>& blocking_task_runner) {
  return base::MakeUnique<ArcSessionImpl>(arc_bridge_service,
                                          blocking_task_runner);
}

}  // namespace arc
