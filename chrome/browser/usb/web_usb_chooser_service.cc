// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_chooser_service.h"

#include <utility>

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_bubble_manager.h"
#include "chrome/browser/usb/usb_chooser_bubble_controller.h"
#include "components/bubble/bubble_controller.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"

WebUsbChooserService::WebUsbChooserService(
    content::RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(render_frame_host);
}

WebUsbChooserService::~WebUsbChooserService() {
  for (const auto& bubble : bubbles_) {
    if (bubble)
      bubble->CloseBubble(BUBBLE_CLOSE_FORCED);
  }
}

void WebUsbChooserService::GetPermission(
    mojo::Array<device::usb::DeviceFilterPtr> device_filters,
    const GetPermissionCallback& callback) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  std::unique_ptr<UsbChooserBubbleController> bubble_controller(
      new UsbChooserBubbleController(render_frame_host_,
                                     std::move(device_filters),
                                     render_frame_host_, callback));
  UsbChooserBubbleController* bubble_controller_ptr = bubble_controller.get();
  BubbleReference bubble_reference =
      browser->GetBubbleManager()->ShowBubble(std::move(bubble_controller));
  bubble_controller_ptr->set_bubble_reference(bubble_reference);
  bubbles_.push_back(bubble_reference);
}

void WebUsbChooserService::Bind(
    mojo::InterfaceRequest<device::usb::ChooserService> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bindings_.AddBinding(this, std::move(request));
}
