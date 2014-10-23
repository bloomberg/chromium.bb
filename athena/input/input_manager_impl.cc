// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/input/input_manager_impl.h"

#include "athena/input/power_button_controller.h"
#include "base/logging.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"

namespace athena {
namespace {

InputManager* instance = nullptr;

}  // namespace

InputManagerImpl::InputManagerImpl()
    : accelerator_manager_(
          AcceleratorManagerImpl::CreateGlobalAcceleratorManager()),
      power_button_controller_(new PowerButtonController) {
  DCHECK(!instance);
  instance = this;
}

InputManagerImpl::~InputManagerImpl() {
  DCHECK_EQ(instance, this);
  Shutdown();
  instance = nullptr;
}

void InputManagerImpl::Init() {
  accelerator_manager_->Init();
  power_button_controller_->InstallAccelerators();
}

void InputManagerImpl::Shutdown() {
  accelerator_manager_.reset();
}

void InputManagerImpl::OnRootWindowCreated(aura::Window* root_window) {
  aura::client::SetEventClient(root_window, this);
  accelerator_manager_->OnRootWindowCreated(root_window);
}

ui::EventTarget* InputManagerImpl::GetTopmostEventTarget() {
  return this;
}

AcceleratorManager* InputManagerImpl::GetAcceleratorManager() {
  return accelerator_manager_.get();
}

void InputManagerImpl::AddPowerButtonObserver(PowerButtonObserver* observer) {
  power_button_controller_->AddPowerButtonObserver(observer);
}
void InputManagerImpl::RemovePowerButtonObserver(
    PowerButtonObserver* observer) {
  power_button_controller_->RemovePowerButtonObserver(observer);
}

bool InputManagerImpl::CanProcessEventsWithinSubtree(
    const aura::Window* window) const {
  return window && !window->ignore_events();
}

ui::EventTarget* InputManagerImpl::GetToplevelEventTarget() {
  return this;
}

bool InputManagerImpl::CanAcceptEvent(const ui::Event& event) {
  return true;
}

ui::EventTarget* InputManagerImpl::GetParentTarget() {
  return aura::Env::GetInstance();
}

scoped_ptr<ui::EventTargetIterator> InputManagerImpl::GetChildIterator() const {
  return scoped_ptr<ui::EventTargetIterator>();
}

ui::EventTargeter* InputManagerImpl::GetEventTargeter() {
  NOTREACHED();
  return nullptr;
}

void InputManagerImpl::OnEvent(ui::Event* event) {
}

int InputManagerImpl::SetPowerButtonTimeoutMsForTest(int timeout) {
  return power_button_controller_->SetPowerButtonTimeoutMsForTest(timeout);
}

// static
InputManager* InputManager::Create() {
  (new InputManagerImpl)->Init();
  DCHECK(instance);
  return instance;
}

// static
InputManager* InputManager::Get() {
  DCHECK(instance);
  return instance;
}

// static
void InputManager::Shutdown() {
  DCHECK(instance);
  delete instance;
  DCHECK(!instance);
}

}  // namespace athena
