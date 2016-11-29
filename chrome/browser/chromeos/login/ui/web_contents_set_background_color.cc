// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/web_contents_set_background_color.h"

#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(chromeos::WebContentsSetBackgroundColor);

namespace chromeos {

// static
void WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
    content::WebContents* web_contents,
    SkColor color) {
  if (FromWebContents(web_contents))
    return;

  // SupportsUserData::Data takes ownership over the
  // WebContentsSetBackgroundColor instance and will destroy it when the
  // WebContents instance is destroyed.
  web_contents->SetUserData(
      UserDataKey(), new WebContentsSetBackgroundColor(web_contents, color));
}

WebContentsSetBackgroundColor::WebContentsSetBackgroundColor(
    content::WebContents* web_contents,
    SkColor color)
    : content::WebContentsObserver(web_contents), color_(color) {}

WebContentsSetBackgroundColor::~WebContentsSetBackgroundColor() {}

void WebContentsSetBackgroundColor::RenderViewReady() {
  web_contents()
      ->GetRenderViewHost()
      ->GetWidget()
      ->GetView()
      ->SetBackgroundColor(color_);
}

void WebContentsSetBackgroundColor::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  render_view_host->GetWidget()->GetView()->SetBackgroundColor(color_);
}

void WebContentsSetBackgroundColor::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  new_host->GetWidget()->GetView()->SetBackgroundColor(color_);
}

}  // namespace chromeos
