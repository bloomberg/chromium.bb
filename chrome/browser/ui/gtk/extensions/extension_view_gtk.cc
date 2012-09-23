// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/extensions/extension_view_gtk.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/ui/gtk/extensions/extension_popup_gtk.h"
#include "chrome/common/view_type.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

ExtensionViewGtk::ExtensionViewGtk(extensions::ExtensionHost* extension_host,
                                   Browser* browser)
    : browser_(browser),
      extension_host_(extension_host),
      container_(NULL) {
}

ExtensionViewGtk::~ExtensionViewGtk() {
}

void ExtensionViewGtk::Init() {
  CreateWidgetHostView();
}

void ExtensionViewGtk::SetBackground(const SkBitmap& background) {
  if (GetRenderViewHost()->IsRenderViewLive() &&
      GetRenderViewHost()->GetView()) {
    GetRenderViewHost()->GetView()->SetBackground(background);
  } else {
    pending_background_ = background;
  }
}

Browser* ExtensionViewGtk::GetBrowser() {
  return browser_;
}

const Browser* ExtensionViewGtk::GetBrowser() const {
  return browser_;
}

gfx::NativeView ExtensionViewGtk::GetNativeView() {
  return extension_host_->host_contents()->GetView()->GetNativeView();
}

content::RenderViewHost* ExtensionViewGtk::GetRenderViewHost() const {
  return extension_host_->render_view_host();
}

void ExtensionViewGtk::SetContainer(ExtensionViewContainer* container) {
  container_ = container;
}

void ExtensionViewGtk::ResizeDueToAutoResize(const gfx::Size& new_size) {
  if (container_)
    container_->OnExtensionSizeChanged(this, new_size);
}

void ExtensionViewGtk::RenderViewCreated() {
  if (!pending_background_.empty() && GetRenderViewHost()->GetView()) {
    GetRenderViewHost()->GetView()->SetBackground(pending_background_);
    pending_background_.reset();
  }

  chrome::ViewType host_type = extension_host_->extension_host_type();
  if (host_type == chrome::VIEW_TYPE_EXTENSION_POPUP) {
    gfx::Size min_size(ExtensionPopupGtk::kMinWidth,
                       ExtensionPopupGtk::kMinHeight);
    gfx::Size max_size(ExtensionPopupGtk::kMaxWidth,
                       ExtensionPopupGtk::kMaxHeight);
    GetRenderViewHost()->EnableAutoResize(min_size, max_size);
  }
}

void ExtensionViewGtk::DidStopLoading() {
  NOTIMPLEMENTED();
}

void ExtensionViewGtk::WindowFrameChanged() {
  NOTIMPLEMENTED();
}

void ExtensionViewGtk::CreateWidgetHostView() {
  extension_host_->CreateRenderViewSoon();
}

// static
ExtensionView* ExtensionView::Create(extensions::ExtensionHost* host,
                                     Browser* browser) {
  ExtensionViewGtk* extension_view = new ExtensionViewGtk(host, browser);
  extension_view->Init();
  return extension_view;
}
