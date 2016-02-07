// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_EXAMPLE_VIEWS_EXAMPLES_APPLICATION_DELEGATE_H_
#define MASH_EXAMPLE_VIEWS_EXAMPLES_APPLICATION_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/shell_client.h"

namespace views {
class AuraInit;
}

class ViewsExamplesApplicationDelegate : public mojo::ShellClient {
 public:
  ViewsExamplesApplicationDelegate();
  ~ViewsExamplesApplicationDelegate() override;

 private:
  // mojo::ShellClient:
  void Initialize(mojo::Shell* shell, const std::string& url,
                  uint32_t id) override;
  bool AcceptConnection(mojo::Connection* connection) override;

  mojo::TracingImpl tracing_;

  scoped_ptr<views::AuraInit> aura_init_;

  DISALLOW_COPY_AND_ASSIGN(ViewsExamplesApplicationDelegate);
};

#endif  // MASH_EXAMPLE_VIEWS_EXAMPLES_APPLICATION_DELEGATE_H_
