// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/browser/shell_app_window_delegate.h"

#include "apps/ui/views/native_app_window_views.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/aura/window.h"

namespace apps {

ShellAppWindowDelegate::ShellAppWindowDelegate() {}

ShellAppWindowDelegate::~ShellAppWindowDelegate() {}

void ShellAppWindowDelegate::InitWebContents(
    content::WebContents* web_contents) {}

NativeAppWindow* ShellAppWindowDelegate::CreateNativeAppWindow(
    AppWindow* window,
    const AppWindow::CreateParams& params) {
  NativeAppWindowViews* native_app_window = new NativeAppWindowViews;
  native_app_window->Init(window, params);
  return native_app_window;
}

content::WebContents* ShellAppWindowDelegate::OpenURLFromTab(
    content::BrowserContext* context,
    content::WebContents* source,
    const content::OpenURLParams& params) {
  return NULL;
}

void ShellAppWindowDelegate::AddNewContents(content::BrowserContext* context,
                                            content::WebContents* new_contents,
                                            WindowOpenDisposition disposition,
                                            const gfx::Rect& initial_pos,
                                            bool user_gesture,
                                            bool* was_blocked) {
  LOG(ERROR) << "app_shell does not support opening a new tab/window.";
}

content::ColorChooser* ShellAppWindowDelegate::ShowColorChooser(
    content::WebContents* web_contents,
    SkColor initial_color) {
  return NULL;
}

void ShellAppWindowDelegate::RunFileChooser(
    content::WebContents* tab,
    const content::FileChooserParams& params) {
  LOG(ERROR) << "app_shell does not support file pickers.";
}

void ShellAppWindowDelegate::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  // TODO(jamescook): Support media capture.
  LOG(ERROR) << "app_shell does not support media capture.";
}

int ShellAppWindowDelegate::PreferredIconSize() {
  // Pick an arbitrary size.
  return 32;
}

void ShellAppWindowDelegate::SetWebContentsBlocked(
    content::WebContents* web_contents,
    bool blocked) {}

bool ShellAppWindowDelegate::IsWebContentsVisible(
    content::WebContents* web_contents) {
  aura::Window* native_window = web_contents->GetView()->GetNativeView();
  return native_window->IsVisible();
}

}  // namespace apps
