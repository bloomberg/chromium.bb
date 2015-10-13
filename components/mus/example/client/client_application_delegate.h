// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_EXAMPLE_CLIENT_CLIENT_APPLICATION_DELEGATE_H_
#define COMPONENTS_MUS_EXAMPLE_CLIENT_CLIENT_APPLICATION_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/cpp/view_tree_delegate.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory_impl.h"

namespace mandoline {
class AuraInit;
}

class ClientApplicationDelegate : public mojo::ApplicationDelegate,
                                  public mus::ViewTreeDelegate {
 public:
  ClientApplicationDelegate();
  ~ClientApplicationDelegate() override;

  // Overridden from ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;

 private:
  // ApplicationDelegate:
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // mus::ViewTreeDelegate:
  void OnEmbed(mus::View* root) override;
  void OnConnectionLost(mus::ViewTreeConnection* connection) override;

  mojo::ApplicationImpl* app_;

  scoped_ptr<mandoline::AuraInit> aura_init_;

  DISALLOW_COPY_AND_ASSIGN(ClientApplicationDelegate);
};

#endif  // COMPONENTS_MUS_EXAMPLE_CLIENT_CLIENT_APPLICATION_DELEGATE_H_
