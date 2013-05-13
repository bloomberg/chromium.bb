// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/notifications/balloon_view_host.h"

#include "chrome/browser/notifications/balloon.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"

class BalloonViewHostView : public views::NativeViewHost {
 public:
  explicit BalloonViewHostView(BalloonViewHost* host)
      : host_(host),
        initialized_(false) {
  }

  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE {
    NativeViewHost::ViewHierarchyChanged(details);
    if (details.is_add && GetWidget() && !initialized_) {
      initialized_ = true;
      host_->Init(GetWidget()->GetNativeView());
    }
  }

 private:
  // The host owns this object.
  BalloonViewHost* host_;

  bool initialized_;
};

BalloonViewHost::BalloonViewHost(Balloon* balloon)
    : BalloonHost(balloon) {
  native_host_ = new BalloonViewHostView(this);
}

BalloonViewHost::~BalloonViewHost() {
  Shutdown();
}

void BalloonViewHost::Init(gfx::NativeView parent_native_view) {
  parent_native_view_ = parent_native_view;
  BalloonHost::Init();

  content::RenderWidgetHostView* render_widget_host_view =
      web_contents_->GetRenderViewHost()->GetView();

  native_host_->Attach(render_widget_host_view->GetNativeView());
}
