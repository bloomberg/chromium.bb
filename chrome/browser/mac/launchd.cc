// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/launchd.h"

#include <launch.h>
#include <signal.h>

#include "base/logging.h"
#include "chrome/browser/mac/scoped_launch_data.h"

namespace launchd {

namespace {

// MessageForJob sends a single message to launchd with a simple dictionary
// mapping |operation| to |job_label|, and returns the result of calling
// launch_msg to send that message. On failure, returns NULL. The caller
// assumes ownership of the returned launch_data_t object.
launch_data_t MessageForJob(const std::string& job_label,
                            const char* operation) {
  // launch_data_alloc returns something that needs to be freed.
  ScopedLaunchData message(launch_data_alloc(LAUNCH_DATA_DICTIONARY));
  if (!message) {
    LOG(ERROR) << "launch_data_alloc";
    return NULL;
  }

  // launch_data_new_string returns something that needs to be freed, but
  // the dictionary will assume ownership when launch_data_dict_insert is
  // called, so put it in a scoper and .release() it when given to the
  // dictionary.
  ScopedLaunchData job_label_launchd(launch_data_new_string(job_label.c_str()));
  if (!job_label_launchd) {
    LOG(ERROR) << "launch_data_new_string";
    return NULL;
  }

  if (!launch_data_dict_insert(message,
                               job_label_launchd.release(),
                               operation)) {
    return NULL;
  }

  return launch_msg(message);
}

// Returns the process ID for |job_label|, or -1 on error.
pid_t PIDForJob(const std::string& job_label) {
  ScopedLaunchData response(MessageForJob(job_label, LAUNCH_KEY_GETJOB));
  if (!response) {
    return -1;
  }

  if (launch_data_get_type(response) != LAUNCH_DATA_DICTIONARY) {
    LOG(ERROR) << "PIDForJob: expected dictionary";
    return -1;
  }

  launch_data_t pid_data = launch_data_dict_lookup(response,
                                                   LAUNCH_JOBKEY_PID);
  if (!pid_data) {
    LOG(ERROR) << "PIDForJob: no pid";
    return -1;
  }

  if (launch_data_get_type(pid_data) != LAUNCH_DATA_INTEGER) {
    LOG(ERROR) << "PIDForJob: expected integer";
    return -1;
  }

  return launch_data_get_integer(pid_data);
}

}  // namespace

void SignalJob(const std::string& job_name, int signal) {
  pid_t pid = PIDForJob(job_name);
  if (pid <= 0) {
    return;
  }

  kill(pid, signal);
}

}  // namespace launchd
