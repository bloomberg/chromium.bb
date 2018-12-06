// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/switch_access_event_handler_delegate.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "chrome/browser/chromeos/accessibility/event_handler_common.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/extension_host.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/events/event.h"

SwitchAccessEventHandlerDelegate::SwitchAccessEventHandlerDelegate()
    : binding_(this) {
  // Connect to ash's AccessibilityController interface.
  ash::mojom::AccessibilityControllerPtr accessibility_controller;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &accessibility_controller);

  // Set this object as the SwitchAccessEventHandlerDelegate.
  ash::mojom::SwitchAccessEventHandlerDelegatePtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  accessibility_controller->SetSwitchAccessEventHandlerDelegate(std::move(ptr));
}

SwitchAccessEventHandlerDelegate::~SwitchAccessEventHandlerDelegate() = default;

void SwitchAccessEventHandlerDelegate::DispatchKeyEvent(
    std::unique_ptr<ui::Event> event) {
  // We can only call the Switch Access extension on the UI thread, make sure we
  // don't ever try to run this code on some other thread.
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(event->IsKeyEvent());

  extensions::ExtensionHost* host = chromeos::GetAccessibilityExtensionHost(
      extension_misc::kSwitchAccessExtensionId);
  if (!host)
    return;

  const ui::KeyEvent* key_event = event->AsKeyEvent();
  chromeos::ForwardKeyToExtension(*key_event, host);
}
