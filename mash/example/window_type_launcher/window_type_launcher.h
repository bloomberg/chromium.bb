// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_EXAMPLE_WINDOW_TYPE_LAUNCHER_WINDOW_TYPE_LAUNCHER_H_
#define MASH_EXAMPLE_WINDOW_TYPE_LAUNCHER_WINDOW_TYPE_LAUNCHER_H_

#include <memory>

#include "base/macros.h"
#include "services/shell/public/cpp/shell_client.h"

namespace views {
class AuraInit;
}

class WindowTypeLauncher : public shell::ShellClient {
 public:
  WindowTypeLauncher();
  ~WindowTypeLauncher() override;

 private:
  // shell::ShellClient:
  void Initialize(shell::Connector* connector,
                  const shell::Identity& identity,
                  uint32_t id) override;
  bool ShellConnectionLost() override;

  std::unique_ptr<views::AuraInit> aura_init_;

  DISALLOW_COPY_AND_ASSIGN(WindowTypeLauncher);
};

#endif  // MASH_EXAMPLE_WINDOW_TYPE_LAUNCHER_WINDOW_TYPE_LAUNCHER_H_
