// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_ACCELERATORS_ACCELERATOR_REGISTRAR_IMPL_H_
#define ASH_MUS_ACCELERATORS_ACCELERATOR_REGISTRAR_IMPL_H_

#include <stdint.h>

#include <map>

#include "ash/mus/accelerators/accelerator_handler.h"
#include "ash/mus/window_manager_observer.h"
#include "base/callback.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ui/public/interfaces/accelerator_registrar.mojom.h"
#include "ui/base/accelerators/accelerator.h"

namespace ash {
namespace mus {

class WindowManager;

// Manages AcceleratorHandlers from a particular AcceleratorRegistrar
// connection. This manages its own lifetime, and destroys itself when the
// AcceleratorRegistrar and all its AcceleratorHandlers are disconnected. Upon
// destruction, it calls the DestroyCallback.
class AcceleratorRegistrarImpl : public ui::mojom::AcceleratorRegistrar,
                                 public WindowManagerObserver,
                                 public AcceleratorHandler,
                                 public ui::AcceleratorTarget {
 public:
  using DestroyCallback = base::Callback<void(AcceleratorRegistrarImpl*)>;
  AcceleratorRegistrarImpl(WindowManager* window_manager,
                           uint16_t accelerator_namespace,
                           mojo::InterfaceRequest<AcceleratorRegistrar> request,
                           const DestroyCallback& destroy_callback);

  void Destroy();

  // Returns true if this AcceleratorRegistrar has an accelerator with the
  // specified id.
  bool OwnsAccelerator(uint32_t accelerator_id) const;

  void ProcessAccelerator(uint32_t accelerator_id, const ui::Event& event);

 private:
  ~AcceleratorRegistrarImpl() override;

  void OnBindingGone();
  void OnHandlerGone();

  void RemoveAllAccelerators();

  // If |matcher| identifies a key-binding accelerator registers it and
  // returns true, returns false otherwise.
  bool AddAcceleratorForKeyBinding(uint32_t accelerator_id,
                                   const ui::mojom::EventMatcher& matcher,
                                   const AddAcceleratorCallback& callback);

  // ui::mojom::AcceleratorRegistrar:
  void SetHandler(ui::mojom::AcceleratorHandlerPtr handler) override;
  void AddAccelerator(uint32_t accelerator_id,
                      ui::mojom::EventMatcherPtr matcher,
                      const AddAcceleratorCallback& callback) override;
  void RemoveAccelerator(uint32_t accelerator_id) override;

  // AcceleratorHandler:
  ui::mojom::EventResult OnAccelerator(uint32_t id,
                                       const ui::Event& event) override;

  // WindowManagerObserver:
  void OnWindowTreeClientDestroyed() override;

  // AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  WindowManager* window_manager_;
  ui::mojom::AcceleratorHandlerPtr accelerator_handler_;
  mojo::Binding<AcceleratorRegistrar> binding_;
  uint16_t accelerator_namespace_;
  // Only contains non-keyboard accelerators.
  std::set<uint32_t> accelerators_;
  DestroyCallback destroy_callback_;

  // Used only for keyboard accelerators.
  std::map<ui::Accelerator, uint16_t> keyboard_accelerator_to_id_;
  std::map<uint16_t, ui::Accelerator> id_to_keyboard_accelerator_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorRegistrarImpl);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_ACCELERATORS_ACCELERATOR_REGISTRAR_IMPL_H_
