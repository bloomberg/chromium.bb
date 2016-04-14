// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_QUICK_LAUNCH_QUICK_LAUNCH_APPLICATION_H_
#define MASH_QUICK_LAUNCH_QUICK_LAUNCH_APPLICATION_H_

#include <memory>

#include "base/macros.h"
#include "services/catalog/public/interfaces/catalog.mojom.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/tracing/public/cpp/tracing_impl.h"

namespace views {
class AuraInit;
}

namespace mash {
namespace quick_launch {

class QuickLaunchApplication : public shell::ShellClient {
 public:
  QuickLaunchApplication();
  ~QuickLaunchApplication() override;

 private:
  // shell::ShellClient:
  void Initialize(shell::Connector* connector,
                  const shell::Identity& identity,
                  uint32_t id) override;
  bool AcceptConnection(shell::Connection* connection) override;

  mojo::TracingImpl tracing_;
  std::unique_ptr<views::AuraInit> aura_init_;

  DISALLOW_COPY_AND_ASSIGN(QuickLaunchApplication);
};

}  // namespace quick_launch
}  // namespace mash

#endif  // MASH_QUICK_LAUNCH_QUICK_LAUNCH_APPLICATION_H_
