// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/content_proxy.h"

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_view_model.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/views/background.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

namespace athena {

ContentProxy::ContentProxy(views::WebView* web_view, Activity* activity)
    : web_view_(web_view),
      window_(activity->GetWindow()),
      background_color_(
          activity->GetActivityViewModel()->GetRepresentativeColor()),
      content_loaded_(true) {
  CreateProxyContent();
  HideOriginalContent();
}

ContentProxy::~ContentProxy() {
  // If we still have a connection to the original Activity, we make it visible
  // again.
  ShowOriginalContent();
  // At this point we should either have no view - or the view should not be
  // shown by its parent anymore.
  DCHECK(!proxy_content_.get() || !proxy_content_->parent());
  proxy_content_.reset();
}

void ContentProxy::ContentWillUnload() {
  content_loaded_ = false;
}

gfx::ImageSkia ContentProxy::GetContentImage() {
  // For the time being we keep this here and return an empty image.
  return image_;
}

void ContentProxy::EvictContent() {
  HideProxyContent();
  CreateSolidProxyContent();
  ShowProxyContent();
}

void ContentProxy::Reparent(aura::Window* new_parent_window) {
  if (new_parent_window == window_)
    return;

  // Since we are breaking now the connection to the old content, we make the
  // content visible again before we continue.
  // Note: Since the owning window is invisible, it does not matter that we
  // make the web content visible if the window gets destroyed shortly after.
  ShowOriginalContent();

  // Transfer the |proxy_content_| to the passed window.
  window_ = new_parent_window;
  web_view_ = NULL;

  // Move the view to the new window and show it there.
  HideOriginalContent();
}

void ContentProxy::ShowOriginalContent() {
  // Hide our content.
  HideProxyContent();

  if (web_view_) {
    // Show the original |web_view_| again.
    web_view_->SetFastResize(false);
    // If the content is loaded, we ask it to relayout itself since the
    // dimensions might have changed. If not, we will reload new content and no
    // layout is required for the old content.
    if (content_loaded_)
      web_view_->Layout();
    web_view_->GetWebContents()->GetNativeView()->Show();
    web_view_->SetVisible(true);
  }
}

void ContentProxy::HideOriginalContent() {
  if (web_view_) {
    // Hide the |web_view_|.
    // TODO(skuhne): We might consider removing the view from the window while
    // it's hidden - it should work the same way as show/hide and does not have
    // any window re-ordering effect.
    web_view_->GetWebContents()->GetNativeView()->Hide();
    web_view_->SetVisible(false);
    // Don't allow the content to get resized with window size changes.
    web_view_->SetFastResize(true);
  }

  // Show our replacement content.
  ShowProxyContent();
}

void ContentProxy::CreateProxyContent() {
  // For the time being we create only a solid color here.
  // TODO(skuhne): Replace with copying the drawn content into |proxy_content_|
  // instead.
  CreateSolidProxyContent();
}

void ContentProxy::CreateSolidProxyContent() {
  proxy_content_.reset(new views::View());
  proxy_content_->set_owned_by_client();
  proxy_content_->set_background(
      views::Background::CreateSolidBackground(background_color_));
}

void ContentProxy::ShowProxyContent() {
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window_);
  DCHECK(widget);
  views::View* client_view = widget->client_view();
  // Place the view in front of all others.
  client_view->AddChildView(proxy_content_.get());
  proxy_content_->SetSize(client_view->bounds().size());
}

void ContentProxy::HideProxyContent() {
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window_);
  views::View* client_view = widget->client_view();
  client_view->RemoveChildView(proxy_content_.get());
}

}  // namespace athena
