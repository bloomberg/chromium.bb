// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_EXAMPLE_VIEWS_EXAMPLES_APPLICATION_DELEGATE_H_
#define MASH_EXAMPLE_VIEWS_EXAMPLES_APPLICATION_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/application_delegate.h"

namespace views {
class AuraInit;
}

class ViewsExamplesApplicationDelegate : public mojo::ApplicationDelegate {
 public:
  ViewsExamplesApplicationDelegate();
  ~ViewsExamplesApplicationDelegate() override;

 private:
  // ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  mojo::TracingImpl tracing_;

  scoped_ptr<views::AuraInit> aura_init_;

  DISALLOW_COPY_AND_ASSIGN(ViewsExamplesApplicationDelegate);
};

#endif  // MASH_EXAMPLE_VIEWS_EXAMPLES_APPLICATION_DELEGATE_H_
