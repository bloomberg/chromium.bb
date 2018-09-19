// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_keyboard_ui.h"

#include <set>
#include <string>
#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/ash/chrome_keyboard_controller_observer.h"
#include "chrome/browser/ui/ash/chrome_keyboard_web_contents.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/platform/aura_window_properties.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_resource_util.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/wm/core/shadow_types.h"

namespace {

const int kShadowElevationVirtualKeyboard = 2;

GURL& GetOverrideVirtualKeyboardUrl() {
  static base::NoDestructor<GURL> url;
  return *url;
}

}  // namespace

class ChromeKeyboardUI::WindowBoundsChangeObserver
    : public aura::WindowObserver {
 public:
  explicit WindowBoundsChangeObserver(ChromeKeyboardUI* ui) : ui_(ui) {}
  ~WindowBoundsChangeObserver() override {}

  void AddObservedWindow(aura::Window* window) {
    if (!window->HasObserver(this)) {
      window->AddObserver(this);
      observed_windows_.insert(window);
    }
  }

  void RemoveAllObservedWindows() {
    for (aura::Window* window : observed_windows_)
      window->RemoveObserver(this);
    observed_windows_.clear();
  }

 private:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override {
    ui_->UpdateInsetsForWindow(window);
  }

  void OnWindowDestroyed(aura::Window* window) override {
    if (window->HasObserver(this))
      window->RemoveObserver(this);
    observed_windows_.erase(window);
  }

  ChromeKeyboardUI* const ui_;
  std::set<aura::Window*> observed_windows_;

  DISALLOW_COPY_AND_ASSIGN(WindowBoundsChangeObserver);
};

void ChromeKeyboardUI::TestApi::SetOverrideVirtualKeyboardUrl(const GURL& url) {
  GURL& override_url = GetOverrideVirtualKeyboardUrl();
  override_url = url;
}

ChromeKeyboardUI::ChromeKeyboardUI(content::BrowserContext* context)
    : browser_context_(context),
      window_bounds_observer_(
          std::make_unique<WindowBoundsChangeObserver>(this)) {}

ChromeKeyboardUI::~ChromeKeyboardUI() {
  ResetInsets();
  DCHECK(!keyboard_controller());
}

void ChromeKeyboardUI::UpdateInsetsForWindow(aura::Window* window) {
  if (!HasKeyboardWindow() || !ShouldWindowOverscroll(window))
    return;

  std::unique_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    content::RenderWidgetHostView* view = widget->GetView();
    if (view && window->Contains(view->GetNativeView())) {
      gfx::Rect view_bounds = view->GetViewBounds();
      gfx::Rect intersect = gfx::IntersectRects(
          view_bounds, GetKeyboardWindow()->GetBoundsInScreen());
      int overlap = ShouldEnableInsets(window) ? intersect.height() : 0;
      if (overlap > 0 && overlap < view_bounds.height())
        view->SetInsets(gfx::Insets(0, 0, overlap, 0));
      else
        view->SetInsets(gfx::Insets());
    }
  }
}

aura::Window* ChromeKeyboardUI::GetKeyboardWindow() {
  if (keyboard_contents_)
    return keyboard_contents_->web_contents()->GetNativeView();

  GURL keyboard_url = GetVirtualKeyboardUrl();
  keyboard_contents_ = std::make_unique<ChromeKeyboardWebContents>(
      browser_context_, keyboard_url);

  aura::Window* keyboard_window =
      keyboard_contents_->web_contents()->GetNativeView();

  keyboard_window->AddObserver(this);
  keyboard_window->set_owned_by_parent(false);

  content::RenderWidgetHostView* view =
      keyboard_contents_->web_contents()->GetMainFrame()->GetView();

  // Only use transparent background when fullscreen handwriting or the new UI
  // is enabled. The old UI sometimes reloads itself, which will cause the
  // keyboard to be see-through.
  // TODO(https://crbug.com/840731): Find a permanent fix for this on the
  // keyboard extension side.
  if (base::FeatureList::IsEnabled(
          features::kEnableFullscreenHandwritingVirtualKeyboard) ||
      base::FeatureList::IsEnabled(features::kEnableVirtualKeyboardMdUi)) {
    view->SetBackgroundColor(SK_ColorTRANSPARENT);
    view->GetNativeView()->SetTransparent(true);
  }

  // By default, layers in WebContents are clipped at the window bounds,
  // but this causes the shadows to be clipped too, so clipping needs to
  // be disabled.
  keyboard_window->layer()->SetMasksToBounds(false);

  keyboard_window->SetProperty(ui::kAXRoleOverride, ax::mojom::Role::kKeyboard);

  return keyboard_window;
}

bool ChromeKeyboardUI::HasKeyboardWindow() const {
  return !!keyboard_contents_;
}

ui::InputMethod* ChromeKeyboardUI::GetInputMethod() {
  ui::IMEBridge* bridge = ui::IMEBridge::Get();
  if (!bridge || !bridge->GetInputContextHandler()) {
    // Needed by a handful of browser tests that use MockInputMethod.
    return ash::Shell::GetRootWindowForNewWindows()
        ->GetHost()
        ->GetInputMethod();
  }

  return bridge->GetInputContextHandler()->GetInputMethod();
}

void ChromeKeyboardUI::SetController(keyboard::KeyboardController* controller) {
  // During KeyboardController destruction, controller can be set to null.
  if (!controller) {
    DCHECK(keyboard_controller());
    keyboard_controller_observer_.reset();
    KeyboardUI::SetController(nullptr);
    return;
  }
  KeyboardUI::SetController(controller);
  keyboard_controller_observer_ =
      std::make_unique<ChromeKeyboardControllerObserver>(browser_context_,
                                                         controller);
}

void ChromeKeyboardUI::ReloadKeyboardIfNeeded() {
  DCHECK(keyboard_contents_);
  keyboard_contents_->SetKeyboardUrl(GetVirtualKeyboardUrl());
}

void ChromeKeyboardUI::InitInsets(const gfx::Rect& new_bounds) {
  // Adjust the height of the viewport for visible windows on the primary
  // display.
  // TODO(kevers): Add EnvObserver to properly initialize insets if a
  // window is created while the keyboard is visible.
  std::unique_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    content::RenderWidgetHostView* view = widget->GetView();
    // Can be null, e.g. if the RenderWidget is being destroyed or
    // the render process crashed.
    if (!view)
      continue;

    aura::Window* window = view->GetNativeView();
    // Added while we determine if RenderWidgetHostViewChildFrame can be
    // changed to always return a non-null value: https://crbug.com/644726.
    // If we cannot guarantee a non-null value, then this may need to stay.
    if (!window)
      continue;

    if (!ShouldWindowOverscroll(window))
      continue;

    gfx::Rect view_bounds = view->GetViewBounds();
    gfx::Rect intersect = gfx::IntersectRects(view_bounds, new_bounds);
    int overlap = intersect.height();
    if (overlap > 0 && overlap < view_bounds.height())
      view->SetInsets(gfx::Insets(0, 0, overlap, 0));
    else
      view->SetInsets(gfx::Insets());
    AddBoundsChangedObserver(window);
  }
}

void ChromeKeyboardUI::ResetInsets() {
  const gfx::Insets insets;
  std::unique_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    content::RenderWidgetHostView* view = widget->GetView();
    if (view)
      view->SetInsets(insets);
  }
  window_bounds_observer_->RemoveAllObservedWindows();
}

void ChromeKeyboardUI::OnWindowBoundsChanged(aura::Window* window,
                                             const gfx::Rect& old_bounds,
                                             const gfx::Rect& new_bounds,
                                             ui::PropertyChangeReason reason) {
  SetShadowAroundKeyboard();
}

void ChromeKeyboardUI::OnWindowDestroyed(aura::Window* window) {
  window->RemoveObserver(this);
}

void ChromeKeyboardUI::OnWindowParentChanged(aura::Window* window,
                                             aura::Window* parent) {
  SetShadowAroundKeyboard();
}

GURL ChromeKeyboardUI::GetVirtualKeyboardUrl() {
  const GURL& override_url = GetOverrideVirtualKeyboardUrl();
  if (!override_url.is_empty())
    return override_url;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          keyboard::switches::kDisableInputView)) {
    return GURL(keyboard::kKeyboardURL);
  }

  chromeos::input_method::InputMethodManager* ime_manager =
      chromeos::input_method::InputMethodManager::Get();
  if (!ime_manager || !ime_manager->GetActiveIMEState())
    return GURL(keyboard::kKeyboardURL);

  const GURL& input_view_url =
      ime_manager->GetActiveIMEState()->GetInputViewUrl();
  if (!input_view_url.is_valid())
    return GURL(keyboard::kKeyboardURL);

  return input_view_url;
}

bool ChromeKeyboardUI::ShouldEnableInsets(aura::Window* window) {
  aura::Window* contents_window = GetKeyboardWindow();
  return (contents_window->GetRootWindow() == window->GetRootWindow() &&
          keyboard::KeyboardController::Get()->IsKeyboardOverscrollEnabled() &&
          contents_window->IsVisible() &&
          keyboard_controller()->IsKeyboardVisible());
}

bool ChromeKeyboardUI::ShouldWindowOverscroll(aura::Window* window) {
  aura::Window* root_window = window->GetRootWindow();
  if (!root_window)
    return true;

  if (!HasKeyboardWindow())
    return false;

  aura::Window* keyboard_window = GetKeyboardWindow();
  if (root_window != keyboard_window->GetRootWindow())
    return false;

  ash::RootWindowController* root_window_controller =
      ash::RootWindowController::ForWindow(root_window);
  // Shell ime window container contains virtual keyboard windows and IME
  // windows (IME windows are created by chrome.app.window.create api). They
  // should never be overscrolled.
  return !root_window_controller
              ->GetContainer(ash::kShellWindowId_ImeWindowParentContainer)
              ->Contains(window);
}

void ChromeKeyboardUI::AddBoundsChangedObserver(aura::Window* window) {
  aura::Window* target_window = window ? window->GetToplevelWindow() : nullptr;
  if (target_window)
    window_bounds_observer_->AddObservedWindow(target_window);
}

void ChromeKeyboardUI::SetShadowAroundKeyboard() {
  DCHECK(HasKeyboardWindow());
  aura::Window* contents_window = GetKeyboardWindow();
  if (!shadow_) {
    shadow_ = std::make_unique<ui::Shadow>();
    shadow_->Init(kShadowElevationVirtualKeyboard);
    shadow_->layer()->SetVisible(true);
    contents_window->layer()->Add(shadow_->layer());
  }

  shadow_->SetContentBounds(gfx::Rect(contents_window->bounds().size()));

  // In floating mode, make the shadow layer invisible because the shadows are
  // drawn manually by the IME extension.
  // TODO(https://crbug.com/856195): Remove this when we figure out how ChromeOS
  // can draw custom shaped shadows, or how overscrolling can account for
  // shadows drawn by IME.
  shadow_->layer()->SetVisible(
      keyboard_controller()->GetActiveContainerType() ==
      keyboard::ContainerType::FULL_WIDTH);
}
