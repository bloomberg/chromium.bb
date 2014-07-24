// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_app_window.h"

#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension_messages.h"
#include "extensions/shell/browser/shell_extension_web_contents_observer.h"
#include "ipc/ipc_message_macros.h"

using content::BrowserContext;
using content::WebContents;

namespace extensions {

ShellAppWindow::ShellAppWindow() {
}

ShellAppWindow::~ShellAppWindow() {
}

void ShellAppWindow::Init(BrowserContext* context, gfx::Size initial_size) {
  extension_function_dispatcher_.reset(
      new ExtensionFunctionDispatcher(context, this));

  // Create the web contents with an initial size to avoid a resize.
  WebContents::CreateParams create_params(context);
  create_params.initial_size = initial_size;
  web_contents_.reset(WebContents::Create(create_params));

  content::WebContentsObserver::Observe(web_contents_.get());

  // Add support for extension startup.
  ShellExtensionWebContentsObserver::CreateForWebContents(web_contents_.get());

  SetViewType(web_contents_.get(), VIEW_TYPE_APP_WINDOW);
}

void ShellAppWindow::LoadURL(const GURL& url) {
  content::NavigationController::LoadURLParams params(url);
  web_contents_->GetController().LoadURLWithParams(params);
  web_contents_->Focus();
}

aura::Window* ShellAppWindow::GetNativeWindow() {
  return web_contents_->GetNativeView();
}

int ShellAppWindow::GetRenderViewRoutingID() {
  return web_contents_->GetRenderViewHost()->GetRoutingID();
}

bool ShellAppWindow::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ShellAppWindow, message)
  IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

content::WebContents* ShellAppWindow::GetAssociatedWebContents() const {
  return web_contents_.get();
}

void ShellAppWindow::OnRequest(const ExtensionHostMsg_Request_Params& params) {
  extension_function_dispatcher_->Dispatch(params,
                                           web_contents_->GetRenderViewHost());
}

}  // namespace extensions
