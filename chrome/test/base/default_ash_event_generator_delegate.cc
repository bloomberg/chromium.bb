// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/default_ash_event_generator_delegate.h"

#include "ash/shell.h"
#include "base/macros.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/test/event_generator_delegate_aura.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/mus/mus_client.h"

namespace {

class DefaultAshEventGeneratorDelegate
    : public aura::test::EventGeneratorDelegateAura {
 public:
  explicit DefaultAshEventGeneratorDelegate(aura::Window* root_window);
  ~DefaultAshEventGeneratorDelegate() override = default;

  // aura::test::EventGeneratorDelegateAura:
  ui::EventTarget* GetTargetAt(const gfx::Point& location) override;
  aura::client::ScreenPositionClient* GetScreenPositionClient(
      const aura::Window* window) const override;
  ui::EventDispatchDetails DispatchKeyEventToIME(ui::EventTarget* target,
                                                 ui::KeyEvent* event) override;

 private:
  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(DefaultAshEventGeneratorDelegate);
};

DefaultAshEventGeneratorDelegate::DefaultAshEventGeneratorDelegate(
    aura::Window* root_window)
    : root_window_(root_window) {}

ui::EventTarget* DefaultAshEventGeneratorDelegate::GetTargetAt(
    const gfx::Point& location) {
  return root_window_->GetHost()->window();
}

aura::client::ScreenPositionClient*
DefaultAshEventGeneratorDelegate::GetScreenPositionClient(
    const aura::Window* window) const {
  return nullptr;
}

ui::EventDispatchDetails
DefaultAshEventGeneratorDelegate::DispatchKeyEventToIME(ui::EventTarget* target,
                                                        ui::KeyEvent* event) {
  return ui::EventDispatchDetails();
}

}  // namespace

std::unique_ptr<ui::test::EventGeneratorDelegate>
CreateAshEventGeneratorDelegate(ui::test::EventGenerator* owner,
                                gfx::NativeWindow root_window,
                                gfx::NativeWindow window) {
  // Tests should not create event generators for a "root window" that's not
  // actually the root window.
  if (root_window)
    DCHECK_EQ(root_window, root_window->GetRootWindow());

  // Do not create EventGeneratorDelegateMus if a root window is supplied.
  // Assume that if a root is supplied the event generator should target the
  // specified window, and there is no need to dispatch remotely.
  if (features::IsUsingWindowService() && !root_window) {
    DCHECK(views::MusClient::Exists());
    return aura::test::EventGeneratorDelegateAura::Create(
        views::MusClient::Get()->window_tree_client()->connector(), owner,
        root_window, window);
  }
  return std::make_unique<DefaultAshEventGeneratorDelegate>(root_window);
}
