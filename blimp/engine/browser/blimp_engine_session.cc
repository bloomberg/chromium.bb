// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/browser/blimp_engine_session.h"

#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/control.pb.h"
#include "blimp/engine/browser/blimp_browser_context.h"
#include "blimp/engine/ui/blimp_screen.h"
#include "blimp/net/blimp_connection.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"

namespace blimp {
namespace engine {

BlimpEngineSession::BlimpEngineSession(
    scoped_ptr<BlimpBrowserContext> browser_context)
    : browser_context_(browser_context.Pass()), screen_(new BlimpScreen) {}

BlimpEngineSession::~BlimpEngineSession() {}

void BlimpEngineSession::Initialize() {
  DCHECK(!gfx::Screen::GetScreenByType(gfx::SCREEN_TYPE_NATIVE));
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_.get());
}

void BlimpEngineSession::AttachClientConnection(
    scoped_ptr<BlimpConnection> client_connection) {
  DCHECK(client_connection);
  client_connection_ = client_connection.Pass();

  // TODO(haibinlu): remove this once we can use client connection to send in
  // a navigation message.
  BlimpMessage message;
  message.set_type(BlimpMessage::CONTROL);
  message.mutable_control()->set_type(ControlMessage::CREATE_TAB);
  OnBlimpMessage(message);
  message.mutable_control()->set_type(ControlMessage::LOAD_URL);
  message.mutable_control()->mutable_load_url()->set_url(
      "https://www.google.com/");
  OnBlimpMessage(message);
}

void BlimpEngineSession::CreateWebContents(const int target_tab_id) {
  if (web_contents_) {
    // only one web_contents is supported for blimp 0.5
    NOTIMPLEMENTED();
    return;
  }

  content::WebContents::CreateParams create_params(browser_context_.get(),
                                                   nullptr);
  scoped_ptr<content::WebContents> new_contents =
      make_scoped_ptr(content::WebContents::Create(create_params));
  PlatformSetContents(new_contents.Pass());
}

void BlimpEngineSession::LoadUrl(const int target_tab_id, const GURL& url) {
  if (url.is_empty() || !web_contents_)
    return;

  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents_->GetController().LoadURLWithParams(params);
  web_contents_->Focus();
}

net::Error BlimpEngineSession::OnBlimpMessage(const BlimpMessage& message) {
  DCHECK(message.type() == BlimpMessage::CONTROL);

  switch (message.control().type()) {
    case ControlMessage::CREATE_TAB:
      CreateWebContents(message.target_tab_id());
      break;
    case ControlMessage::LOAD_URL:
      LoadUrl(message.target_tab_id(),
              GURL(message.control().load_url().url()));
      break;
    default:
      NOTIMPLEMENTED();
  }

  return net::OK;
}

void BlimpEngineSession::AddNewContents(content::WebContents* source,
                                        content::WebContents* new_contents,
                                        WindowOpenDisposition disposition,
                                        const gfx::Rect& initial_rect,
                                        bool user_gesture,
                                        bool* was_blocked) {
  NOTIMPLEMENTED();
}

content::WebContents* BlimpEngineSession::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  // CURRENT_TAB is the only one we implement for now.
  if (params.disposition != CURRENT_TAB) {
    // TODO(haibinlu): handle other disposition properly.
    NOTIMPLEMENTED();
    return nullptr;
  }
  // TOOD(haibinlu): add helper method to get LoadURLParams from OpenURLParams.
  content::NavigationController::LoadURLParams load_url_params(params.url);
  load_url_params.source_site_instance = params.source_site_instance;
  load_url_params.referrer = params.referrer;
  load_url_params.frame_tree_node_id = params.frame_tree_node_id;
  load_url_params.transition_type = params.transition;
  load_url_params.extra_headers = params.extra_headers;
  load_url_params.should_replace_current_entry =
      params.should_replace_current_entry;

  if (params.transferred_global_request_id != content::GlobalRequestID()) {
    // The navigation as being transferred from one RVH to another.
    // Copies the request ID of the old request.
    load_url_params.is_renderer_initiated = params.is_renderer_initiated;
    load_url_params.transferred_global_request_id =
        params.transferred_global_request_id;
  } else if (params.is_renderer_initiated) {
    load_url_params.is_renderer_initiated = true;
  }

  source->GetController().LoadURLWithParams(load_url_params);
  return source;
}

void BlimpEngineSession::RequestToLockMouse(content::WebContents* web_contents,
                                            bool user_gesture,
                                            bool last_unlocked_by_target) {
  web_contents->GotResponseToLockMouseRequest(true);
}

void BlimpEngineSession::CloseContents(content::WebContents* source) {
  if (source == web_contents_)
    web_contents_.reset();
}

void BlimpEngineSession::ActivateContents(content::WebContents* contents) {
  contents->GetRenderViewHost()->GetWidget()->Focus();
}

void BlimpEngineSession::PlatformSetContents(
    scoped_ptr<content::WebContents> new_contents) {
  new_contents->SetDelegate(this);
  web_contents_ = new_contents.Pass();
}

}  // namespace engine
}  // namespace blimp
