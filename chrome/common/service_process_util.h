// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SERVICE_PROCESS_UTIL_H_
#define CHROME_COMMON_SERVICE_PROCESS_UTIL_H_

#include <string>

#include "chrome/common/service_process_type.h"

class Profile;

// Return the IPC channel to connect to the service process.
//
// TODO(hclam): Need more information to come up with the channel name.
std::string GetServiceProcessChannelName();

// The following methods are used as a mechanism to signal a service process
// is running properly and all initialized.
//
// Signal that the service process is running.
// This method is called when the service process is running and initialized.
void SignalServiceProcessRunning(ServiceProcessType type);

// Signal that the service process is stopped.
void SignalServiceProcessStopped(ServiceProcessType type);

// This method checks that if the service process is running.
bool CheckServiceProcessRunning(ServiceProcessType type);

#endif  // CHROME_COMMON_SERVICE_PROCESS_UTIL_H_
