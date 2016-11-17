// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_MASH_MASH_RUNNER_H_
#define CHROME_APP_MASH_MASH_RUNNER_H_

#include <memory>

#include "base/macros.h"
#include "services/service_manager/public/interfaces/service.mojom.h"

namespace service_manager {
class ServiceContext;
}

// Responsible for running mash, both child and main processes.
class MashRunner {
 public:
  MashRunner();
  ~MashRunner();

  // Returns 0 if the process was initialized correctly, or error code on
  // failure.
  int Run();

 private:
  void RunMain();

  // Returns 0 if the child process was initialized correctly, or error code on
  // failure.
  int RunChild();

  void StartChildApp(service_manager::mojom::ServiceRequest service_request);

  std::unique_ptr<service_manager::ServiceContext> context_;

  DISALLOW_COPY_AND_ASSIGN(MashRunner);
};

int MashMain();

#endif  // CHROME_APP_MASH_MASH_RUNNER_H_
