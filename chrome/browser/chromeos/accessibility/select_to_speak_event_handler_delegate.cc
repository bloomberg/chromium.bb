// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/select_to_speak_event_handler_delegate.h"

#include <memory>

#include "ash/public/interfaces/constants.mojom.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/event_handler_common.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/content_accelerators/accelerator_util.h"
#include "ui/display/display.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/events/event.h"
#include "ui/events/event_sink.h"

namespace chromeos {

namespace {

gfx::PointF GetScreenLocationFromEvent(const ui::LocatedEvent& event) {
  aura::Window* root =
      static_cast<aura::Window*>(event.target())->GetRootWindow();
  aura::client::ScreenPositionClient* spc =
      aura::client::GetScreenPositionClient(root);
  if (!spc)
    return event.root_location_f();

  gfx::PointF screen_location(event.root_location_f());
  spc->ConvertPointToScreen(root, &screen_location);
  return screen_location;
}

}  // namespace

SelectToSpeakEventHandlerDelegate::SelectToSpeakEventHandlerDelegate()
    : binding_(this) {
  // Connect to ash's AccessibilityController interface.
  ash::mojom::AccessibilityControllerPtr accessibility_controller;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &accessibility_controller);

  // Set this object as the SelectToSpeakEventHandlerDelegate.
  ash::mojom::SelectToSpeakEventHandlerDelegatePtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  accessibility_controller->SetSelectToSpeakEventHandlerDelegate(
      std::move(ptr));
}

SelectToSpeakEventHandlerDelegate::~SelectToSpeakEventHandlerDelegate() =
    default;

void SelectToSpeakEventHandlerDelegate::DispatchKeyEvent(
    std::unique_ptr<ui::Event> event) {
  // We can only call the STS extension on the UI thread, make sure we
  // don't ever try to run this code on some other thread.
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(event->IsKeyEvent());

  extensions::ExtensionHost* host = chromeos::GetAccessibilityExtensionHost(
      extension_misc::kSelectToSpeakExtensionId);
  if (!host)
    return;

  const ui::KeyEvent* key_event = event->AsKeyEvent();
  chromeos::ForwardKeyToExtension(*key_event, host);
}

void SelectToSpeakEventHandlerDelegate::DispatchMouseEvent(
    std::unique_ptr<ui::Event> event) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(event->IsMouseEvent());
  extensions::ExtensionHost* host = chromeos::GetAccessibilityExtensionHost(
      extension_misc::kSelectToSpeakExtensionId);
  if (!host)
    return;

  content::RenderViewHost* rvh = host->render_view_host();
  if (!rvh)
    return;

  const ui::MouseEvent* mouse_event = event->AsMouseEvent();
  const blink::WebMouseEvent web_event = ui::MakeWebMouseEvent(
      *mouse_event, base::BindRepeating(&GetScreenLocationFromEvent));
  rvh->GetWidget()->ForwardMouseEvent(web_event);
}

}  // namespace chromeos
