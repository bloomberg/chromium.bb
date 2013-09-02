// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/first_run_view.h"

#include "chrome/browser/ui/webui/chromeos/first_run/first_run_ui.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
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
  web_view_->RequestFocus();

  web_view_->web_contents()->SetDelegate(this);

  SkBitmap background;
  background.setConfig(SkBitmap::kA8_Config, 1, 1);
  background.allocPixels();
  background.eraseARGB(0, 0, 0, 0);
  web_view_->web_contents()->GetRenderViewHost()->GetView()->
    SetBackground(background);
}

FirstRunActor* FirstRunView::GetActor() {
  return static_cast<FirstRunUI*>(
      web_view_->web_contents()->GetWebUI()->GetController())->get_actor();
}

void FirstRunView::Layout() {
  web_view_->SetBoundsRect(bounds());
}

bool FirstRunView::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Discards context menu.
  return true;
}

}  // namespace chromeos

