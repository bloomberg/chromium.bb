// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_keyboard_controller_client.h"

#include <memory>

#include "ash/public/interfaces/constants.mojom.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_private_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/virtual_keyboard_private.h"
#include "extensions/common/extension_messages.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/keyboard/keyboard_controller.h"

namespace virtual_keyboard_private = extensions::api::virtual_keyboard_private;

namespace {

static ChromeKeyboardControllerClient* g_chrome_keyboard_controller_client =
    nullptr;

Profile* GetProfile() {
  // Always use the active profile for generating keyboard events so that any
  // virtual keyboard extensions associated with the active user are notified.
  // (Note: UI and associated extensions only exist for the active user).
  return ProfileManager::GetActiveUserProfile();
}

}  // namespace

// static
ChromeKeyboardControllerClient* ChromeKeyboardControllerClient::Get() {
  CHECK(g_chrome_keyboard_controller_client)
      << "ChromeKeyboardControllerClient::Get() called before Initialize()";
  return g_chrome_keyboard_controller_client;
}

ChromeKeyboardControllerClient::ChromeKeyboardControllerClient() {
  CHECK(!g_chrome_keyboard_controller_client);
  g_chrome_keyboard_controller_client = this;

  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &keyboard_controller_ptr_);

  // Request the configuration. This will be queued until the service is ready.
  keyboard_controller_ptr_->GetKeyboardConfig(base::BindOnce(
      &ChromeKeyboardControllerClient::OnGetInitialKeyboardConfig,
      weak_ptr_factory_.GetWeakPtr()));
}

ChromeKeyboardControllerClient::~ChromeKeyboardControllerClient() {
  CHECK(g_chrome_keyboard_controller_client);
  g_chrome_keyboard_controller_client = nullptr;
}

void ChromeKeyboardControllerClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ChromeKeyboardControllerClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

keyboard::mojom::KeyboardConfig
ChromeKeyboardControllerClient::GetKeyboardConfig() {
  if (!cached_keyboard_config_) {
    // Unlikely edge case (called before the Ash mojo service replies to the
    // initial GetKeyboardConfig request). Return the default value.
    return keyboard::mojom::KeyboardConfig();
  }
  return *cached_keyboard_config_.get();
}

void ChromeKeyboardControllerClient::SetKeyboardConfig(
    const keyboard::mojom::KeyboardConfig& config) {
  // Update the cache immediately.
  cached_keyboard_config_ = keyboard::mojom::KeyboardConfig::New(config);
  keyboard_controller_ptr_->SetKeyboardConfig(cached_keyboard_config_.Clone());
}

void ChromeKeyboardControllerClient::OnGetInitialKeyboardConfig(
    keyboard::mojom::KeyboardConfigPtr config) {
  // Only set the cached value if not already set by SetKeyboardConfig (the
  // set value will override the initial value once processed).
  if (!cached_keyboard_config_)
    cached_keyboard_config_ = std::move(config);

  // Add this as a KeyboardController observer now that the service is ready.
  ash::mojom::KeyboardControllerObserverAssociatedPtrInfo ptr_info;
  keyboard_controller_observer_binding_.Bind(mojo::MakeRequest(&ptr_info));
  keyboard_controller_ptr_->AddObserver(std::move(ptr_info));
}

void ChromeKeyboardControllerClient::OnKeyboardVisibleBoundsChanged(
    const gfx::Rect& bounds) {
  Profile* profile = GetProfile();
  extensions::EventRouter* router = extensions::EventRouter::Get(profile);

  if (!router->HasEventListener(
          virtual_keyboard_private::OnBoundsChanged::kEventName)) {
    return;
  }

  auto event_args = std::make_unique<base::ListValue>();
  auto new_bounds = std::make_unique<base::DictionaryValue>();
  new_bounds->SetInteger("left", bounds.x());
  new_bounds->SetInteger("top", bounds.y());
  new_bounds->SetInteger("width", bounds.width());
  new_bounds->SetInteger("height", bounds.height());
  event_args->Append(std::move(new_bounds));

  auto event = std::make_unique<extensions::Event>(
      extensions::events::VIRTUAL_KEYBOARD_PRIVATE_ON_BOUNDS_CHANGED,
      virtual_keyboard_private::OnBoundsChanged::kEventName,
      std::move(event_args), profile);
  router->BroadcastEvent(std::move(event));
}

void ChromeKeyboardControllerClient::OnKeyboardWindowDestroyed() {
  Profile* profile = GetProfile();
  extensions::EventRouter* router = extensions::EventRouter::Get(profile);

  if (!router->HasEventListener(
          virtual_keyboard_private::OnKeyboardClosed::kEventName)) {
    return;
  }

  auto event = std::make_unique<extensions::Event>(
      extensions::events::VIRTUAL_KEYBOARD_PRIVATE_ON_KEYBOARD_CLOSED,
      virtual_keyboard_private::OnKeyboardClosed::kEventName,
      std::make_unique<base::ListValue>(), profile);
  router->BroadcastEvent(std::move(event));
}

void ChromeKeyboardControllerClient::OnKeyboardConfigChanged(
    keyboard::mojom::KeyboardConfigPtr config) {
  cached_keyboard_config_ = std::move(config);
  extensions::VirtualKeyboardAPI* api =
      extensions::BrowserContextKeyedAPIFactory<
          extensions::VirtualKeyboardAPI>::Get(GetProfile());
  api->delegate()->OnKeyboardConfigChanged();
}

void ChromeKeyboardControllerClient::OnKeyboardVisibilityChanged(bool visible) {
  for (auto& observer : observers_)
    observer.OnKeyboardVisibilityChanged(visible);
}
