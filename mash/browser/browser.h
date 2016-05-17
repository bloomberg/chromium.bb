// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_BROWSER_BROWSER_H_
#define MASH_BROWSER_BROWSER_H_

#include <memory>

#include "base/macros.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/tracing/public/cpp/tracing_impl.h"

namespace views {
class AuraInit;
class Widget;
}

namespace mash {
namespace browser {

class Browser : public shell::ShellClient,
                public mojom::Launchable,
                public shell::InterfaceFactory<mojom::Launchable> {
 public:
  Browser();
  ~Browser() override;

  // Start/stop tracking individual browser windows. When we no longer track any
  // browser windows, the application terminates. The Browser object does not
  // own these widgets.
  void AddWindow(views::Widget* window);
  void RemoveWindow(views::Widget* window);

 private:
  // shell::ShellClient:
  void Initialize(shell::Connector* connector,
                  const shell::Identity& identity,
                  uint32_t id) override;
  bool AcceptConnection(shell::Connection* connection) override;

  // mojom::Launchable:
  void Launch(uint32_t what, mojom::LaunchMode how) override;

  // shell::InterfaceFactory<mojom::Launchable>:
  void Create(shell::Connection* connection,
              mojom::LaunchableRequest request) override;

  shell::Connector* connector_ = nullptr;
  mojo::BindingSet<mojom::Launchable> bindings_;
  std::vector<views::Widget*> windows_;

  mojo::TracingImpl tracing_;
  std::unique_ptr<views::AuraInit> aura_init_;

  DISALLOW_COPY_AND_ASSIGN(Browser);
};

}  // namespace browser
}  // namespace mash

#endif  // MASH_BROWSER_BROWSER_H_
