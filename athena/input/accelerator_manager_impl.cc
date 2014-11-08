// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/input/accelerator_manager_impl.h"

#include "athena/input/public/input_manager.h"
#include "athena/util/switches.h"
#include "base/logging.h"
#include "ui/aura/window.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/events/event.h"
#include "ui/events/event_target.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/focus_manager_delegate.h"
#include "ui/views/focus/focus_manager_factory.h"
#include "ui/wm/core/accelerator_delegate.h"
#include "ui/wm/core/accelerator_filter.h"
#include "ui/wm/core/nested_accelerator_controller.h"
#include "ui/wm/core/nested_accelerator_delegate.h"
#include "ui/wm/public/dispatcher_client.h"

namespace athena {

// This wrapper interface provides a common interface that handles global
// accelerators as well as local accelerators.
class AcceleratorManagerImpl::AcceleratorWrapper {
 public:
  virtual ~AcceleratorWrapper() {}
  virtual void Register(const ui::Accelerator& accelerator,
                        ui::AcceleratorTarget* target) = 0;
  virtual void Unregister(const ui::Accelerator& accelerator,
                          ui::AcceleratorTarget* target) = 0;
  virtual bool Process(const ui::Accelerator& accelerator) = 0;
  virtual ui::AcceleratorTarget* GetCurrentTarget(
      const ui::Accelerator& accelerator) const = 0;
};

namespace {

// Accelerators inside nested message loop are handled by
// wm::NestedAcceleratorController while accelerators in normal case are
// handled by wm::AcceleratorFilter. These delegates act bridges in these
// two different environment so that AcceleratorManagerImpl can handle
// accelerators in an uniform way.

class NestedAcceleratorDelegate : public wm::NestedAcceleratorDelegate {
 public:
  explicit NestedAcceleratorDelegate(
      AcceleratorManagerImpl* accelerator_manager)
      : accelerator_manager_(accelerator_manager) {}
  ~NestedAcceleratorDelegate() override {}

 private:
  // wm::NestedAcceleratorDelegate:
  Result ProcessAccelerator(const ui::Accelerator& accelerator) override {
    return accelerator_manager_->Process(accelerator) ? RESULT_PROCESSED
                                                      : RESULT_NOT_PROCESSED;
  }

  AcceleratorManagerImpl* accelerator_manager_;

  DISALLOW_COPY_AND_ASSIGN(NestedAcceleratorDelegate);
};

class AcceleratorDelegate : public wm::AcceleratorDelegate {
 public:
  explicit AcceleratorDelegate(AcceleratorManagerImpl* accelerator_manager)
      : accelerator_manager_(accelerator_manager) {}
  ~AcceleratorDelegate() override {}

 private:
  // wm::AcceleratorDelegate:
  bool ProcessAccelerator(const ui::KeyEvent& event,
                          const ui::Accelerator& accelerator,
                          KeyType key_type) override {
    aura::Window* target = static_cast<aura::Window*>(event.target());
    if (!target->IsRootWindow() &&
        !accelerator_manager_->IsRegistered(accelerator, AF_RESERVED)) {
      // TODO(oshima): do the same when the active window is in fullscreen.
      return false;
    }
    return accelerator_manager_->Process(accelerator);
  }

  AcceleratorManagerImpl* accelerator_manager_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorDelegate);
};

class FocusManagerDelegate : public views::FocusManagerDelegate {
 public:
  explicit FocusManagerDelegate(AcceleratorManagerImpl* accelerator_manager)
      : accelerator_manager_(accelerator_manager) {}
  ~FocusManagerDelegate() override {}

  bool ProcessAccelerator(const ui::Accelerator& accelerator) override {
    return accelerator_manager_->Process(accelerator);
  }

  ui::AcceleratorTarget* GetCurrentTargetForAccelerator(
      const ui::Accelerator& accelerator) const override {
    return accelerator_manager_->IsRegistered(accelerator, AF_NONE)
               ? accelerator_manager_
               : nullptr;
  }

 private:
  AcceleratorManagerImpl* accelerator_manager_;

  DISALLOW_COPY_AND_ASSIGN(FocusManagerDelegate);
};

// Key strokes must be sent to web contents to give them a chance to
// consume them unless they are reserved, and unhandled key events are
// sent back to focus manager asynchronously. This installs the athena's
// focus manager that handles athena shell's accelerators.
class FocusManagerFactory : public views::FocusManagerFactory {
 public:
  explicit FocusManagerFactory(AcceleratorManagerImpl* accelerator_manager)
      : accelerator_manager_(accelerator_manager) {}
  ~FocusManagerFactory() override {}

  views::FocusManager* CreateFocusManager(views::Widget* widget,
                                          bool desktop_widget) override {
    return new views::FocusManager(
        widget,
        desktop_widget ? nullptr
                       : new FocusManagerDelegate(accelerator_manager_));
  }

 private:
  AcceleratorManagerImpl* accelerator_manager_;

  DISALLOW_COPY_AND_ASSIGN(FocusManagerFactory);
};

class UIAcceleratorManagerWrapper
    : public AcceleratorManagerImpl::AcceleratorWrapper {
 public:
  UIAcceleratorManagerWrapper()
      : ui_accelerator_manager_(new ui::AcceleratorManager) {}
  ~UIAcceleratorManagerWrapper() override {}

  virtual void Register(const ui::Accelerator& accelerator,
                        ui::AcceleratorTarget* target) override {
    ui_accelerator_manager_->Register(
        accelerator, ui::AcceleratorManager::kNormalPriority, target);
  }

  virtual void Unregister(const ui::Accelerator& accelerator,
                          ui::AcceleratorTarget* target) override {
    ui_accelerator_manager_->Unregister(accelerator, target);
  }

  virtual bool Process(const ui::Accelerator& accelerator) override {
    return ui_accelerator_manager_->Process(accelerator);
  }

  virtual ui::AcceleratorTarget* GetCurrentTarget(
      const ui::Accelerator& accelerator) const override {
    return ui_accelerator_manager_->GetCurrentTarget(accelerator);
  }

 private:
  scoped_ptr<ui::AcceleratorManager> ui_accelerator_manager_;

  DISALLOW_COPY_AND_ASSIGN(UIAcceleratorManagerWrapper);
};

class FocusManagerWrapper : public AcceleratorManagerImpl::AcceleratorWrapper {
 public:
  explicit FocusManagerWrapper(views::FocusManager* focus_manager)
      : focus_manager_(focus_manager) {}
  ~FocusManagerWrapper() override {}

  virtual void Register(const ui::Accelerator& accelerator,
                        ui::AcceleratorTarget* target) override {
    return focus_manager_->RegisterAccelerator(
        accelerator, ui::AcceleratorManager::kNormalPriority, target);
  }

  virtual void Unregister(const ui::Accelerator& accelerator,
                          ui::AcceleratorTarget* target) override {
    focus_manager_->UnregisterAccelerator(accelerator, target);
  }

  virtual bool Process(const ui::Accelerator& accelerator) override {
    NOTREACHED();
    return true;
  }

  virtual ui::AcceleratorTarget* GetCurrentTarget(
      const ui::Accelerator& accelerator) const override {
    return focus_manager_->GetCurrentTargetForAccelerator(accelerator);
  }

 private:
  views::FocusManager* focus_manager_;

  DISALLOW_COPY_AND_ASSIGN(FocusManagerWrapper);
};

}  // namespace

class AcceleratorManagerImpl::InternalData {
 public:
  InternalData(int command_id, AcceleratorHandler* handler, int flags)
      : command_id_(command_id), handler_(handler), flags_(flags) {}

  bool IsNonAutoRepeatable() const { return flags_ & AF_NON_AUTO_REPEATABLE; }
  bool IsDebug() const { return flags_ & AF_DEBUG; }
  int flags() const { return flags_; }

  bool IsCommandEnabled() const {
    return handler_->IsCommandEnabled(command_id_);
  }

  bool OnAcceleratorFired(const ui::Accelerator& accelerator) {
    return handler_->OnAcceleratorFired(command_id_, accelerator);
  }

 private:
  int command_id_;
  AcceleratorHandler* handler_;
  int flags_;

  // This class is copyable by design.
};

// static
AcceleratorManagerImpl*
AcceleratorManagerImpl::CreateGlobalAcceleratorManager() {
  return new AcceleratorManagerImpl(new UIAcceleratorManagerWrapper(), true);
}

scoped_ptr<AcceleratorManager> AcceleratorManagerImpl::CreateForFocusManager(
    views::FocusManager* focus_manager) {
  return scoped_ptr<AcceleratorManager>(
             new AcceleratorManagerImpl(new FocusManagerWrapper(focus_manager),
                                        false)).Pass();
}

AcceleratorManagerImpl::~AcceleratorManagerImpl() {
  nested_accelerator_controller_.reset();
  accelerator_filter_.reset();
  // Reset to use the default focus manager because the athena's
  // FocusManager has the reference to this object.
  if (global_)
    views::FocusManagerFactory::Install(nullptr);
}

void AcceleratorManagerImpl::Init() {
  if (global_)
    views::FocusManagerFactory::Install(new FocusManagerFactory(this));

  ui::EventTarget* toplevel = InputManager::Get()->GetTopmostEventTarget();
  nested_accelerator_controller_.reset(
      new wm::NestedAcceleratorController(new NestedAcceleratorDelegate(this)));

  scoped_ptr<wm::AcceleratorDelegate> accelerator_delegate(
      new AcceleratorDelegate(this));

  accelerator_filter_.reset(
      new wm::AcceleratorFilter(accelerator_delegate.Pass()));
  toplevel->AddPreTargetHandler(accelerator_filter_.get());
}

void AcceleratorManagerImpl::OnRootWindowCreated(aura::Window* root_window) {
  aura::client::SetDispatcherClient(root_window,
                                    nested_accelerator_controller_.get());
}

bool AcceleratorManagerImpl::Process(const ui::Accelerator& accelerator) {
  return accelerator_wrapper_->Process(accelerator);
}

bool AcceleratorManagerImpl::IsRegistered(const ui::Accelerator& accelerator,
                                          int flags) const {
  std::map<ui::Accelerator, InternalData>::const_iterator iter =
      accelerators_.find(accelerator);
  if (iter == accelerators_.end())
    return false;
  DCHECK(accelerator_wrapper_->GetCurrentTarget(accelerator));
  return flags == AF_NONE || iter->second.flags() & flags;
}

AcceleratorManagerImpl::AcceleratorManagerImpl(
    AcceleratorWrapper* accelerator_wrapper,
    bool global)
    : accelerator_wrapper_(accelerator_wrapper),
      debug_accelerators_enabled_(switches::IsDebugAcceleratorsEnabled()),
      global_(global) {
}

void AcceleratorManagerImpl::RegisterAccelerators(
    const AcceleratorData accelerators[],
    size_t num_accelerators,
    AcceleratorHandler* handler) {
  for (size_t i = 0; i < num_accelerators; ++i)
    RegisterAccelerator(accelerators[i], handler);
}

void AcceleratorManagerImpl::SetDebugAcceleratorsEnabled(bool enabled) {
  debug_accelerators_enabled_ = enabled;
}

bool AcceleratorManagerImpl::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  std::map<ui::Accelerator, InternalData>::iterator iter =
      accelerators_.find(accelerator);
  DCHECK(iter != accelerators_.end());
  if (iter == accelerators_.end())
    return false;
  InternalData& data = iter->second;
  if (data.IsDebug() && !debug_accelerators_enabled_)
    return false;
  if (accelerator.IsRepeat() && data.IsNonAutoRepeatable())
    return false;
  return data.IsCommandEnabled() ? data.OnAcceleratorFired(accelerator) : false;
}

bool AcceleratorManagerImpl::CanHandleAccelerators() const {
  return true;
}

void AcceleratorManagerImpl::RegisterAccelerator(
    const AcceleratorData& accelerator_data,
    AcceleratorHandler* handler) {
  ui::Accelerator accelerator(accelerator_data.keycode,
                              accelerator_data.keyevent_flags);
  accelerator.set_type(accelerator_data.trigger_event == TRIGGER_ON_PRESS
                           ? ui::ET_KEY_PRESSED
                           : ui::ET_KEY_RELEASED);
  accelerator_wrapper_->Register(accelerator, this);
  accelerators_.insert(
      std::make_pair(accelerator,
                     InternalData(accelerator_data.command_id,
                                  handler,
                                  accelerator_data.accelerator_flags)));
}

void AcceleratorManagerImpl::UnregisterAccelerator(
    const AcceleratorData& accelerator_data,
    AcceleratorHandler* handler) {
  ui::Accelerator accelerator(accelerator_data.keycode,
                              accelerator_data.keyevent_flags);
  accelerator.set_type(accelerator_data.trigger_event == TRIGGER_ON_PRESS
                           ? ui::ET_KEY_PRESSED
                           : ui::ET_KEY_RELEASED);
  accelerator_wrapper_->Unregister(accelerator, this);
  accelerators_.erase(accelerator);
}

// static
AcceleratorManager* AcceleratorManager::Get() {
  return InputManager::Get()->GetAcceleratorManager();
}

// static
scoped_ptr<AcceleratorManager> AcceleratorManager::CreateForFocusManager(
    views::FocusManager* focus_manager) {
  return AcceleratorManagerImpl::CreateForFocusManager(focus_manager).Pass();
}

}  // namespace athena
