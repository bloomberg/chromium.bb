// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SERVICE_PROCESS_UTIL_H_
#define CHROME_COMMON_SERVICE_PROCESS_UTIL_H_

#include <string>

#include "base/process.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/task.h"

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

// Returns the process id of the currently running service process. Returns 0
// if no service process is running.
// Note: DO NOT use this check whether the service process is ready because
// a non-zero return value only means that the process is running and not that
// it is ready to receive IPC commands. This method is only exposed for testing.
base::ProcessId GetServiceProcessPid();
// --------------------------------------------------------------------------

// Forces a service process matching the specified version to shut down.
bool ForceServiceProcessShutdown(const std::string& version);

// This is a class that is used by the service process to signal events and
// share data with external clients. This class lives in this file because the
// internal data structures and mechanisms used by the utility methods above
// and this class are shared.
class ServiceProcessState {
 public:
  ServiceProcessState();
  ~ServiceProcessState();

  // Tries to become the sole service process for the current user data dir.
  // Returns false is another service process is already running.
  bool Initialize();

  // Signal that the service process is ready.
  // This method is called when the service process is running and initialized.
  // |shutdown_task| is invoked when we get a shutdown request from another
  // process (in the same thread that called SignalReady). It can be NULL.
  void SignalReady(Task* shutdown_task);

  // Signal that the service process is stopped.
  void SignalStopped();

  // Register the service process to run on startup.
  bool AddToAutoRun();

  // Unregister the service process to run on startup.
  bool RemoveFromAutoRun();
 private:

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

  // Allows each platform to specify whether it supports killing older versions.
  bool ShouldHandleOtherVersion();
  // An opaque object that maintains state. The actual definition of this is
  // platform dependent.
  struct StateData;
  StateData* state_;
  scoped_ptr<base::SharedMemory> shared_mem_service_data_;
};

#endif  // CHROME_COMMON_SERVICE_PROCESS_UTIL_H_
