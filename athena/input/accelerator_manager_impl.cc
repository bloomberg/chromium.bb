// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/input/accelerator_manager_impl.h"

#include "athena/common/switches.h"
#include "athena/input/public/input_manager.h"
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
  virtual ~NestedAcceleratorDelegate() {}

 private:
  // wm::NestedAcceleratorDelegate:
  virtual Result ProcessAccelerator(
      const ui::Accelerator& accelerator) OVERRIDE {
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
  virtual ~AcceleratorDelegate() {}

 private:
  // wm::AcceleratorDelegate:
  virtual bool ProcessAccelerator(const ui::KeyEvent& event,
                                  const ui::Accelerator& accelerator,
                                  KeyType key_type) OVERRIDE {
    aura::Window* target = static_cast<aura::Window*>(event.target());
    if (!target->IsRootWindow() &&
        !accelerator_manager_->IsReserved(accelerator)) {
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
  virtual ~FocusManagerDelegate() {}

  virtual bool ProcessAccelerator(const ui::Accelerator& accelerator) OVERRIDE {
    return accelerator_manager_->Process(accelerator);
  }

  virtual ui::AcceleratorTarget* GetCurrentTargetForAccelerator(
      const ui::Accelerator& accelerator) const OVERRIDE {
    return accelerator_manager_->IsRegistered(accelerator)
               ? accelerator_manager_
               : NULL;
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
  virtual ~FocusManagerFactory() {}

  virtual views::FocusManager* CreateFocusManager(
      views::Widget* widget,
      bool desktop_widget) OVERRIDE {
    return new views::FocusManager(
        widget,
        desktop_widget ? NULL : new FocusManagerDelegate(accelerator_manager_));
  }

 private:
  AcceleratorManagerImpl* accelerator_manager_;

  DISALLOW_COPY_AND_ASSIGN(FocusManagerFactory);
};

}  // namespace

class AcceleratorManagerImpl::InternalData {
 public:
  InternalData(int command_id, AcceleratorHandler* handler, int flags)
      : command_id_(command_id), handler_(handler), flags_(flags) {}

  bool IsNonAutoRepeatable() const { return flags_ & AF_NON_AUTO_REPEATABLE; }
  bool IsDebug() const { return flags_ & AF_DEBUG; }
  bool IsReserved() const { return flags_ & AF_RESERVED; }

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

AcceleratorManagerImpl::AcceleratorManagerImpl()
    : accelerator_manager_(new ui::AcceleratorManager),
      debug_accelerators_enabled_(switches::IsDebugAcceleratorsEnabled()) {
}

AcceleratorManagerImpl::~AcceleratorManagerImpl() {
  nested_accelerator_controller_.reset();
  accelerator_filter_.reset();
  // Reset to use the default focus manager because the athena's
  // FocusManager has the reference to this object.
  views::FocusManagerFactory::Install(NULL);
}

void AcceleratorManagerImpl::Init() {
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

bool AcceleratorManagerImpl::IsRegistered(
    const ui::Accelerator& accelerator) const {
  return accelerator_manager_->GetCurrentTarget(accelerator) != NULL;
}

bool AcceleratorManagerImpl::IsReserved(
    const ui::Accelerator& accelerator) const {
  std::map<ui::Accelerator, InternalData>::const_iterator iter =
      accelerators_.find(accelerator);
  if (iter == accelerators_.end())
    return false;
  return iter->second.IsReserved();
}

bool AcceleratorManagerImpl::Process(const ui::Accelerator& accelerator) {
  return accelerator_manager_->Process(accelerator);
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
  accelerator_manager_->Register(
      accelerator, ui::AcceleratorManager::kNormalPriority, this);
  accelerators_.insert(
      std::make_pair(accelerator,
                     InternalData(accelerator_data.command_id,
                                  handler,
                                  accelerator_data.accelerator_flags)));
}

// static
AcceleratorManager* AcceleratorManager::Get() {
  return InputManager::Get()->GetAcceleratorManager();
}

}  // namespace athena
