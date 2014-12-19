// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/screen/screen_manager_impl.h"

#include "athena/input/public/accelerator_manager.h"
#include "athena/screen/modal_window_controller.h"
#include "athena/screen/screen_accelerator_handler.h"
#include "athena/util/container_priorities.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/default_screen_position_client.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/window_util.h"

DECLARE_WINDOW_PROPERTY_TYPE(athena::ScreenManager::ContainerParams*);

namespace athena {
namespace {

DEFINE_OWNED_WINDOW_PROPERTY_KEY(ScreenManager::ContainerParams,
                                 kContainerParamsKey,
                                 nullptr);

ScreenManagerImpl* instance = nullptr;

// A functor to find a container that has the higher priority.
struct HigherPriorityFinder {
  HigherPriorityFinder(int p) : priority(p) {}
  bool operator()(aura::Window* window) {
    return window->GetProperty(kContainerParamsKey)->z_order_priority >
           priority;
  }
  int priority;
};

bool BlockEvents(aura::Window* container) {
  ScreenManager::ContainerParams* params =
      container->GetProperty(kContainerParamsKey);
  return params && params->block_events && container->IsVisible();
}

bool DefaultContainer(aura::Window* container) {
  ScreenManager::ContainerParams* params =
      container->GetProperty(kContainerParamsKey);
  return params && params->default_parent;
}

bool HasModalContainerPriority(aura::Window* container) {
  ScreenManager::ContainerParams* params =
      container->GetProperty(kContainerParamsKey);
  return params && params->modal_container_priority != -1;
}

bool IsSystemModal(aura::Window* window) {
  return window->GetProperty(aura::client::kModalKey) == ui::MODAL_TYPE_SYSTEM;
}

// Returns the container which contains |window|.
aura::Window* GetContainer(aura::Window* window) {
  aura::Window* container = window;
  while (container && !container->GetProperty(kContainerParamsKey))
    container = container->parent();
  return container;
}

class AthenaFocusRules : public wm::BaseFocusRules {
 public:
  AthenaFocusRules() {}
  ~AthenaFocusRules() override {}

  // wm::BaseFocusRules:
  virtual bool SupportsChildActivation(aura::Window* window) const override {
    ScreenManager::ContainerParams* params =
        window->GetProperty(kContainerParamsKey);
    return params && params->can_activate_children;
  }
  virtual bool CanActivateWindow(aura::Window* window) const override {
    if (!window)
      return true;

    // Check if containers of higher z-order than |window| have 'block_events'
    // fields.
    if (window->GetRootWindow()) {
      const aura::Window::Windows& containers =
          window->GetRootWindow()->children();
      aura::Window::Windows::const_iterator iter =
          std::find(containers.begin(), containers.end(), GetContainer(window));
      DCHECK(iter != containers.end());
      for (++iter; iter != containers.end(); ++iter) {
        if (BlockEvents(*iter))
          return false;
      }
    }
    return BaseFocusRules::CanActivateWindow(window);
  }

  aura::Window* GetTopmostWindowToActivateInContainer(
      aura::Window* container,
      aura::Window* ignore) const {
    for (aura::Window::Windows::const_reverse_iterator i =
             container->children().rbegin();
         i != container->children().rend();
         ++i) {
      if (*i != ignore && CanActivateWindow(*i))
        return *i;
    }
    return NULL;
  }

  virtual aura::Window* GetNextActivatableWindow(
      aura::Window* ignore) const override {
    const aura::Window::Windows& containers =
        ignore->GetRootWindow()->children();
    auto starting_container_iter = containers.begin();
    for (auto container_iter = containers.begin();
         container_iter != containers.end();
         container_iter++) {
      if ((*container_iter)->Contains(ignore)) {
        starting_container_iter = container_iter;
        break;
      }
    }

    // Find next window from the front containers.
    aura::Window* next = nullptr;
    for (auto container_iter = starting_container_iter;
         !next && container_iter != containers.end();
         container_iter++) {
      next = GetTopmostWindowToActivateInContainer(*container_iter, ignore);
    }

    // Find next window from the back containers.
    auto container_iter = starting_container_iter;
    while (!next && container_iter != containers.begin()) {
      container_iter--;
      next = GetTopmostWindowToActivateInContainer(*container_iter, ignore);
    }
    return next;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AthenaFocusRules);
};

class AthenaScreenPositionClient : public wm::DefaultScreenPositionClient {
 public:
  AthenaScreenPositionClient() {
  }
  ~AthenaScreenPositionClient() override {}

 private:
  // aura::client::ScreenPositionClient:
  void ConvertHostPointToScreen(aura::Window* window,
                                gfx::Point* point) override {
    // TODO(oshima): Implement this when adding multiple display support.
    NOTREACHED();
  }

  DISALLOW_COPY_AND_ASSIGN(AthenaScreenPositionClient);
};

class AthenaWindowTargeter : public aura::WindowTargeter {
 public:
  explicit AthenaWindowTargeter(aura::Window* root_window)
      : root_window_(root_window) {}

  ~AthenaWindowTargeter() override {}

 private:
  // aura::WindowTargeter:
  virtual bool SubtreeCanAcceptEvent(
      ui::EventTarget* target,
      const ui::LocatedEvent& event) const override {
    const aura::Window::Windows& containers = root_window_->children();
    auto r_iter =
        std::find_if(containers.rbegin(), containers.rend(), &BlockEvents);
    if (r_iter == containers.rend())
      return aura::WindowTargeter::SubtreeCanAcceptEvent(target, event);

    aura::Window* window = static_cast<aura::Window*>(target);
    for (;; --r_iter) {
      if ((*r_iter)->Contains(window))
        return aura::WindowTargeter::SubtreeCanAcceptEvent(target, event);
      if (r_iter == containers.rbegin())
        break;
    }
    return false;
  }

  virtual ui::EventTarget* FindTargetForLocatedEvent(
      ui::EventTarget* root,
      ui::LocatedEvent* event) override {
    ui::EventTarget* target =
        aura::WindowTargeter::FindTargetForLocatedEvent(root, event);
    if (target)
      return target;
    // If the root target is blocking the event, return the container even if
    // there is no target found so that windows behind it will not be searched.
    const ScreenManager::ContainerParams* params =
        static_cast<aura::Window*>(root)->GetProperty(kContainerParamsKey);
    return (params && params->block_events) ? root : nullptr;
  }

  // Not owned.
  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(AthenaWindowTargeter);
};

}  // namespace

ScreenManagerImpl::ScreenManagerImpl(aura::Window* root_window)
    : root_window_(root_window),
      last_requested_rotation_(gfx::Display::ROTATE_0),
      rotation_locked_(false) {
  DCHECK(root_window_);
  DCHECK(!instance);
  instance = this;
}

ScreenManagerImpl::~ScreenManagerImpl() {
  aura::client::SetScreenPositionClient(root_window_, nullptr);
  aura::client::SetWindowTreeClient(root_window_, nullptr);
  wm::FocusController* focus_controller =
      static_cast<wm::FocusController*>(focus_client_.get());
  root_window_->RemovePreTargetHandler(focus_controller);
  aura::client::SetActivationClient(root_window_, nullptr);
  aura::client::SetFocusClient(root_window_, nullptr);
  aura::Window::Windows children = root_window_->children();
  // Close All children:
  for (aura::Window::Windows::iterator iter = children.begin();
       iter != children.end();
       ++iter) {
    delete *iter;
  }
  instance = nullptr;
}

void ScreenManagerImpl::Init() {
  wm::FocusController* focus_controller =
      new wm::FocusController(new AthenaFocusRules());

  aura::client::SetFocusClient(root_window_, focus_controller);
  root_window_->AddPreTargetHandler(focus_controller);
  aura::client::SetActivationClient(root_window_, focus_controller);
  focus_client_.reset(focus_controller);

  capture_client_.reset(new ::wm::ScopedCaptureClient(root_window_));
  accelerator_handler_.reset(new ScreenAcceleratorHandler());

  aura::client::SetWindowTreeClient(root_window_, this);

  screen_position_client_.reset(new AthenaScreenPositionClient());
  aura::client::SetScreenPositionClient(root_window_,
                                        screen_position_client_.get());
  root_window_->SetEventTargeter(
      make_scoped_ptr(new AthenaWindowTargeter(root_window_)));
}

aura::Window* ScreenManagerImpl::FindContainerByPriority(int priority) {
  for (aura::Window* window : root_window_->children()) {
    if (window->GetProperty(kContainerParamsKey)->z_order_priority == priority)
      return window;
  }
  return nullptr;
}

aura::Window* ScreenManagerImpl::CreateContainer(
    const ContainerParams& params) {
  const aura::Window::Windows& children = root_window_->children();

  if (params.default_parent) {
    CHECK(std::find_if(children.begin(), children.end(), &DefaultContainer) ==
          children.end());
  }
  // mmodal container's priority must be higher than the container's priority.
  DCHECK(params.modal_container_priority == -1 ||
         params.modal_container_priority > params.z_order_priority);
  // Default parent must specify modal_container_priority.
  DCHECK(!params.default_parent || params.modal_container_priority != -1);

  aura::Window* container = new aura::Window(nullptr);
  CHECK_GE(params.z_order_priority, 0);
  container->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  container->SetName(params.name);

  DCHECK(!FindContainerByPriority(params.z_order_priority))
      << "The container with the priority " << params.z_order_priority
      << " already exists.";

  container->SetProperty(kContainerParamsKey, new ContainerParams(params));

  root_window_->AddChild(container);

  aura::Window::Windows::const_iterator iter =
      std::find_if(children.begin(),
                   children.end(),
                   HigherPriorityFinder(params.z_order_priority));
  if (iter != children.end())
    root_window_->StackChildBelow(container, *iter);

  container->Show();
  return container;
}

aura::Window* ScreenManagerImpl::GetContext() {
  return root_window_;
}

void ScreenManagerImpl::SetRotation(gfx::Display::Rotation rotation) {
  last_requested_rotation_ = rotation;
  if (rotation_locked_ || rotation ==
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().rotation()) {
    return;
  }

  // TODO(flackr): Use display manager to update display rotation:
  // http://crbug.com/401044.
  static_cast<aura::TestScreen*>(gfx::Screen::GetNativeScreen())->
      SetDisplayRotation(rotation);
}

void ScreenManagerImpl::SetRotationLocked(bool rotation_locked) {
  rotation_locked_ = rotation_locked;
  if (!rotation_locked_)
    SetRotation(last_requested_rotation_);
}

int ScreenManagerImpl::GetModalContainerPriority(aura::Window* window,
                                                 aura::Window* parent) {
  const aura::Window::Windows& children = root_window_->children();
  if (window->GetProperty(aura::client::kAlwaysOnTopKey)) {
    // Use top most modal container.
    auto iter = std::find_if(
        children.rbegin(), children.rend(), &HasModalContainerPriority);
    DCHECK(iter != children.rend());
    return (*iter)->GetProperty(kContainerParamsKey)->modal_container_priority;
  } else {
    // use the container closest to the parent which has modal
    // container priority.
    auto iter = std::find(children.rbegin(), children.rend(), parent);
    DCHECK(iter != children.rend());
    iter = std::find_if(iter, children.rend(), &HasModalContainerPriority);
    DCHECK(iter != children.rend());
    return (*iter)->GetProperty(kContainerParamsKey)->modal_container_priority;
  }
}

aura::Window* ScreenManagerImpl::GetDefaultParent(aura::Window* context,
                                                  aura::Window* window,
                                                  const gfx::Rect& bounds) {
  aura::Window* parent = wm::GetTransientParent(window);
  if (parent)
    parent = GetContainer(parent);
  else
    parent = GetDefaultContainer();

  if (IsSystemModal(window)) {
    DCHECK(window->type() == ui::wm::WINDOW_TYPE_NORMAL ||
           window->type() == ui::wm::WINDOW_TYPE_POPUP);
    int priority = GetModalContainerPriority(window, parent);

    parent = FindContainerByPriority(priority);
    if (!parent) {
      ModalWindowController* controller = new ModalWindowController(priority);
      parent = controller->modal_container();
    }
  }
  return parent;
}

aura::Window* ScreenManagerImpl::GetDefaultContainer() {
  const aura::Window::Windows& children = root_window_->children();
  return *(std::find_if(children.begin(), children.end(), &DefaultContainer));
}

ScreenManager::ContainerParams::ContainerParams(const std::string& n,
                                                int priority)
    : name(n),
      can_activate_children(false),
      block_events(false),
      z_order_priority(priority),
      default_parent(false),
      modal_container_priority(-1) {
}

// static
ScreenManager* ScreenManager::Create(aura::Window* root_window) {
  (new ScreenManagerImpl(root_window))->Init();
  DCHECK(instance);
  return instance;
}

// static
ScreenManager* ScreenManager::Get() {
  DCHECK(instance);
  return instance;
}

// static
void ScreenManager::Shutdown() {
  DCHECK(instance);
  delete instance;
  DCHECK(!instance);
}

}  // namespace athena
