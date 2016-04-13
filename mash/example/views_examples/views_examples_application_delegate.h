// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_EXAMPLE_VIEWS_EXAMPLES_APPLICATION_DELEGATE_H_
#define MASH_EXAMPLE_VIEWS_EXAMPLES_APPLICATION_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/tracing/public/cpp/tracing_impl.h"

namespace views {
class AuraInit;
}

class ViewsExamplesApplicationDelegate : public mojo::ShellClient {
 public:
  ViewsExamplesApplicationDelegate();
  ~ViewsExamplesApplicationDelegate() override;

 private:
  // mojo::ShellClient:
  void Initialize(mojo::Connector* connector, const mojo::Identity& identity,
                  uint32_t id) override;
  bool AcceptConnection(mojo::Connection* connection) override;

  mojo::TracingImpl tracing_;

  std::unique_ptr<views::AuraInit> aura_init_;

  DISALLOW_COPY_AND_ASSIGN(ViewsExamplesApplicationDelegate);
};

#endif  // MASH_EXAMPLE_VIEWS_EXAMPLES_APPLICATION_DELEGATE_H_
