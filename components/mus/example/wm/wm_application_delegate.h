// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_EXAMPLE_WM_WM_APPLICATION_DELEGATE_H_
#define COMPONENTS_MUS_EXAMPLE_WM_WM_APPLICATION_DELEGATE_H_

#include "base/macros.h"
#include "components/mus/example/wm/wm.mojom.h"
#include "components/mus/public/interfaces/view_tree_host.mojom.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory_impl.h"
#include "mojo/common/weak_binding_set.h"

class WM;

class WMApplicationDelegate : public mojo::ApplicationDelegate,
                              public mojo::InterfaceFactory<mojom::WM> {
 public:
  WMApplicationDelegate();
  ~WMApplicationDelegate() override;

  // Overridden from ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;

 private:
  // ApplicationDelegate:
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // InterfaceFactory<mojom::WM>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojom::WM> request) override;

  scoped_ptr<WM> wm_;
  mojo::ViewTreeHostPtr host_;
  mojo::WeakBindingSet<mojom::WM> wm_bindings_;

  DISALLOW_COPY_AND_ASSIGN(WMApplicationDelegate);
};

#endif  // COMPONENTS_MUS_EXAMPLE_WM_WM_APPLICATION_DELEGATE_H_
