// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/accessibility/accessibility_controller_client.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

AccessibilityControllerClient* g_instance = nullptr;

void SetAutomationManagerEnabled(content::BrowserContext* context,
                                 bool enabled) {
  DCHECK(context);
  AutomationManagerAura* manager = AutomationManagerAura::GetInstance();
  if (enabled)
    manager->Enable(context);
  else
    manager->Disable();
}

}  // namespace

AccessibilityControllerClient::AccessibilityControllerClient()
    : binding_(this) {
  DCHECK(!g_instance);
  g_instance = this;
}

AccessibilityControllerClient::~AccessibilityControllerClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
AccessibilityControllerClient* AccessibilityControllerClient::Get() {
  return g_instance;
}

void AccessibilityControllerClient::Init() {
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &accessibility_controller_);
  BindAndSetClient();
}

void AccessibilityControllerClient::InitForTesting(
    ash::mojom::AccessibilityControllerPtr controller) {
  accessibility_controller_ = std::move(controller);
  BindAndSetClient();
}

void AccessibilityControllerClient::TriggerAccessibilityAlert(
    ash::mojom::AccessibilityAlert alert) {
  last_a11y_alert_for_test_ = alert;

  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile)
    return;

  int msg = 0;
  switch (alert) {
    case ash::mojom::AccessibilityAlert::CAPS_ON:
      msg = IDS_A11Y_ALERT_CAPS_ON;
      break;
    case ash::mojom::AccessibilityAlert::CAPS_OFF:
      msg = IDS_A11Y_ALERT_CAPS_OFF;
      break;
    case ash::mojom::AccessibilityAlert::SCREEN_ON:
      // Enable automation manager when alert is screen-on, as it is
      // previously disabled by alert screen-off.
      SetAutomationManagerEnabled(profile, true);
      msg = IDS_A11Y_ALERT_SCREEN_ON;
      break;
    case ash::mojom::AccessibilityAlert::SCREEN_OFF:
      msg = IDS_A11Y_ALERT_SCREEN_OFF;
      break;
    case ash::mojom::AccessibilityAlert::WINDOW_MOVED_TO_ABOVE_DISPLAY:
      msg = IDS_A11Y_ALERT_WINDOW_MOVED_TO_ABOVE_DISPLAY;
      break;
    case ash::mojom::AccessibilityAlert::WINDOW_MOVED_TO_BELOW_DISPLAY:
      msg = IDS_A11Y_ALERT_WINDOW_MOVED_TO_BELOW_DISPLAY;
      break;
    case ash::mojom::AccessibilityAlert::WINDOW_MOVED_TO_LEFT_DISPLAY:
      msg = IDS_A11Y_ALERT_WINDOW_MOVED_TO_LEFT_DISPLAY;
      break;
    case ash::mojom::AccessibilityAlert::WINDOW_MOVED_TO_RIGHT_DISPLAY:
      msg = IDS_A11Y_ALERT_WINDOW_MOVED_TO_RIGHT_DISPLAY;
      break;
    case ash::mojom::AccessibilityAlert::WINDOW_NEEDED:
      msg = IDS_A11Y_ALERT_WINDOW_NEEDED;
      break;
    case ash::mojom::AccessibilityAlert::WINDOW_OVERVIEW_MODE_ENTERED:
      msg = IDS_A11Y_ALERT_WINDOW_OVERVIEW_MODE_ENTERED;
      break;
    case ash::mojom::AccessibilityAlert::NONE:
      msg = 0;
      break;
  }

  if (msg) {
    AutomationManagerAura::GetInstance()->HandleAlert(
        profile, l10n_util::GetStringUTF8(msg));
    // After handling the alert, if the alert is screen-off, we should
    // disable automation manager to handle any following a11y events.
    if (alert == ash::mojom::AccessibilityAlert::SCREEN_OFF)
      SetAutomationManagerEnabled(profile, false);
  }
}

void AccessibilityControllerClient::FlushForTesting() {
  accessibility_controller_.FlushForTesting();
}

void AccessibilityControllerClient::BindAndSetClient() {
  ash::mojom::AccessibilityControllerClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  accessibility_controller_->SetClient(std::move(client));
}
