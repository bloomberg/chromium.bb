// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/first_run_view.h"

#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/ui/webui/chromeos/first_run/first_run_ui.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/views/controls/webview/webview.h"
#include "url/gurl.h"

namespace chromeos {

FirstRunView::FirstRunView()
    : web_view_(NULL) {
}

void FirstRunView::Init(content::BrowserContext* context) {
  web_view_ = new views::WebView(context);
  AddChildView(web_view_);
  web_view_->LoadInitialURL(GURL(chrome::kChromeUIFirstRunURL));

  content::WebContents* web_contents = web_view_->web_contents();
  web_contents->SetDelegate(this);
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents);

  web_contents->GetRenderViewHost()->GetView()->SetBackgroundOpaque(false);
}

FirstRunActor* FirstRunView::GetActor() {
  return static_cast<FirstRunUI*>(
      web_view_->web_contents()->GetWebUI()->GetController())->get_actor();
}

void FirstRunView::Layout() {
  web_view_->SetBoundsRect(bounds());
}

void FirstRunView::RequestFocus() {
  web_view_->RequestFocus();
}

content::WebContents* FirstRunView::GetWebContents() {
  return web_view_->web_contents();
}

bool FirstRunView::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Discards context menu.
  return true;
}

bool FirstRunView::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  // Disable pinch zooming.
  return event.type == blink::WebGestureEvent::GesturePinchBegin ||
      event.type == blink::WebGestureEvent::GesturePinchUpdate ||
      event.type == blink::WebGestureEvent::GesturePinchEnd;
}

}  // namespace chromeos

