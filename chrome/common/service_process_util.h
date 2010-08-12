// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SERVICE_PROCESS_UTIL_H_
#define CHROME_COMMON_SERVICE_PROCESS_UTIL_H_

#include <string>

#include "chrome/common/service_process_type.h"

class Profile;

// Return the IPC channel to connect to the service process with |profile|
// and |type|.
//
// TODO(hclam): Need more information to come up with the channel name.
std::string GetServiceProcessChannelName(ServiceProcessType type);

// The following methods are used as a mechanism to signal a service process
// is running properly and all initialized.
//
// The way it works is that we will create a file on the file system to indicate
// that service process is running. This way the browser process will know that
// it can connect to it without problem.
//
// When the service process this lock file is deleted.

// This method is called when the service process is running and initialized.
// Return true if lock file was created.
bool CreateServiceProcessLockFile(ServiceProcessType type);

// This method deletes the lock file created by this above method.
// Return true if lock file was deleted.
bool DeleteServiceProcessLockFile(ServiceProcessType type);

// This method checks that if the service process lock file exists. This means
// that the service process is running.
// TODO(hclam): We should use a better mechanism to detect that a service
// process is running.
bool CheckServiceProcessRunning(ServiceProcessType type);

#endif  // CHROME_COMMON_SERVICE_PROCESS_UTIL_H_
