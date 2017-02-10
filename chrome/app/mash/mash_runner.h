// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_MASH_MASH_RUNNER_H_
#define CHROME_APP_MASH_MASH_RUNNER_H_

#include <memory>

#include "base/macros.h"
#include "services/service_manager/public/interfaces/service.mojom.h"

// Responsible for running mash, both child and main processes.
class MashRunner {
 public:
  MashRunner();
  ~MashRunner();

  // Returns 0 if the process was initialized correctly, or error code on
  // failure.
  int Run();

 private:
  // Runs the main process, including the service manager. Returns the exit
  // value for the process.
  int RunMain();

  // Runs a background service manager in the main process. Returns the exit
  // value for the process.
  int RunServiceManagerInMain();

  // Returns 0 if the child process was initialized correctly, or error code on
  // failure.
  int RunChild();

  void StartChildApp(service_manager::mojom::ServiceRequest service_request);

  DISALLOW_COPY_AND_ASSIGN(MashRunner);
};

// Called during chrome --mash startup instead of ContentMain().
int MashMain();

// Called if the command line isn't mash to potentially wait for a debugger
// to attach.
void WaitForMashDebuggerIfNecessary();

#endif  // CHROME_APP_MASH_MASH_RUNNER_H_
