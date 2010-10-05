// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SERVICE_PROCESS_UTIL_H_
#define CHROME_COMMON_SERVICE_PROCESS_UTIL_H_

#include <string>

class Profile;

// Return the IPC channel to connect to the service process.
//
// TODO(hclam): Need more information to come up with the channel name.
std::string GetServiceProcessChannelName();

// The following methods are used as a mechanism to signal a service process
// is running properly and all initialized.
//

// Tries to become the sole service process for the current user data dir.
// Returns false is another service process is already running.
bool TakeServiceProcessSingletonLock();

// Signal that the service process is running.
// This method is called when the service process is running and initialized.
void SignalServiceProcessRunning();

// Signal that the service process is stopped.
void SignalServiceProcessStopped();

// This method checks that if the service process is running (ready to receive
// IPC commands).
bool CheckServiceProcessRunning();

#endif  // CHROME_COMMON_SERVICE_PROCESS_UTIL_H_
