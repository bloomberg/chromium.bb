// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/input/public/input_manager.h"

#include "athena/input/accelerator_manager_impl.h"
#include "base/logging.h"
#include "ui/aura/client/event_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/events/event_target.h"

namespace athena {
namespace {

InputManager* instance = NULL;

class InputManagerImpl : public InputManager,
                         public ui::EventTarget,
                         public aura::client::EventClient {
 public:
  InputManagerImpl();
  virtual ~InputManagerImpl();

  void Init();
  void Shutdown();

 private:
  // InputManager:
  virtual void OnRootWindowCreated(aura::Window* root_window) OVERRIDE;
  virtual ui::EventTarget* GetTopmostEventTarget() OVERRIDE { return this; }
  virtual AcceleratorManager* GetAcceleratorManager() OVERRIDE {
    return accelerator_manager_.get();
  }

  // Overridden from aura::client::EventClient:
  virtual bool CanProcessEventsWithinSubtree(
      const aura::Window* window) const OVERRIDE {
    return window && !window->ignore_events();
  }
  virtual ui::EventTarget* GetToplevelEventTarget() OVERRIDE { return this; }

  // ui::EventTarget:
  virtual bool CanAcceptEvent(const ui::Event& event) OVERRIDE;
  virtual ui::EventTarget* GetParentTarget() OVERRIDE;
  virtual scoped_ptr<ui::EventTargetIterator> GetChildIterator() const OVERRIDE;
  virtual ui::EventTargeter* GetEventTargeter() OVERRIDE;
  virtual void OnEvent(ui::Event* event) OVERRIDE;

  scoped_ptr<AcceleratorManagerImpl> accelerator_manager_;

  DISALLOW_COPY_AND_ASSIGN(InputManagerImpl);
};

InputManagerImpl::InputManagerImpl()
    : accelerator_manager_(
          AcceleratorManagerImpl::CreateGlobalAcceleratorManager()) {
  DCHECK(!instance);
  instance = this;
}

InputManagerImpl::~InputManagerImpl() {
  DCHECK_EQ(instance, this);
  Shutdown();
  instance = NULL;
}

void InputManagerImpl::Init() {
  accelerator_manager_->Init();
}

void InputManagerImpl::Shutdown() {
  accelerator_manager_.reset();
}

void InputManagerImpl::OnRootWindowCreated(aura::Window* root_window) {
  aura::client::SetEventClient(root_window, this);
  accelerator_manager_->OnRootWindowCreated(root_window);
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
  return NULL;
}

void InputManagerImpl::OnEvent(ui::Event* event) {
}

}  // namespace

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
