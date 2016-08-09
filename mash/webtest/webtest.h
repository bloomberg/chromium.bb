// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WEBTEST_WEBTEST_H_
#define MASH_WEBTEST_WEBTEST_H_

#include <memory>

#include "base/macros.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/service.h"
#include "services/tracing/public/cpp/provider.h"

namespace views {
class AuraInit;
class Widget;
class WindowManagerConnection;
}

namespace mash {
namespace webtest {

class Webtest
    : public shell::Service,
      public mojom::Launchable,
      public shell::InterfaceFactory<mojom::Launchable> {
 public:
  Webtest();
  ~Webtest() override;

  void AddWindow(views::Widget* window);
  void RemoveWindow(views::Widget* window);

 private:
  // shell::Service:
  void OnStart(const shell::Identity& identity) override;
  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry) override;

  // mojom::Launchable:
  void Launch(uint32_t what, mojom::LaunchMode how) override;

  // shell::InterfaceFactory<mojom::Launchable>:
  void Create(const shell::Identity& remote_identity,
              mojom::LaunchableRequest request) override;

  mojo::BindingSet<mojom::Launchable> bindings_;
  std::vector<views::Widget*> windows_;

  tracing::Provider tracing_;
  std::unique_ptr<views::AuraInit> aura_init_;
  std::unique_ptr<views::WindowManagerConnection> window_manager_connection_;

  DISALLOW_COPY_AND_ASSIGN(Webtest);
};

}  // namespace webtest
}  // namespace mash

#endif  // MASH_WEBTEST_WEBTEST_H_
