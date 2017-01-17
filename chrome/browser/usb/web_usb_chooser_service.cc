// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_chooser_service.h"

#include <utility>

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_bubble_manager.h"
#include "chrome/browser/ui/website_settings/chooser_bubble_delegate.h"
#include "chrome/browser/usb/usb_chooser_controller.h"
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
    const std::vector<device::UsbDeviceFilter>& device_filters,
    const GetPermissionCallback& callback) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  std::unique_ptr<UsbChooserController> usb_chooser_controller(
      new UsbChooserController(render_frame_host_, device_filters, callback));
  std::unique_ptr<ChooserBubbleDelegate> chooser_bubble_delegate(
      new ChooserBubbleDelegate(render_frame_host_,
                                std::move(usb_chooser_controller)));
  BubbleReference bubble_reference = browser->GetBubbleManager()->ShowBubble(
      std::move(chooser_bubble_delegate));
  bubbles_.push_back(bubble_reference);
}

void WebUsbChooserService::Bind(
    mojo::InterfaceRequest<device::usb::ChooserService> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bindings_.AddBinding(this, std::move(request));
}
