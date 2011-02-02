// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SERVICE_PROCESS_UTIL_H_
#define CHROME_COMMON_SERVICE_PROCESS_UTIL_H_

#include <string>

#include "base/process.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"

class Task;
class MessageLoop;

template <typename T> struct DefaultSingletonTraits;

// Return the IPC channel to connect to the service process.
//
std::string GetServiceProcessChannelName();

// Return a name that is scoped to this instance of the service process. We
// use the user-data-dir as a scoping prefix.
std::string GetServiceProcessScopedName(const std::string& append_str);

// Return a name that is scoped to this instance of the service process. We
// use the user-data-dir and the version as a scoping prefix.
std::string GetServiceProcessScopedVersionedName(const std::string& append_str);

// The following methods are used in a process that acts as a client to the
// service process (typically the browser process).
// --------------------------------------------------------------------------
// This method checks that if the service process is ready to receive
// IPC commands.
bool CheckServiceProcessReady();

// Returns the process id and version of the currently running service process.
// Note: DO NOT use this check whether the service process is ready because
// a true return value only means that some process shared data was available,
// and not that the process is ready to receive IPC commands, or even running.
// This method is only exposed for testing.
bool GetServiceProcessSharedData(std::string* version, base::ProcessId* pid);
// --------------------------------------------------------------------------

// Forces a service process matching the specified version to shut down.
bool ForceServiceProcessShutdown(const std::string& version,
                                 base::ProcessId process_id);

// This is a class that is used by the service process to signal events and
// share data with external clients. This class lives in this file because the
// internal data structures and mechanisms used by the utility methods above
// and this class are shared.
class ServiceProcessState {
 public:
  // Returns the singleton instance.
  static ServiceProcessState* GetInstance();

  // Tries to become the sole service process for the current user data dir.
  // Returns false if another service process is already running.
  bool Initialize();

  // Signal that the service process is ready.
  // This method is called when the service process is running and initialized.
  // |shutdown_task| is invoked when we get a shutdown request from another
  // process (in the same thread that called SignalReady). It can be NULL.
  // |message_loop| must be of type IO and is the loop that POSIX uses
  // to monitor the service process.
  bool SignalReady(MessageLoop *message_loop, Task* shutdown_task);

  // Signal that the service process is stopped.
  void SignalStopped();

  // Register the service process to run on startup.
  bool AddToAutoRun();

  // Unregister the service process to run on startup.
  bool RemoveFromAutoRun();

 private:
  ServiceProcessState();
  ~ServiceProcessState();

  // Create the shared memory data for the service process.
  bool CreateSharedData();

  // If an older version of the service process running, it should be shutdown.
  // Returns false if this process needs to exit.
  bool HandleOtherVersion();

  // Acquires a singleton lock for the service process. A return value of false
  // means that a service process instance is already running.
  bool TakeSingletonLock();

  // Key used to register the service process to auto-run.
  std::string GetAutoRunKey();

  // Tear down the platform specific state.
  void TearDownState();

  // An opaque object that maintains state. The actual definition of this is
  // platform dependent.
  struct StateData;
  StateData* state_;
  scoped_ptr<base::SharedMemory> shared_mem_service_data_;

  friend struct DefaultSingletonTraits<ServiceProcessState>;
};

#endif  // CHROME_COMMON_SERVICE_PROCESS_UTIL_H_
