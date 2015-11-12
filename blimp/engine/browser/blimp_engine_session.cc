// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/browser/blimp_engine_session.h"

#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/control.pb.h"
#include "blimp/engine/browser/blimp_browser_context.h"
#include "blimp/engine/ui/blimp_layout_manager.h"
#include "blimp/engine/ui/blimp_screen.h"
#include "blimp/engine/ui/blimp_ui_context_factory.h"
#include "blimp/net/blimp_connection.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/core/focus_controller.h"

#if !defined(USE_X11)
#include "blimp/engine/ui/blimp_window_tree_host.h"
#endif

namespace blimp {
namespace engine {
namespace {

// Focus rules that support activating an child window.
class FocusRulesImpl : public wm::BaseFocusRules {
 public:
  FocusRulesImpl() {}
  ~FocusRulesImpl() override {}

  bool SupportsChildActivation(aura::Window* window) const override {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusRulesImpl);
};

}  // namespace

BlimpEngineSession::BlimpEngineSession(
    scoped_ptr<BlimpBrowserContext> browser_context)
    : browser_context_(browser_context.Pass()), screen_(new BlimpScreen) {}

BlimpEngineSession::~BlimpEngineSession() {}

void BlimpEngineSession::Initialize() {
  DCHECK(!gfx::Screen::GetScreenByType(gfx::SCREEN_TYPE_NATIVE));
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_.get());

#if defined(USE_X11)
  window_tree_host_.reset(aura::WindowTreeHost::Create(
      gfx::Rect(screen_->GetPrimaryDisplay().size())));
#else
  context_factory_.reset(new BlimpUiContextFactory());
  aura::Env::GetInstance()->set_context_factory(context_factory_.get());
  window_tree_host_.reset(new BlimpWindowTreeHost());
#endif

  window_tree_host_->InitHost();
  window_tree_host_->window()->SetLayoutManager(
      new BlimpLayoutManager(window_tree_host_->window()));
  focus_client_.reset(new wm::FocusController(new FocusRulesImpl));
  aura::client::SetFocusClient(window_tree_host_->window(),
                               focus_client_.get());
  aura::client::SetActivationClient(window_tree_host_->window(),
                                    focus_client_.get());
  capture_client_.reset(
      new aura::client::DefaultCaptureClient(window_tree_host_->window()));

#if defined(USE_X11)
  window_tree_host_->Show();
#endif

  window_tree_host_->SetBounds(gfx::Rect(screen_->GetPrimaryDisplay().size()));
}

void BlimpEngineSession::CreateWebContents(const int target_tab_id) {
  // TODO(haibinlu): Support more than one active WebContents (crbug/547231).
  DCHECK(!web_contents_);
  content::WebContents::CreateParams create_params(browser_context_.get(),
                                                   nullptr);
  scoped_ptr<content::WebContents> new_contents =
      make_scoped_ptr(content::WebContents::Create(create_params));
  PlatformSetContents(new_contents.Pass());
}

void BlimpEngineSession::CloseWebContents(const int target_tab_id) {
  DCHECK(web_contents_);
  web_contents_->Close();
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

void BlimpEngineSession::GoBack(const int target_tab_id) {
  if (!web_contents_)
    return;

  web_contents_->GetController().GoBack();
}

void BlimpEngineSession::GoForward(const int target_tab_id) {
  if (!web_contents_)
    return;

  web_contents_->GetController().GoForward();
}

void BlimpEngineSession::Reload(const int target_tab_id) {
  if (!web_contents_)
    return;

  web_contents_->GetController().Reload(true);
}

void BlimpEngineSession::ProcessMessage(
    scoped_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  DCHECK(message->type() == BlimpMessage::CONTROL ||
         message->type() == BlimpMessage::NAVIGATION);

  if (message->type() == BlimpMessage::CONTROL) {
    switch (message->control().type()) {
      case ControlMessage::CREATE_TAB:
        CreateWebContents(message->target_tab_id());
        break;
      case ControlMessage::CLOSE_TAB:
        CloseWebContents(message->target_tab_id());
      default:
        NOTIMPLEMENTED();
    }
  } else if (message->type() == BlimpMessage::NAVIGATION && web_contents_) {
    switch (message->navigation().type()) {
      case NavigationMessage::LOAD_URL:
        LoadUrl(message->target_tab_id(),
                GURL(message->navigation().load_url().url()));
        break;
      case NavigationMessage::GO_BACK:
        GoBack(message->target_tab_id());
        break;
      case NavigationMessage::GO_FORWARD:
        GoForward(message->target_tab_id());
        break;
      case NavigationMessage::RELOAD:
        Reload(message->target_tab_id());
        break;
      default:
        NOTIMPLEMENTED();
    }
  }

  if (!callback.is_null()) {
    callback.Run(net::OK);
  }
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
    NOTIMPLEMENTED();
    return nullptr;
  }

  // TODO(haibinlu): Add helper method to get LoadURLParams from OpenURLParams.
  content::NavigationController::LoadURLParams load_url_params(params.url);
  load_url_params.source_site_instance = params.source_site_instance;
  load_url_params.referrer = params.referrer;
  load_url_params.frame_tree_node_id = params.frame_tree_node_id;
  load_url_params.transition_type = params.transition;
  load_url_params.extra_headers = params.extra_headers;
  load_url_params.should_replace_current_entry =
      params.should_replace_current_entry;

  if (params.transferred_global_request_id != content::GlobalRequestID()) {
    // If transferred_global_request_id is set, then
    // the navigation is being transferred from one RenderViewHost to another.
    // Preserve the request-id and renderer-initiated flag.
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

  aura::Window* parent = window_tree_host_->window();
  aura::Window* content = web_contents_->GetNativeView();
  if (!parent->Contains(content))
    parent->AddChild(content);
  content->Show();
}

}  // namespace engine
}  // namespace blimp
