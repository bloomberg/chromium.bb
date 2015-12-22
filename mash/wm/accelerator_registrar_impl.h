// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_ACCELERATOR_REGISTRAR_IMPL_H_
#define MASH_WM_ACCELERATOR_REGISTRAR_IMPL_H_

#include <stdint.h>

#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "components/mus/public/interfaces/accelerator_registrar.mojom.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace mus {
namespace mojom {
class WindowTreeHost;
}
}

namespace mash {
namespace wm {

class WindowManagerApplication;

// Manages AcceleratorHandlers from a particular AcceleratorRegistrar
// connection. This manages its own lifetime, and destroys itself when the
// AcceleratorRegistrar and all its AcceleratorHandlers are disconnected. Upon
// destruction, it calls the DestroyCallback.
class AcceleratorRegistrarImpl : public mus::mojom::AcceleratorRegistrar {
 public:
  using DestroyCallback = base::Callback<void(AcceleratorRegistrarImpl*)>;
  AcceleratorRegistrarImpl(mus::mojom::WindowTreeHost* host,
                           uint32_t accelerator_namespace,
                           mojo::InterfaceRequest<AcceleratorRegistrar> request,
                           const DestroyCallback& destroy_callback);

  bool OwnsAccelerator(uint32_t accelerator_id) const;
  void ProcessAccelerator(uint32_t accelerator_id, mus::mojom::EventPtr event);

 private:
  ~AcceleratorRegistrarImpl() override;

  uint32_t ComputeAcceleratorId(uint32_t accelerator_id) const;
  void OnBindingGone();
  void OnHandlerGone();

  // mus::mojom::AcceleratorRegistrar:
  void SetHandler(mus::mojom::AcceleratorHandlerPtr handler) override;
  void AddAccelerator(uint32_t accelerator_id,
                      mus::mojom::EventMatcherPtr matcher,
                      const AddAcceleratorCallback& callback) override;
  void RemoveAccelerator(uint32_t accelerator_id) override;

  mus::mojom::WindowTreeHost* host_;
  mus::mojom::AcceleratorHandlerPtr accelerator_handler_;
  mojo::Binding<AcceleratorRegistrar> binding_;
  uint32_t accelerator_namespace_;
  std::set<uint32_t> accelerator_ids_;
  DestroyCallback destroy_callback_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorRegistrarImpl);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_ACCELERATOR_REGISTRAR_IMPL_H_
