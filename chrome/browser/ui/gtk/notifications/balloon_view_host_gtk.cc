// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/notifications/balloon_view_host_gtk.h"

#include <gtk/gtk.h>

#include "chrome/browser/notifications/balloon.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

BalloonViewHost::BalloonViewHost(Balloon* balloon)
    : BalloonHost(balloon) {
}

BalloonViewHost::~BalloonViewHost() {
  Shutdown();
}

void BalloonViewHost::UpdateActualSize(const gfx::Size& new_size) {
  content::RenderViewHost* host = web_contents_->GetRenderViewHost();
  if (host) {
    content::RenderWidgetHostView* view = host->GetView();
    if (view)
      view->SetSize(new_size);
  }

  gtk_widget_set_size_request(
      native_view(), new_size.width(), new_size.height());
}

gfx::NativeView BalloonViewHost::native_view() const {
  return web_contents_->GetView()->GetNativeView();
}
