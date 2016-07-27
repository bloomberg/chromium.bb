// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/accelerators/accelerator_registrar_impl.h"

#include <stdint.h>
#include <utility>

#include "ash/common/accelerators/accelerator_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/mus/accelerators/accelerator_ids.h"
#include "ash/mus/root_window_controller.h"
#include "ash/mus/window_manager.h"
#include "base/bind.h"
#include "services/ui/public/cpp/window_manager_delegate.h"
#include "ui/events/event.h"

namespace ash {
namespace mus {

namespace {

void CallAddAcceleratorCallback(
    const ui::mojom::AcceleratorRegistrar::AddAcceleratorCallback& callback,
    bool result) {
  callback.Run(result);
}

// Returns true if |event_matcher| corresponds to matching an Accelerator.
bool IsMatcherForKeyAccelerator(const ui::mojom::EventMatcher& event_matcher) {
  return (
      event_matcher.accelerator_phase ==
          ui::mojom::AcceleratorPhase::PRE_TARGET &&
      event_matcher.type_matcher &&
      (event_matcher.type_matcher->type == ui::mojom::EventType::KEY_PRESSED ||
       event_matcher.type_matcher->type ==
           ui::mojom::EventType::KEY_RELEASED) &&
      event_matcher.key_matcher && event_matcher.flags_matcher);
}

// Converts |event_matcher| into the ui::Accelerator it matches. Only valid if
// IsMatcherForKeyAccelerator() returns true.
ui::Accelerator EventMatcherToAccelerator(
    const ui::mojom::EventMatcher& event_matcher) {
  DCHECK(IsMatcherForKeyAccelerator(event_matcher));
  ui::Accelerator accelerator(
      static_cast<ui::KeyboardCode>(event_matcher.key_matcher->keyboard_code),
      event_matcher.flags_matcher->flags);
  accelerator.set_type(event_matcher.type_matcher->type ==
                               ui::mojom::EventType::KEY_PRESSED
                           ? ui::ET_KEY_PRESSED
                           : ui::ET_KEY_RELEASED);
  return accelerator;
}

}  // namespace

AcceleratorRegistrarImpl::AcceleratorRegistrarImpl(
    WindowManager* window_manager,
    uint16_t accelerator_namespace,
    mojo::InterfaceRequest<AcceleratorRegistrar> request,
    const DestroyCallback& destroy_callback)
    : window_manager_(window_manager),
      binding_(this, std::move(request)),
      accelerator_namespace_(accelerator_namespace),
      destroy_callback_(destroy_callback) {
  window_manager_->AddObserver(this);
  window_manager_->AddAcceleratorHandler(accelerator_namespace_, this);
  binding_.set_connection_error_handler(base::Bind(
      &AcceleratorRegistrarImpl::OnBindingGone, base::Unretained(this)));
}

void AcceleratorRegistrarImpl::Destroy() {
  delete this;
}

bool AcceleratorRegistrarImpl::OwnsAccelerator(uint32_t accelerator_id) const {
  return !!accelerators_.count(accelerator_id);
}

void AcceleratorRegistrarImpl::ProcessAccelerator(uint32_t accelerator_id,
                                                  const ui::Event& event) {
  DCHECK(OwnsAccelerator(accelerator_id));
  // TODO(moshayedi): crbug.com/617167. Don't clone even once we map
  // mojom::Event directly to ui::Event.
  accelerator_handler_->OnAccelerator(accelerator_id & kLocalIdMask,
                                      ui::Event::Clone(event));
}

AcceleratorRegistrarImpl::~AcceleratorRegistrarImpl() {
  window_manager_->RemoveAcceleratorHandler(accelerator_namespace_);
  window_manager_->RemoveObserver(this);
  RemoveAllAccelerators();
  destroy_callback_.Run(this);
}

void AcceleratorRegistrarImpl::OnBindingGone() {
  binding_.Unbind();
  // If there's no outstanding accelerators for this connection, then destroy
  // it.
  if (accelerators_.empty() && keyboard_accelerator_to_id_.empty())
    delete this;
}

void AcceleratorRegistrarImpl::OnHandlerGone() {
  // The handler is dead. If AcceleratorRegistrar connection is also closed,
  // then destroy this. Otherwise, remove all the accelerators, but keep the
  // AcceleratorRegistrar connection alive (the client could still set another
  // handler and install new accelerators).
  if (!binding_.is_bound()) {
    delete this;
    return;
  }
  accelerator_handler_.reset();
  RemoveAllAccelerators();
}

void AcceleratorRegistrarImpl::RemoveAllAccelerators() {
  for (uint32_t accelerator : accelerators_)
    window_manager_->window_manager_client()->RemoveAccelerator(accelerator);

  WmShell::Get()->accelerator_controller()->UnregisterAll(this);
  keyboard_accelerator_to_id_.clear();
  id_to_keyboard_accelerator_.clear();

  accelerators_.clear();
}

bool AcceleratorRegistrarImpl::AddAcceleratorForKeyBinding(
    uint32_t accelerator_id,
    const ui::mojom::EventMatcher& matcher,
    const AddAcceleratorCallback& callback) {
  if (!IsMatcherForKeyAccelerator(matcher))
    return false;

  const ui::Accelerator accelerator = EventMatcherToAccelerator(matcher);
  if (keyboard_accelerator_to_id_.count(accelerator)) {
    callback.Run(false);
    return true;
  }

  AcceleratorController* accelerator_controller =
      WmShell::Get()->accelerator_controller();
  // TODO(sky): reenable this when we decide on the future of AppDriver.
  // http://crbug.com/631836.
  /*
  if (accelerator_controller->IsRegistered(accelerator)) {
    DVLOG(1) << "Attempt to register accelerator that is already registered";
    callback.Run(false);
    // Even though we're not registering the accelerator it's a key that should
    // be handled as an accelerator, so return true.
    return true;
  }
  */

  const uint16_t local_id = GetAcceleratorLocalId(accelerator_id);
  keyboard_accelerator_to_id_[accelerator] = local_id;
  id_to_keyboard_accelerator_[local_id] = accelerator;
  accelerator_controller->Register(accelerator, this);
  callback.Run(true);
  return true;
}

void AcceleratorRegistrarImpl::SetHandler(
    ui::mojom::AcceleratorHandlerPtr handler) {
  accelerator_handler_ = std::move(handler);
  accelerator_handler_.set_connection_error_handler(base::Bind(
      &AcceleratorRegistrarImpl::OnHandlerGone, base::Unretained(this)));
}

void AcceleratorRegistrarImpl::AddAccelerator(
    uint32_t accelerator_id,
    ui::mojom::EventMatcherPtr matcher,
    const AddAcceleratorCallback& callback) {
  if (!accelerator_handler_ || accelerator_id > 0xFFFF) {
    // The |accelerator_id| is too large, and it can't be handled correctly.
    callback.Run(false);
    DVLOG(1) << "AddAccelerator failed because of bogus id";
    return;
  }

  uint32_t namespaced_accelerator_id = ComputeAcceleratorId(
      accelerator_namespace_, static_cast<uint16_t>(accelerator_id));

  if (accelerators_.count(namespaced_accelerator_id) ||
      id_to_keyboard_accelerator_.count(
          GetAcceleratorLocalId(namespaced_accelerator_id))) {
    callback.Run(false);
    DVLOG(1) << "AddAccelerator failed because id already in use "
             << accelerator_id;
    return;
  }

  if (AddAcceleratorForKeyBinding(accelerator_id, *matcher, callback))
    return;

  accelerators_.insert(namespaced_accelerator_id);
  window_manager_->window_manager_client()->AddAccelerator(
      namespaced_accelerator_id, std::move(matcher),
      base::Bind(&CallAddAcceleratorCallback, callback));
}

void AcceleratorRegistrarImpl::RemoveAccelerator(uint32_t accelerator_id) {
  if (accelerator_id > 0xFFFF)
    return;

  const uint16_t local_id = GetAcceleratorLocalId(accelerator_id);
  auto iter = id_to_keyboard_accelerator_.find(local_id);
  if (iter != id_to_keyboard_accelerator_.end()) {
    WmShell::Get()->accelerator_controller()->Unregister(iter->second, this);
    keyboard_accelerator_to_id_.erase(iter->second);
    id_to_keyboard_accelerator_.erase(iter);
    return;
  }

  uint32_t namespaced_accelerator_id = ComputeAcceleratorId(
      accelerator_namespace_, static_cast<uint16_t>(accelerator_id));
  if (accelerators_.erase(namespaced_accelerator_id) == 0)
    return;
  window_manager_->window_manager_client()->RemoveAccelerator(
      namespaced_accelerator_id);

  // If the registrar is not bound anymore (i.e. the client can no longer
  // install new accelerators), and the last accelerator has been removed, then
  // there's no point keeping this alive anymore.
  if (accelerators_.empty() && keyboard_accelerator_to_id_.empty() &&
      !binding_.is_bound()) {
    delete this;
  }
}

ui::mojom::EventResult AcceleratorRegistrarImpl::OnAccelerator(
    uint32_t id,
    const ui::Event& event) {
  if (OwnsAccelerator(id))
    ProcessAccelerator(id, event);
  return ui::mojom::EventResult::HANDLED;
}

void AcceleratorRegistrarImpl::OnWindowTreeClientDestroyed() {
  delete this;
}

bool AcceleratorRegistrarImpl::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  auto iter = keyboard_accelerator_to_id_.find(accelerator);
  DCHECK(iter != keyboard_accelerator_to_id_.end());
  const ui::KeyEvent key_event(accelerator.type(), accelerator.key_code(),
                               accelerator.modifiers());
  // TODO(moshayedi): crbug.com/617167. Don't clone even once we map
  // mojom::Event directly to ui::Event.
  accelerator_handler_->OnAccelerator(iter->second,
                                      ui::Event::Clone(key_event));
  return true;
}

bool AcceleratorRegistrarImpl::CanHandleAccelerators() const {
  return true;
}

}  // namespace mus
}  // namespace ash
