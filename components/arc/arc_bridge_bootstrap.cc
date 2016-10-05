// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_bridge_bootstrap.h"

#include <fcntl.h>
#include <grp.h>
#include <poll.h>
#include <unistd.h>

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
#include "base/threading/worker_pool.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/arc/arc_bridge_host_impl.h"
#include "components/arc/arc_features.h"
#include "components/user_manager/user_manager.h"
#include "ipc/unix_domain_socket_util.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/platform_channel_utils_posix.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

namespace {

const base::FilePath::CharType kArcBridgeSocketPath[] =
    FILE_PATH_LITERAL("/var/run/chrome/arc_bridge.sock");

const char kArcBridgeSocketGroup[] = "arc-bridge";

const base::FilePath::CharType kDiskCheckPath[] = "/home";

const int64_t kCriticalDiskFreeBytes = 64 << 20;  // 64MB

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
class ArcBridgeBootstrapImpl : public ArcBridgeBootstrap,
                               public chromeos::SessionManagerClient::Observer {
 public:
  // The possible states of the bootstrap connection.  In the normal flow,
  // the state changes in the following sequence:
  //
  // NOT_STARTED
  //   Start() ->
  // CHECKING_DISK_SPACE
  //   OnDiskSpaceChecked() ->
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
  // The actual stop will be notified via Observer::OnStopped().
  //
  // When Stop() is called, it makes various behavior based on the current
  // phase.
  //
  // NOT_STARTED:
  //   Do nothing. Immediately transition to the STOPPED state.
  // CHECKING_DISK_SPACE, CREATING_SOCKET:
  //   The main task of those phases runs on WorkerPool thread. So, Stop()
  //   just sets the flag and return. On the main task completion, a callback
  //   will run on the main (practically UI) thread, and the flag is checked
  //   at the beginning of them. This should work under the assumption that
  //   the main tasks do not block indefinitely.
  // STARTING_INSTANCE:
  //   The ARC instance is starting via SessionManager. So, similar to
  //   CHECKING_DISK_SPACE/CREATING_SOCKET cases, Stop() just sets the flag
  //   and return. In its callback, it checks if ARC instance is successfully
  //   started or not. In case of success, a request to stop the ARC instance
  //   is sent to SessionManager. Its completion will be notified via
  //   ArcInstanceStopped. Otherwise, it just turns into STOPPED state.
  // CONNECTING_MOJO:
  //   The main task runs on WorkerPool thread, but it is blocking call.
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
  //   WorkerPool() thread to cancel it to unblock the thread. In
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

    // Checking the disk space.
    CHECKING_DISK_SPACE,

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

  ArcBridgeBootstrapImpl();
  ~ArcBridgeBootstrapImpl() override;

  // ArcBridgeBootstrap:
  void Start() override;
  void Stop() override;

 private:
  // Called after getting the device free disk space.
  void OnFreeDiskSpaceObtained(int64_t disk_free_bytes);

  // Creates the UNIX socket on the bootstrap thread and then processes its
  // file descriptor.
  static base::ScopedFD CreateSocket();
  void OnSocketCreated(base::ScopedFD fd);

  // DBus callback for StartArcInstance().
  void OnInstanceStarted(base::ScopedFD socket_fd, bool success);

  // Synchronously accepts a connection on |socket_fd| and then processes the
  // connected socket's file descriptor.
  static base::ScopedFD ConnectMojo(base::ScopedFD socket_fd,
                                    base::ScopedFD cancel_fd);
  void OnMojoConnected(base::ScopedFD fd);

  // Request to stop ARC instance via DBus.
  void StopArcInstance();

  // chromeos::SessionManagerClient::Observer:
  void ArcInstanceStopped(bool clean) override;

  // Completes the termination procedure.
  void OnStopped(ArcBridgeService::StopReason reason);

  // The state of the bootstrap connection.
  State state_ = State::NOT_STARTED;

  // When Stop() is called, this flag is set.
  bool stop_requested_ = false;

  // In CONNECTING_MOJO state, this is set to the write side of the pipe
  // to notify cancelling of the procedure.
  base::ScopedFD accept_cancel_pipe_;

  // Mojo endpoint.
  std::unique_ptr<mojom::ArcBridgeHost> arc_bridge_host_;

  base::ThreadChecker thread_checker_;

  // WeakPtrFactory to use callbacks.
  base::WeakPtrFactory<ArcBridgeBootstrapImpl> weak_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcBridgeBootstrapImpl);
};

ArcBridgeBootstrapImpl::ArcBridgeBootstrapImpl()
    : weak_factory_(this) {
  chromeos::SessionManagerClient* client = GetSessionManagerClient();
  if (client == nullptr)
    return;
  client->AddObserver(this);
}

ArcBridgeBootstrapImpl::~ArcBridgeBootstrapImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(hidehiko): CHECK if |state_| is in NOT_STARTED or STOPPED.
  // Currently, specifically on shutdown, the state_ can be any value.
  chromeos::SessionManagerClient* client = GetSessionManagerClient();
  if (client == nullptr)
    return;
  client->RemoveObserver(this);
}

void ArcBridgeBootstrapImpl::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, State::NOT_STARTED);
  VLOG(2) << "Starting ARC session.";
  VLOG(2) << "Checking disk space...";
  state_ = State::CHECKING_DISK_SPACE;

  // TODO(crbug.com/628124): Move disk space checking logic to session_manager.
  base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(true).get(), FROM_HERE,
      base::Bind(&base::SysInfo::AmountOfFreeDiskSpace,
                 base::FilePath(kDiskCheckPath)),
      base::Bind(&ArcBridgeBootstrapImpl::OnFreeDiskSpaceObtained,
                 weak_factory_.GetWeakPtr()));
}

void ArcBridgeBootstrapImpl::OnFreeDiskSpaceObtained(int64_t disk_free_bytes) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, State::CHECKING_DISK_SPACE);

  if (stop_requested_) {
    VLOG(1) << "Stop() called while checking disk space";
    OnStopped(ArcBridgeService::StopReason::SHUTDOWN);
    return;
  }

  if (disk_free_bytes < 0) {
    LOG(ERROR) << "ARC: Failed to get free disk space";
    OnStopped(ArcBridgeService::StopReason::GENERIC_BOOT_FAILURE);
    return;
  }
  if (disk_free_bytes < kCriticalDiskFreeBytes) {
    LOG(ERROR) << "ARC: The device is too low on disk space to start ARC";
    OnStopped(ArcBridgeService::StopReason::LOW_DISK_SPACE);
    return;
  }

  VLOG(2) << "Disk space check is done. Creating socket...";
  state_ = State::CREATING_SOCKET;
  base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(true).get(), FROM_HERE,
      base::Bind(&ArcBridgeBootstrapImpl::CreateSocket),
      base::Bind(&ArcBridgeBootstrapImpl::OnSocketCreated,
                 weak_factory_.GetWeakPtr()));
}

// static
base::ScopedFD ArcBridgeBootstrapImpl::CreateSocket() {
  base::FilePath socket_path(kArcBridgeSocketPath);

  int raw_fd = -1;
  if (!IPC::CreateServerUnixDomainSocket(socket_path, &raw_fd))
    return base::ScopedFD();
  base::ScopedFD socket_fd(raw_fd);

  // Change permissions on the socket.
  struct group arc_bridge_group;
  struct group* arc_bridge_group_res = nullptr;
  char buf[10000];
  if (HANDLE_EINTR(getgrnam_r(kArcBridgeSocketGroup, &arc_bridge_group, buf,
                              sizeof(buf), &arc_bridge_group_res)) < 0) {
    PLOG(ERROR) << "getgrnam_r";
    return base::ScopedFD();
  }

  if (!arc_bridge_group_res) {
    LOG(ERROR) << "Group '" << kArcBridgeSocketGroup << "' not found";
    return base::ScopedFD();
  }

  if (HANDLE_EINTR(chown(kArcBridgeSocketPath, -1, arc_bridge_group.gr_gid)) <
      0) {
    PLOG(ERROR) << "chown";
    return base::ScopedFD();
  }

  if (!base::SetPosixFilePermissions(socket_path, 0660)) {
    PLOG(ERROR) << "Could not set permissions: " << socket_path.value();
    return base::ScopedFD();
  }

  return socket_fd;
}

void ArcBridgeBootstrapImpl::OnSocketCreated(base::ScopedFD socket_fd) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, State::CREATING_SOCKET);

  if (stop_requested_) {
    VLOG(1) << "Stop() called while connecting";
    OnStopped(ArcBridgeService::StopReason::SHUTDOWN);
    return;
  }

  if (!socket_fd.is_valid()) {
    LOG(ERROR) << "ARC: Error creating socket";
    OnStopped(ArcBridgeService::StopReason::GENERIC_BOOT_FAILURE);
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

  chromeos::SessionManagerClient* session_manager_client =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  session_manager_client->StartArcInstance(
      cryptohome_id,
      disable_boot_completed_broadcast,
      base::Bind(&ArcBridgeBootstrapImpl::OnInstanceStarted,
                 weak_factory_.GetWeakPtr(), base::Passed(&socket_fd)));
}

void ArcBridgeBootstrapImpl::OnInstanceStarted(base::ScopedFD socket_fd,
                                               bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (state_ == State::STOPPED) {
    // This is the case that error is notified via DBus before the
    // OnInstanceStarted() callback is invoked. The stopping procedure has
    // been run, so do nothing.
    return;
  }

  DCHECK_EQ(state_, State::STARTING_INSTANCE);

  if (stop_requested_) {
    if (success) {
      // The ARC instance has started to run. Request to stop.
      StopArcInstance();
      return;
    }
    OnStopped(ArcBridgeService::StopReason::SHUTDOWN);
    return;
  }

  if (!success) {
    LOG(ERROR) << "Failed to start ARC instance";
    OnStopped(ArcBridgeService::StopReason::GENERIC_BOOT_FAILURE);
    return;
  }

  VLOG(2) << "ARC instance is successfully started. Connecting Mojo...";
  state_ = State::CONNECTING_MOJO;

  // Prepare a pipe so that AcceptInstanceConnection can be interrupted on
  // Stop().
  base::ScopedFD cancel_fd;
  if (!CreatePipe(&cancel_fd, &accept_cancel_pipe_)) {
    OnStopped(ArcBridgeService::StopReason::GENERIC_BOOT_FAILURE);
    return;
  }

  base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(true).get(), FROM_HERE,
      base::Bind(&ArcBridgeBootstrapImpl::ConnectMojo, base::Passed(&socket_fd),
                 base::Passed(&cancel_fd)),
      base::Bind(&ArcBridgeBootstrapImpl::OnMojoConnected,
                 weak_factory_.GetWeakPtr()));
}

// static
base::ScopedFD ArcBridgeBootstrapImpl::ConnectMojo(base::ScopedFD socket_fd,
                                                   base::ScopedFD cancel_fd) {
  if (!WaitForSocketReadable(socket_fd.get(), cancel_fd.get())) {
    VLOG(1) << "Mojo connection was cancelled.";
    return base::ScopedFD();
  }

  int raw_fd = -1;
  if (!IPC::ServerOnConnect(socket_fd.get(), &raw_fd)) {
    return base::ScopedFD();
  }
  base::ScopedFD scoped_fd(raw_fd);

  // Hardcode pid 0 since it is unused in mojo.
  const base::ProcessHandle kUnusedChildProcessHandle = 0;
  mojo::edk::PlatformChannelPair channel_pair;
  mojo::edk::ChildProcessLaunched(kUnusedChildProcessHandle,
                                  channel_pair.PassServerHandle(),
                                  mojo::edk::GenerateRandomToken());

  mojo::edk::ScopedPlatformHandleVectorPtr handles(
      new mojo::edk::PlatformHandleVector{
          channel_pair.PassClientHandle().release()});

  struct iovec iov = {const_cast<char*>(""), 1};
  ssize_t result = mojo::edk::PlatformChannelSendmsgWithHandles(
      mojo::edk::PlatformHandle(scoped_fd.get()), &iov, 1, handles->data(),
      handles->size());
  if (result == -1) {
    PLOG(ERROR) << "sendmsg";
    return base::ScopedFD();
  }

  return scoped_fd;
}

void ArcBridgeBootstrapImpl::OnMojoConnected(base::ScopedFD fd) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == State::STOPPED) {
    // This is the case that error is notified via DBus before the
    // OnMojoConnected() callback is invoked. The stopping procedure has
    // been run, so do nothing.
    return;
  }

  DCHECK_EQ(state_, State::CONNECTING_MOJO);
  accept_cancel_pipe_.reset();

  if (stop_requested_) {
    StopArcInstance();
    return;
  }

  if (!fd.is_valid()) {
    LOG(ERROR) << "Invalid handle";
    StopArcInstance();
    return;
  }

  mojo::ScopedMessagePipeHandle server_pipe = mojo::edk::CreateMessagePipe(
      mojo::edk::ScopedPlatformHandle(mojo::edk::PlatformHandle(fd.release())));
  if (!server_pipe.is_valid()) {
    LOG(ERROR) << "Invalid pipe";
    StopArcInstance();
    return;
  }

  mojom::ArcBridgeInstancePtr instance;
  instance.Bind(mojo::InterfacePtrInfo<mojom::ArcBridgeInstance>(
      std::move(server_pipe), 0u));
  arc_bridge_host_.reset(new ArcBridgeHostImpl(std::move(instance)));

  VLOG(2) << "Mojo is connected. ARC is running.";
  state_ = State::RUNNING;
  FOR_EACH_OBSERVER(ArcBridgeBootstrap::Observer, observer_list_, OnReady());
}

void ArcBridgeBootstrapImpl::Stop() {
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
      OnStopped(ArcBridgeService::StopReason::SHUTDOWN);
      return;

    case State::CHECKING_DISK_SPACE:
    case State::CREATING_SOCKET:
    case State::STARTING_INSTANCE:
      // Before starting the ARC instance, we do nothing here.
      // At some point, a callback will be invoked on UI thread,
      // and stopping procedure will be run there.
      // On Chrome shutdown, it is not the case because the message loop is
      // already stopped here. Practically, it is not a problem because;
      // - On disk space checking or on socket creating, it is ok to simply
      // ignore such cases, because we no-longer continue the bootstrap
      // procedure.
      // - On starting instance, the container instance can be leaked.
      // Practically it is not problematic because the session manager will
      // clean it up.
      return;

    case State::CONNECTING_MOJO:
      // Mojo connection is being waited on a WorkerPool thread.
      // Request to cancel it. Following stopping procedure will run
      // in its callback.
      DCHECK(accept_cancel_pipe_.get());
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

void ArcBridgeBootstrapImpl::StopArcInstance() {
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

void ArcBridgeBootstrapImpl::ArcInstanceStopped(bool clean) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "Notified that ARC instance is stopped "
          << (clean ? "cleanly" : "uncleanly");

  // In case that crash happens during before the Mojo channel is connected,
  // unlock the WorkerPool thread.
  accept_cancel_pipe_.reset();

  ArcBridgeService::StopReason reason;
  if (stop_requested_) {
    // If the ARC instance is stopped after its explicit request,
    // return SHUTDOWN.
    reason = ArcBridgeService::StopReason::SHUTDOWN;
  } else if (clean) {
    // If the ARC instance is stopped, but it is not explicitly requested,
    // then this is triggered by some failure during the starting procedure.
    // Return GENERIC_BOOT_FAILURE for the case.
    reason = ArcBridgeService::StopReason::GENERIC_BOOT_FAILURE;
  } else {
    // Otherwise, this is caused by CRASH occured inside of the ARC instance.
    reason = ArcBridgeService::StopReason::CRASH;
  }
  OnStopped(reason);
}

void ArcBridgeBootstrapImpl::OnStopped(ArcBridgeService::StopReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // OnStopped() should be called once per instance.
  DCHECK_NE(state_, State::STOPPED);
  VLOG(2) << "ARC session is stopped.";
  arc_bridge_host_.reset();
  state_ = State::STOPPED;
  FOR_EACH_OBSERVER(ArcBridgeBootstrap::Observer, observer_list_,
                    OnStopped(reason));
}

}  // namespace

ArcBridgeBootstrap::ArcBridgeBootstrap() = default;
ArcBridgeBootstrap::~ArcBridgeBootstrap() = default;

void ArcBridgeBootstrap::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ArcBridgeBootstrap::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

// static
std::unique_ptr<ArcBridgeBootstrap> ArcBridgeBootstrap::Create() {
  return base::MakeUnique<ArcBridgeBootstrapImpl>();
}

}  // namespace arc
