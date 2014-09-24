// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/screen/public/screen_manager.h"

#include "athena/input/public/accelerator_manager.h"
#include "athena/screen/screen_accelerator_handler.h"
#include "athena/util/container_priorities.h"
#include "athena/util/fill_layout_manager.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/window_util.h"

namespace athena {
namespace {

DEFINE_OWNED_WINDOW_PROPERTY_KEY(ScreenManager::ContainerParams,
                                 kContainerParamsKey,
                                 NULL);

ScreenManager* instance = NULL;

bool GrabsInput(aura::Window* container) {
  ScreenManager::ContainerParams* params =
      container->GetProperty(kContainerParamsKey);
  return params && params->grab_inputs;
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
  virtual ~AthenaFocusRules() {}

  // wm::BaseFocusRules:
  virtual bool SupportsChildActivation(aura::Window* window) const OVERRIDE {
    ScreenManager::ContainerParams* params =
        window->GetProperty(kContainerParamsKey);
    return params && params->can_activate_children;
  }
  virtual bool CanActivateWindow(aura::Window* window) const OVERRIDE {
    // Check if containers of higher z-order than |window| have 'grab_inputs'
    // fields.
    if (window) {
      const aura::Window::Windows& containers =
          window->GetRootWindow()->children();
      aura::Window::Windows::const_iterator iter =
          std::find(containers.begin(), containers.end(), GetContainer(window));
      DCHECK(iter != containers.end());
      for (++iter; iter != containers.end(); ++iter) {
        if (GrabsInput(*iter))
          return false;
      }
    }
    return BaseFocusRules::CanActivateWindow(window);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AthenaFocusRules);
};

class AthenaWindowTreeClient : public aura::client::WindowTreeClient {
 public:
  explicit AthenaWindowTreeClient(aura::Window* container)
      : container_(container) {}

 private:
  virtual ~AthenaWindowTreeClient() {}

  // aura::client::WindowTreeClient:
  virtual aura::Window* GetDefaultParent(aura::Window* context,
                                         aura::Window* window,
                                         const gfx::Rect& bounds) OVERRIDE {
    aura::Window* transient_parent = wm::GetTransientParent(window);
    if (transient_parent)
      return GetContainer(transient_parent);
    return container_;
  }

  aura::Window* container_;

  DISALLOW_COPY_AND_ASSIGN(AthenaWindowTreeClient);
};

class AthenaScreenPositionClient : public aura::client::ScreenPositionClient {
 public:
  AthenaScreenPositionClient() {
  }
  virtual ~AthenaScreenPositionClient() {
  }

 private:
  // aura::client::ScreenPositionClient:
  virtual void ConvertPointToScreen(const aura::Window* window,
                                    gfx::Point* point) OVERRIDE {
    const aura::Window* root = window->GetRootWindow();
    aura::Window::ConvertPointToTarget(window, root, point);
  }

  virtual void ConvertPointFromScreen(const aura::Window* window,
                                      gfx::Point* point) OVERRIDE {
    const aura::Window* root = window->GetRootWindow();
    aura::Window::ConvertPointToTarget(root, window, point);
  }

  virtual void ConvertHostPointToScreen(aura::Window* window,
                                        gfx::Point* point) OVERRIDE {
    // TODO(oshima): Implement this when adding multiple display support.
    NOTREACHED();
  }

  virtual void SetBounds(aura::Window* window,
                         const gfx::Rect& bounds,
                         const gfx::Display& display) OVERRIDE {
    window->SetBounds(bounds);
  }

  DISALLOW_COPY_AND_ASSIGN(AthenaScreenPositionClient);
};

class AthenaEventTargeter : public aura::WindowTargeter,
                            public aura::WindowObserver {
 public:
  explicit AthenaEventTargeter(aura::Window* container)
      : container_(container) {
    container_->AddObserver(this);
  }

  virtual ~AthenaEventTargeter() {
    // Removed before the container is removed.
    if (container_)
      container_->RemoveObserver(this);
  }

 private:
  // aura::WindowTargeter:
  virtual bool SubtreeCanAcceptEvent(
      ui::EventTarget* target,
      const ui::LocatedEvent& event) const OVERRIDE {
    aura::Window* window = static_cast<aura::Window*>(target);
    const aura::Window::Windows& containers =
        container_->GetRootWindow()->children();
    aura::Window::Windows::const_iterator iter =
        std::find(containers.begin(), containers.end(), container_);
    DCHECK(iter != containers.end());
    for (; iter != containers.end(); ++iter) {
      if ((*iter)->Contains(window))
        return true;
    }
    return false;
  }

  // aura::WindowObserver:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    aura::Window* root_window = container_->GetRootWindow();
    DCHECK_EQ(window, container_);
    DCHECK_EQ(
        this, static_cast<ui::EventTarget*>(root_window)->GetEventTargeter());

    container_->RemoveObserver(this);
    container_ = NULL;

    // This will remove myself.
    root_window->SetEventTargeter(scoped_ptr<ui::EventTargeter>());
  }

  aura::Window* container_;

  DISALLOW_COPY_AND_ASSIGN(AthenaEventTargeter);
};

class ScreenManagerImpl : public ScreenManager {
 public:
  explicit ScreenManagerImpl(aura::Window* root_window);
  virtual ~ScreenManagerImpl();

  void Init();

 private:
  // ScreenManager:
  virtual aura::Window* CreateDefaultContainer(
      const ContainerParams& params) OVERRIDE;
  virtual aura::Window* CreateContainer(const ContainerParams& params) OVERRIDE;
  virtual aura::Window* GetContext() OVERRIDE { return root_window_; }
  virtual void SetRotation(gfx::Display::Rotation rotation) OVERRIDE;
  virtual void SetRotationLocked(bool rotation_locked) OVERRIDE;

  // Not owned.
  aura::Window* root_window_;

  scoped_ptr<aura::client::FocusClient> focus_client_;
  scoped_ptr<aura::client::WindowTreeClient> window_tree_client_;
  scoped_ptr<AcceleratorHandler> accelerator_handler_;
  scoped_ptr< ::wm::ScopedCaptureClient> capture_client_;
  scoped_ptr<aura::client::ScreenPositionClient> screen_position_client_;

  gfx::Display::Rotation last_requested_rotation_;
  bool rotation_locked_;

  DISALLOW_COPY_AND_ASSIGN(ScreenManagerImpl);
};

ScreenManagerImpl::ScreenManagerImpl(aura::Window* root_window)
    : root_window_(root_window),
      last_requested_rotation_(gfx::Display::ROTATE_0),
      rotation_locked_(false) {
  DCHECK(root_window_);
  DCHECK(!instance);
  instance = this;
}

ScreenManagerImpl::~ScreenManagerImpl() {
  aura::client::SetScreenPositionClient(root_window_, NULL);
  aura::client::SetWindowTreeClient(root_window_, NULL);
  wm::FocusController* focus_controller =
      static_cast<wm::FocusController*>(focus_client_.get());
  root_window_->RemovePreTargetHandler(focus_controller);
  aura::client::SetActivationClient(root_window_, NULL);
  aura::client::SetFocusClient(root_window_, NULL);
  aura::Window::Windows children = root_window_->children();
  // Close All children:
  for (aura::Window::Windows::iterator iter = children.begin();
       iter != children.end();
       ++iter) {
    delete *iter;
  }
  instance = NULL;
}

void ScreenManagerImpl::Init() {
  wm::FocusController* focus_controller =
      new wm::FocusController(new AthenaFocusRules());

  aura::client::SetFocusClient(root_window_, focus_controller);
  root_window_->AddPreTargetHandler(focus_controller);
  aura::client::SetActivationClient(root_window_, focus_controller);
  focus_client_.reset(focus_controller);

  root_window_->SetLayoutManager(new FillLayoutManager(root_window_));
  capture_client_.reset(new ::wm::ScopedCaptureClient(root_window_));
  accelerator_handler_.reset(new ScreenAcceleratorHandler(root_window_));
}

aura::Window* ScreenManagerImpl::CreateDefaultContainer(
    const ContainerParams& params) {
  aura::Window* container = CreateContainer(params);
  window_tree_client_.reset(new AthenaWindowTreeClient(container));
  aura::client::SetWindowTreeClient(root_window_, window_tree_client_.get());

  screen_position_client_.reset(new AthenaScreenPositionClient());
  aura::client::SetScreenPositionClient(root_window_,
                                        screen_position_client_.get());

  return container;
}

// A functor to find a container that has the higher priority.
struct HigherPriorityFinder {
  HigherPriorityFinder(int p) : priority(p) {}
  bool operator()(aura::Window* window) {
    return window->GetProperty(kContainerParamsKey)->z_order_priority >
           priority;
  }
  int priority;
};

#if !defined(NDEBUG)
struct PriorityMatcher {
  PriorityMatcher(int p) : priority(p) {}
  bool operator()(aura::Window* window) {
    return window->GetProperty(kContainerParamsKey)->z_order_priority ==
           priority;
  }
  int priority;
};
#endif

aura::Window* ScreenManagerImpl::CreateContainer(
    const ContainerParams& params) {
  aura::Window* container = new aura::Window(NULL);
  CHECK_GE(params.z_order_priority, 0);
  container->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  container->SetName(params.name);

  const aura::Window::Windows& children = root_window_->children();

#if !defined(NDEBUG)
  DCHECK(std::find_if(children.begin(),
                      children.end(),
                      PriorityMatcher(params.z_order_priority))
         == children.end())
      << "The container with the priority "
      << params.z_order_priority << " already exists.";
#endif

  container->SetProperty(kContainerParamsKey, new ContainerParams(params));

  // If another container is already grabbing the input, SetEventTargeter
  // implicitly release the grabbing and remove the EventTargeter instance.
  // TODO(mukai|oshima): think about the ideal behavior of multiple grabbing
  // and implement it.
  if (params.grab_inputs) {
    DCHECK(std::find_if(children.begin(), children.end(), &GrabsInput)
           == children.end())
        << "input has already been grabbed by another container";
    root_window_->SetEventTargeter(
        scoped_ptr<ui::EventTargeter>(new AthenaEventTargeter(container)));
  }

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

}  // namespace

ScreenManager::ContainerParams::ContainerParams(const std::string& n,
                                                int priority)
    : name(n),
      can_activate_children(false),
      grab_inputs(false),
      z_order_priority(priority) {
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
