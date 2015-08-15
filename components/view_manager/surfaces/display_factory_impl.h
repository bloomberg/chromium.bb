// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_SURFACES_DISPLAY_FACTORY_IMPL_H_
#define COMPONENTS_VIEW_MANAGER_SURFACES_DISPLAY_FACTORY_IMPL_H_

#include "components/view_manager/public/interfaces/display.mojom.h"
#include "components/view_manager/surfaces/display_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"

namespace cc {
class SurfaceManager;
}

namespace surfaces {

class DisplayDelegate;
class SurfacesScheduler;
class SurfacesServiceApplication;

class DisplayFactoryImpl : public mojo::DisplayFactory {
 public:
  DisplayFactoryImpl(DisplayDelegate* display_delegate,
                     const scoped_refptr<SurfacesState>& surfaces_state,
                     mojo::InterfaceRequest<mojo::DisplayFactory> request);

  void CloseConnection();

 private:
  ~DisplayFactoryImpl() override;

  // mojo::DisplayFactory implementation.
  void Create(mojo::ContextProviderPtr context_provider,
              mojo::ResourceReturnerPtr returner,
              mojo::InterfaceRequest<mojo::Display> display_request) override;

  // We use one ID namespace for all DisplayImpls since the ID is used only by
  // cc and not exposed through mojom.
  uint32_t id_namespace_;
  uint32_t next_local_id_;
  DisplayDelegate* display_delegate_;
  scoped_refptr<SurfacesState> surfaces_state_;
  mojo::Binding<mojo::DisplayFactory> binding_;
  bool connection_closed_;

  DISALLOW_COPY_AND_ASSIGN(DisplayFactoryImpl);
};

}  // namespace surfaces

#endif  // COMPONENTS_VIEW_MANAGER_SURFACES_DISPLAY_FACTORY_IMPL_H_
