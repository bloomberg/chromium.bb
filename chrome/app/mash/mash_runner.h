// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_MASH_MASH_RUNNER_H_
#define CHROME_APP_MASH_MASH_RUNNER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "services/shell/public/interfaces/shell_client.mojom.h"

namespace shell {
class ShellClient;
class ShellConnection;
}

// Responsible for running mash, both child and main processes.
class MashRunner {
 public:
  MashRunner();
  ~MashRunner();

  void Run();

 private:
  void RunMain();
  void RunChild();

  void StartChildApp(shell::mojom::ShellClientRequest client_request);

  scoped_ptr<shell::ShellClient> shell_client_;
  scoped_ptr<shell::ShellConnection> shell_connection_;

  DISALLOW_COPY_AND_ASSIGN(MashRunner);
};

int MashMain();

#endif  // CHROME_APP_MASH_MASH_RUNNER_H_
