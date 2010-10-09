// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SERVICE_PROCESS_UTIL_H_
#define CHROME_COMMON_SERVICE_PROCESS_UTIL_H_

#include <string>

#include "base/task.h"

// Return the IPC channel to connect to the service process.
//
std::string GetServiceProcessChannelName();

// The following methods are used within the service process.
// --------------------------------------------------------------------------

// Tries to become the sole service process for the current user data dir.
// Returns false is another service process is already running.
bool TakeServiceProcessSingletonLock();

// Signal that the service process is running.
// This method is called when the service process is running and initialized.
// |shutdown_task| is invoked when we get a shutdown request from another
// process (in the same thread that called SignalServiceProcessRunning).
void SignalServiceProcessRunning(Task* shutdown_task);

// Signal that the service process is stopped.
void SignalServiceProcessStopped();

// Key used to register the service process to auto-run.
std::string GetServiceProcessAutoRunKey();

// This is exposed for testing only. Forces a service process matching the
// specified version to shut down.
bool ForceServiceProcessShutdown(const std::string& version);

// --------------------------------------------------------------------------


// The following methods are used in a process that acts as a client to the
// service process (typically the browser process).

// --------------------------------------------------------------------------

// This method checks that if the service process is running (ready to receive
// IPC commands).

bool CheckServiceProcessRunning();
// --------------------------------------------------------------------------

#endif  // CHROME_COMMON_SERVICE_PROCESS_UTIL_H_

