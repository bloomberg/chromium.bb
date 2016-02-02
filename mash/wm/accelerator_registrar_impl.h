// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_ACCELERATOR_REGISTRAR_IMPL_H_
#define MASH_WM_ACCELERATOR_REGISTRAR_IMPL_H_

#include <stdint.h>

#include <map>

#include "base/callback.h"
#include "base/macros.h"
#include "components/mus/public/interfaces/accelerator_registrar.mojom.h"
#include "mash/wm/root_windows_observer.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace mash {
namespace wm {

class WindowManagerApplication;

// Manages AcceleratorHandlers from a particular AcceleratorRegistrar
// connection. This manages its own lifetime, and destroys itself when the
// AcceleratorRegistrar and all its AcceleratorHandlers are disconnected. Upon
// destruction, it calls the DestroyCallback.
class AcceleratorRegistrarImpl : public mus::mojom::AcceleratorRegistrar,
                                 public RootWindowsObserver {
 public:
  using DestroyCallback = base::Callback<void(AcceleratorRegistrarImpl*)>;
  AcceleratorRegistrarImpl(WindowManagerApplication* wm_app,
                           uint32_t accelerator_namespace,
                           mojo::InterfaceRequest<AcceleratorRegistrar> request,
                           const DestroyCallback& destroy_callback);

  void Destroy();

  // Returns true if this AcceleratorRegistrar has an accelerator with the
  // specified id.
  bool OwnsAccelerator(uint32_t accelerator_id) const;

  void ProcessAccelerator(uint32_t accelerator_id, mus::mojom::EventPtr event);

 private:
  struct Accelerator;

  ~AcceleratorRegistrarImpl() override;

  uint32_t ComputeAcceleratorId(uint32_t accelerator_id) const;
  void OnBindingGone();
  void OnHandlerGone();

  void AddAcceleratorToRoot(RootWindowController* root,
                            uint32_t namespaced_accelerator_id);

  void RemoveAllAccelerators();

  // mus::mojom::AcceleratorRegistrar:
  void SetHandler(mus::mojom::AcceleratorHandlerPtr handler) override;
  void AddAccelerator(uint32_t accelerator_id,
                      mus::mojom::EventMatcherPtr matcher,
                      const AddAcceleratorCallback& callback) override;
  void RemoveAccelerator(uint32_t accelerator_id) override;

  // RootWindowsObserver:
  void OnRootWindowControllerAdded(RootWindowController* controller) override;

  WindowManagerApplication* wm_app_;
  mus::mojom::AcceleratorHandlerPtr accelerator_handler_;
  mojo::Binding<AcceleratorRegistrar> binding_;
  uint32_t accelerator_namespace_;
  std::map<uint32_t, Accelerator> accelerators_;
  DestroyCallback destroy_callback_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorRegistrarImpl);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_ACCELERATOR_REGISTRAR_IMPL_H_
