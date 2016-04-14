// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_SYSUI_APPLICATION_H_
#define ASH_MUS_SYSUI_APPLICATION_H_

#include <memory>

#include "base/macros.h"
#include "mash/shelf/public/interfaces/shelf.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/tracing/public/cpp/tracing_impl.h"

namespace ash {
namespace sysui {

class AshInit;

class SysUIApplication
    : public shell::ShellClient,
      public shell::InterfaceFactory<mash::shelf::mojom::ShelfController> {
 public:
  SysUIApplication();
  ~SysUIApplication() override;

 private:
  // shell::ShellClient:
  void Initialize(::shell::Connector* connector,
                  const ::shell::Identity& identity,
                  uint32_t id) override;
  bool AcceptConnection(shell::Connection* connection) override;

  // InterfaceFactory<mash::shelf::mojom::ShelfController>:
  void Create(shell::Connection* connection,
              mojo::InterfaceRequest<mash::shelf::mojom::ShelfController>
                  request) override;

  mojo::TracingImpl tracing_;
  std::unique_ptr<AshInit> ash_init_;

  mojo::BindingSet<mash::shelf::mojom::ShelfController>
      shelf_controller_bindings_;

  DISALLOW_COPY_AND_ASSIGN(SysUIApplication);
};

}  // namespace sysui
}  // namespace ash

#endif  // ASH_MUS_SYSUI_APPLICATION_H_
