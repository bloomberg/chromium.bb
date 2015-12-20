// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/browser/blimp_engine_session.h"

#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/tab_control.pb.h"
#include "blimp/engine/browser/blimp_browser_context.h"
#include "blimp/engine/ui/blimp_layout_manager.h"
#include "blimp/engine/ui/blimp_screen.h"
#include "blimp/engine/ui/blimp_ui_context_factory.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/blimp_message_multiplexer.h"
#include "blimp/net/browser_connection_handler.h"
#include "blimp/net/engine_authentication_handler.h"
#include "blimp/net/engine_connection_manager.h"
#include "blimp/net/null_blimp_message_processor.h"
#include "blimp/net/tcp_engine_transport.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
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
#include "ui/gfx/geometry/size.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/core/focus_controller.h"

#if !defined(USE_X11)
#include "blimp/engine/ui/blimp_window_tree_host.h"
#endif

namespace blimp {
namespace engine {
namespace {

const int kDummyTabId = 0;
const float kDefaultScaleFactor = 1.f;
const int kDefaultDisplayWidth = 800;
const int kDefaultDisplayHeight = 600;
const uint16_t kDefaultPortNumber = 25467;

base::LazyInstance<blimp::NullBlimpMessageProcessor> g_blimp_message_processor =
    LAZY_INSTANCE_INITIALIZER;

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

// This class's functions and destruction are all invoked on the IO thread by
// the BlimpEngineSession.
class BlimpNetworkComponents {
 public:
  explicit BlimpNetworkComponents(net::NetLog* net_log);
  ~BlimpNetworkComponents();

  void Initialize();

 private:
  net::NetLog* net_log_;
  scoped_ptr<BrowserConnectionHandler> connection_handler_;
  scoped_ptr<EngineAuthenticationHandler> authentication_handler_;
  scoped_ptr<EngineConnectionManager> connection_manager_;

  DISALLOW_COPY_AND_ASSIGN(BlimpNetworkComponents);
};

BlimpNetworkComponents::BlimpNetworkComponents(net::NetLog* net_log)
    : net_log_(net_log) {}

BlimpNetworkComponents::~BlimpNetworkComponents() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void BlimpNetworkComponents::Initialize() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!connection_handler_);

  // Creates and connects net components.
  // A BlimpConnection flows from
  // connection_manager_ --> authentication_handler_ --> connection_handler_
  connection_handler_.reset(new BrowserConnectionHandler);
  authentication_handler_.reset(
      new EngineAuthenticationHandler(connection_handler_.get()));
  connection_manager_.reset(
      new EngineConnectionManager(authentication_handler_.get()));

  // Adds BlimpTransports to connection_manager_.
  net::IPAddressNumber local_ip_any;
  bool success = net::ParseIPLiteralToNumber("0.0.0.0", &local_ip_any);
  DCHECK(success);
  net::IPEndPoint address(local_ip_any, kDefaultPortNumber);
  connection_manager_->AddTransport(
      make_scoped_ptr(new TCPEngineTransport(address, net_log_)));
}

BlimpEngineSession::BlimpEngineSession(
    scoped_ptr<BlimpBrowserContext> browser_context,
    net::NetLog* net_log)
    : browser_context_(std::move(browser_context)),
      screen_(new BlimpScreen),
      // TODO(dtrainor, haibinlu): Properly pull these from the BlimpMessageMux.
      render_widget_processor_(g_blimp_message_processor.Pointer(),
                               g_blimp_message_processor.Pointer()),
      net_components_(new BlimpNetworkComponents(net_log)) {
  screen_->UpdateDisplayScaleAndSize(kDefaultScaleFactor,
                                     gfx::Size(kDefaultDisplayWidth,
                                               kDefaultDisplayHeight));
  render_widget_processor_.SetDelegate(kDummyTabId, this);
}

BlimpEngineSession::~BlimpEngineSession() {
  render_widget_processor_.RemoveDelegate(kDummyTabId);

  // Safely delete network components on the IO thread.
  content::BrowserThread::DeleteSoon(content::BrowserThread::IO, FROM_HERE,
                                     net_components_.release());
}

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

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&BlimpNetworkComponents::Initialize,
                 base::Unretained(net_components_.get())));
}

void BlimpEngineSession::CreateWebContents(const int target_tab_id) {
  // TODO(haibinlu): Support more than one active WebContents (crbug/547231).
  DCHECK(!web_contents_);
  content::WebContents::CreateParams create_params(browser_context_.get(),
                                                   nullptr);
  scoped_ptr<content::WebContents> new_contents =
      make_scoped_ptr(content::WebContents::Create(create_params));
  PlatformSetContents(std::move(new_contents));
}

void BlimpEngineSession::CloseWebContents(const int target_tab_id) {
  DCHECK(web_contents_);
  web_contents_->Close();
}

void BlimpEngineSession::HandleResize(float device_pixel_ratio,
                                      const gfx::Size& size) {
  screen_->UpdateDisplayScaleAndSize(device_pixel_ratio, size);
  if (web_contents_ && web_contents_->GetRenderViewHost() &&
      web_contents_->GetRenderViewHost()->GetWidget()) {
    web_contents_->GetRenderViewHost()->GetWidget()->WasResized();
  }
}

void BlimpEngineSession::LoadUrl(const int target_tab_id, const GURL& url) {
  if (url.is_empty() || !web_contents_)
    return;

  // TODO(dtrainor, haibinlu): Fix up the URL with url_fixer.h.  If that doesn't
  // produce a valid spec() then try to build a search query?
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

void BlimpEngineSession::OnWebInputEvent(
    scoped_ptr<blink::WebInputEvent> event) {
  if (!web_contents_ || !web_contents_->GetRenderViewHost())
    return;

  // TODO(dtrainor): Send the input event directly to the render process?
}

void BlimpEngineSession::OnCompositorMessageReceived(
    const std::vector<uint8_t>& message) {
  // Make sure that We actually have a valid WebContents and RenderViewHost.
  if (!web_contents_ || !web_contents_->GetRenderViewHost())
    return;

  content::RenderWidgetHost* host =
      web_contents_->GetRenderViewHost()->GetWidget();

  // Make sure we actually have a valid RenderWidgetHost.
  if (!host)
    return;

  host->HandleCompositorProto(message);
}

void BlimpEngineSession::ProcessMessage(
    scoped_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  DCHECK(message->type() == BlimpMessage::TAB_CONTROL ||
         message->type() == BlimpMessage::NAVIGATION);

  if (message->type() == BlimpMessage::TAB_CONTROL) {
    switch (message->tab_control().type()) {
      case TabControlMessage::CREATE_TAB:
        CreateWebContents(message->target_tab_id());
        break;
      case TabControlMessage::CLOSE_TAB:
        CloseWebContents(message->target_tab_id());
      case TabControlMessage::SIZE:
        HandleResize(message->tab_control().size().device_pixel_ratio(),
                     gfx::Size(message->tab_control().size().width(),
                               message->tab_control().size().height()));
        break;
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
  if (source == web_contents_.get()) {
    Observe(nullptr);
    web_contents_.reset();
  }
}

void BlimpEngineSession::ActivateContents(content::WebContents* contents) {
  contents->GetRenderViewHost()->GetWidget()->Focus();
}

void BlimpEngineSession::ForwardCompositorProto(
    const std::vector<uint8_t>& proto) {
  render_widget_processor_.SendCompositorMessage(kDummyTabId, proto);
}

void BlimpEngineSession::NavigationStateChanged(
    content::WebContents* source,
    content::InvalidateTypes changed_flags) {
  if (source != web_contents_.get() || !changed_flags)
    return;

  NavigationMessage* navigation_message;
  scoped_ptr<BlimpMessage> message =
      CreateBlimpMessage(&navigation_message, kDummyTabId);
  navigation_message->set_type(NavigationMessage::NAVIGATION_STATE_CHANGED);
  NavigationStateChangeMessage* details =
      navigation_message->mutable_navigation_state_change();

  if (changed_flags & content::InvalidateTypes::INVALIDATE_TYPE_URL)
    details->set_url(source->GetURL().spec());

  if (changed_flags & content::InvalidateTypes::INVALIDATE_TYPE_TAB) {
    // TODO(dtrainor): Serialize the favicon?
    NOTIMPLEMENTED();
  }

  if (changed_flags & content::InvalidateTypes::INVALIDATE_TYPE_TITLE)
    details->set_title(base::UTF16ToUTF8(source->GetTitle()));

  if (changed_flags & content::InvalidateTypes::INVALIDATE_TYPE_LOAD)
    details->set_loading(source->IsLoading());

  // TODO(dtrainor, haibinlu): Send to the right BlimpMessageProcessor.
  g_blimp_message_processor.Pointer()->ProcessMessage(
      std::move(message),
      net::CompletionCallback());
}

void BlimpEngineSession::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  render_widget_processor_.OnRenderWidgetInitialized(kDummyTabId);
}

void BlimpEngineSession::PlatformSetContents(
    scoped_ptr<content::WebContents> new_contents) {
  new_contents->SetDelegate(this);
  Observe(new_contents.get());
  web_contents_ = std::move(new_contents);

  aura::Window* parent = window_tree_host_->window();
  aura::Window* content = web_contents_->GetNativeView();
  if (!parent->Contains(content))
    parent->AddChild(content);
  content->Show();
}

}  // namespace engine
}  // namespace blimp
