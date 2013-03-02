// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/ibus_daemon_controller.h"

#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/environment.h"
#include "base/files/file_path_watcher.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/thread_checker.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

namespace {

IBusDaemonController* g_ibus_daemon_controller = NULL;
base::FilePathWatcher* g_file_path_watcher = NULL;

// Called when the ibus-daemon address file is modified.
static void OnFilePathChanged(
    const scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    const base::Closure& closure,
    const base::FilePath& file_path,
    bool failed) {
  if (failed)
    return;  // Can't recover, do nothing.
  if (!g_file_path_watcher)
    return;  // Already discarded watch task.

  ui_task_runner->PostTask(FROM_HERE, closure);
  ui_task_runner->DeleteSoon(FROM_HERE, g_file_path_watcher);
  g_file_path_watcher = NULL;
}

// Start watching |address_file_path|. If the target file is changed, |callback|
// is called on UI thread. This function should be called on FILE thread.
void StartWatch(
    const std::string& address_file_path,
    const base::Closure& closure,
    const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner) {
  // Before start watching, discard on-going watching task.
  delete g_file_path_watcher;
  g_file_path_watcher = new base::FilePathWatcher;
  bool result = g_file_path_watcher->Watch(
      base::FilePath::FromUTF8Unsafe(address_file_path),
      false,  // do not watch child directory.
      base::Bind(&OnFilePathChanged,
                 ui_task_runner,
                 closure));
  DCHECK(result);
}

// The implementation of IBusDaemonController.
class IBusDaemonControllerImpl : public IBusDaemonController {
 public:
  // Represents current ibus-daemon status.
  enum IBusDaemonStatus {
    IBUS_DAEMON_INITIALIZING,
    IBUS_DAEMON_RUNNING,
    IBUS_DAEMON_SHUTTING_DOWN,
    IBUS_DAEMON_STOP,
  };

  IBusDaemonControllerImpl(
      const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& file_task_runner)
      : process_handle_(base::kNullProcessHandle),
      ibus_daemon_status_(IBUS_DAEMON_STOP),
      ui_task_runner_(ui_task_runner),
      file_task_runner_(file_task_runner),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  }

  virtual ~IBusDaemonControllerImpl() {}

  // IBusDaemonController override:
  virtual void AddObserver(Observer* observer) OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    observers_.AddObserver(observer);
  }

  // IBusDaemonController override:
  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    observers_.RemoveObserver(observer);
  }

  // IBusDaemonController override:
  virtual bool Start() OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (ibus_daemon_status_ == IBUS_DAEMON_RUNNING)
      return true;
    if (ibus_daemon_status_ == IBUS_DAEMON_STOP ||
        ibus_daemon_status_ == IBUS_DAEMON_SHUTTING_DOWN) {
      return StartIBusDaemon();
    }
    return true;
  }

  // IBusDaemonController override:
  virtual bool Stop() OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    NOTREACHED() << "Termination of ibus-daemon is not supported"
                 << "http://crosbug.com/27051";
    return false;
  }

 private:
  // Starts ibus-daemon service.
  bool StartIBusDaemon() {
    if (ibus_daemon_status_ == IBUS_DAEMON_INITIALIZING ||
        ibus_daemon_status_ == IBUS_DAEMON_RUNNING) {
      DVLOG(1) << "MaybeLaunchIBusDaemon: ibus-daemon is already running.";
      return false;
    }

    ibus_daemon_status_ = IBUS_DAEMON_INITIALIZING;
    ibus_daemon_address_ = base::StringPrintf(
        "unix:abstract=ibus-%d",
        base::RandInt(0, std::numeric_limits<int>::max()));

    scoped_ptr<base::Environment> env(base::Environment::Create());
    std::string address_file_path;
    env->GetVar("IBUS_ADDRESS_FILE", &address_file_path);
    DCHECK(!address_file_path.empty());

    // Set up ibus-daemon address file watcher before launching ibus-daemon,
    // because if watcher starts after ibus-daemon, we may miss the ibus
    // connection initialization.
    bool success = file_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::Bind(&StartWatch,
                   address_file_path,
                   base::Bind(&IBusDaemonControllerImpl::FilePathChanged,
                              weak_ptr_factory_.GetWeakPtr(),
                              ibus_daemon_address_),
                   ui_task_runner_),
        base::Bind(&IBusDaemonControllerImpl::LaunchIBusDaemon,
                   weak_ptr_factory_.GetWeakPtr(),
                   ibus_daemon_address_));
    DCHECK(success);
    return true;
  }

  // Launhes actual ibus-daemon process.
  void LaunchIBusDaemon(const std::string& ibus_address) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK_EQ(base::kNullProcessHandle, process_handle_);
    static const char kIBusDaemonPath[] = "/usr/bin/ibus-daemon";
    // TODO(zork): Send output to /var/log/ibus.log
    std::vector<std::string> ibus_daemon_command_line;
    ibus_daemon_command_line.push_back(kIBusDaemonPath);
    ibus_daemon_command_line.push_back("--panel=disable");
    ibus_daemon_command_line.push_back("--cache=none");
    ibus_daemon_command_line.push_back("--restart");
    ibus_daemon_command_line.push_back("--replace");
    ibus_daemon_command_line.push_back("--address=" + ibus_address);

    if (!base::LaunchProcess(ibus_daemon_command_line,
                             base::LaunchOptions(),
                             &process_handle_)) {
      LOG(WARNING) << "Could not launch: "
                   << JoinString(ibus_daemon_command_line, " ");
    }
  }

  // Called by FilePathWatcher when the ibus-daemon address file is changed.
  // This function will be called on FILE thread.
  void FilePathChanged(const std::string& ibus_address) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&IBusDaemonControllerImpl::IBusDaemonInitializationDone,
                   weak_ptr_factory_.GetWeakPtr(),
                   ibus_address));
  }

  // Called by FilePathChaged function, this function should be called on UI
  // thread.
  void IBusDaemonInitializationDone(const std::string& ibus_address) {
    if (ibus_daemon_address_ != ibus_address)
      return;

    if (ibus_daemon_status_ != IBUS_DAEMON_INITIALIZING) {
      // Stop() or OnIBusDaemonExit() has already been called.
      return;
    }

    DBusThreadManager::Get()->InitIBusBus(
        ibus_address,
        base::Bind(&IBusDaemonControllerImpl::OnIBusDaemonDisconnected,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::GetProcId(process_handle_)));
    ibus_daemon_status_ = IBUS_DAEMON_RUNNING;
    FOR_EACH_OBSERVER(Observer, observers_, OnConnected());

    VLOG(1) << "The ibus-daemon initialization is done.";
  }

  // Called when the connection with ibus-daemon is disconnected.
  void OnIBusDaemonDisconnected(base::ProcessId pid) {
    if (!chromeos::DBusThreadManager::Get())
      return;  // Expected disconnection at shutting down. do nothing.

    if (process_handle_ != base::kNullProcessHandle) {
      if (base::GetProcId(process_handle_) == pid) {
        // ibus-daemon crashed.
        // TODO(nona): Shutdown ibus-bus connection.
        process_handle_ = base::kNullProcessHandle;
      } else {
        // This condition is as follows.
        // 1. Called Stop (process_handle_ becomes null)
        // 2. Called LaunchProcess (process_handle_ becomes new instance)
        // 3. Callbacked OnIBusDaemonExit for old instance and reach here.
        // In this case, we should not reset process_handle_ as null, and do not
        // re-launch ibus-daemon.
        return;
      }
    }

    const IBusDaemonStatus on_exit_state = ibus_daemon_status_;
    ibus_daemon_status_ = IBUS_DAEMON_STOP;
    FOR_EACH_OBSERVER(Observer, observers_, OnDisconnected());

    if (on_exit_state == IBUS_DAEMON_SHUTTING_DOWN)
      return;  // Normal exitting, so do nothing.

    LOG(ERROR) << "The ibus-daemon crashed. Re-launching...";
    StartIBusDaemon();
  }

  // The current ibus_daemon address. This value is assigned at the launching
  // ibus-daemon and used in bus connection initialization.
  std::string ibus_daemon_address_;

  // The process handle of the IBus daemon. kNullProcessHandle if it's not
  // running.
  base::ProcessHandle process_handle_;

  // Represents ibus-daemon's status.
  IBusDaemonStatus ibus_daemon_status_;

  // The task runner of UI thread.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // The task runner of FILE thread.
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  ObserverList<Observer> observers_;
  base::ThreadChecker thread_checker_;

  // Used for making callbacks for PostTask.
  base::WeakPtrFactory<IBusDaemonControllerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IBusDaemonControllerImpl);
};

// The stub implementation of IBusDaemonController.
class IBusDaemonControllerStubImpl : public IBusDaemonController {
 public:
  IBusDaemonControllerStubImpl() {}
  virtual ~IBusDaemonControllerStubImpl() {}

  // IBusDaemonController overrides:
  virtual void AddObserver(Observer* observer) OVERRIDE {}
  virtual void RemoveObserver(Observer* observer) OVERRIDE {}
  virtual bool Start() OVERRIDE { return true; }
  virtual bool Stop() OVERRIDE { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusDaemonControllerStubImpl);
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// IBusDaemonController

IBusDaemonController::IBusDaemonController() {
}

IBusDaemonController::~IBusDaemonController() {
}

// static
void IBusDaemonController::Initialize(
    const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& file_task_runner) {
  DCHECK(g_ibus_daemon_controller == NULL)
      << "Do not call Initialize function multiple times.";
  if (base::chromeos::IsRunningOnChromeOS()) {
    g_ibus_daemon_controller = new IBusDaemonControllerImpl(ui_task_runner,
                                                            file_task_runner);
  } else {
    g_ibus_daemon_controller = new IBusDaemonControllerStubImpl();
  }
}

// static
void IBusDaemonController::InitializeForTesting(
    IBusDaemonController* controller_) {
  DCHECK(g_ibus_daemon_controller == NULL);
  DCHECK(controller_);
  g_ibus_daemon_controller = controller_;
}

// static
void IBusDaemonController::Shutdown() {
  delete g_ibus_daemon_controller;
  g_ibus_daemon_controller = NULL;
}

// static
IBusDaemonController* IBusDaemonController::GetInstance() {
  return g_ibus_daemon_controller;
}

}  // namespace chromeos
