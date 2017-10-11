// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_web_contents_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/renderer_preferences.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_browser_main_parts.h"
#include "headless/lib/browser/headless_tab_socket_impl.h"
#include "headless/public/internal/headless_devtools_client_impl.h"
#include "printing/features/features.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/compositor/compositor.h"

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
#include "headless/lib/browser/headless_print_manager.h"
#endif

namespace headless {

// static
HeadlessWebContentsImpl* HeadlessWebContentsImpl::From(
    HeadlessWebContents* web_contents) {
  // This downcast is safe because there is only one implementation of
  // HeadlessWebContents.
  return static_cast<HeadlessWebContentsImpl*>(web_contents);
}

// static
HeadlessWebContentsImpl* HeadlessWebContentsImpl::From(
    HeadlessBrowser* browser,
    content::WebContents* contents) {
  return HeadlessWebContentsImpl::From(
      browser->GetWebContentsForDevToolsAgentHostId(
          content::DevToolsAgentHost::GetOrCreateFor(contents)->GetId()));
}

class HeadlessWebContentsImpl::Delegate : public content::WebContentsDelegate {
 public:
  explicit Delegate(HeadlessWebContentsImpl* headless_web_contents)
      : headless_web_contents_(headless_web_contents) {}

  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override {
    DCHECK(new_contents->GetBrowserContext() ==
           headless_web_contents_->browser_context());

    std::unique_ptr<HeadlessWebContentsImpl> child_contents =
        HeadlessWebContentsImpl::CreateForChildContents(headless_web_contents_,
                                                        new_contents);
    HeadlessWebContentsImpl* raw_child_contents = child_contents.get();
    headless_web_contents_->browser_context()->RegisterWebContents(
        std::move(child_contents));
    headless_web_contents_->browser_context()->NotifyChildContentsCreated(
        headless_web_contents_, raw_child_contents);
  }

#if !defined(CHROME_MULTIPLE_DLL_CHILD)
  // Return the security style of the given |web_contents|, populating
  // |security_style_explanations| to explain why the SecurityStyle was chosen.
  blink::WebSecurityStyle GetSecurityStyle(
      content::WebContents* web_contents,
      content::SecurityStyleExplanations* security_style_explanations)
      override {
    security_state::SecurityInfo security_info;
    security_state::GetSecurityInfo(
        security_state::GetVisibleSecurityState(web_contents),
        false /* used_policy_installed_certificate */,
        base::Bind(&content::IsOriginSecure), &security_info);
    return security_state::GetSecurityStyle(security_info,
                                            security_style_explanations);
  }
#endif  // !defined(CHROME_MULTIPLE_DLL_CHILD)

  void ActivateContents(content::WebContents* contents) override {
    contents->GetRenderViewHost()->GetWidget()->Focus();
  }

  void CloseContents(content::WebContents* source) override {
    auto* const headless_contents =
        HeadlessWebContentsImpl::From(browser(), source);
    DCHECK(headless_contents);
    headless_contents->Close();
  }

  void AddNewContents(content::WebContents* source,
                      content::WebContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override {
    const gfx::Rect default_rect(
        headless_web_contents_->browser()->options()->window_size);
    const gfx::Rect rect = initial_rect.IsEmpty() ? default_rect : initial_rect;
    auto* const headless_contents =
        HeadlessWebContentsImpl::From(browser(), new_contents);
    DCHECK(headless_contents);
    headless_contents->SetBounds(rect);
  }

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override {
    DCHECK_EQ(source, headless_web_contents_->web_contents());
    content::WebContents* target = nullptr;
    switch (params.disposition) {
      case WindowOpenDisposition::CURRENT_TAB:
        target = source;
        break;

      case WindowOpenDisposition::NEW_POPUP:
      case WindowOpenDisposition::NEW_WINDOW:
      case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      case WindowOpenDisposition::NEW_FOREGROUND_TAB: {
        HeadlessWebContentsImpl* child_contents = HeadlessWebContentsImpl::From(
            headless_web_contents_->browser_context()
                ->CreateWebContentsBuilder()
                .SetAllowTabSockets(
                    !!headless_web_contents_->GetHeadlessTabSocket())
                .SetWindowSize(source->GetContainerBounds().size())
                .Build());
        headless_web_contents_->browser_context()->NotifyChildContentsCreated(
            headless_web_contents_, child_contents);
        target = child_contents->web_contents();
        break;
      }

      // TODO(veluca): add support for other disposition types.
      case WindowOpenDisposition::SINGLETON_TAB:
      case WindowOpenDisposition::OFF_THE_RECORD:
      case WindowOpenDisposition::SAVE_TO_DISK:
      case WindowOpenDisposition::IGNORE_ACTION:
      default:
        return nullptr;
    }

    content::NavigationController::LoadURLParams load_url_params(params.url);
    load_url_params.source_site_instance = params.source_site_instance;
    load_url_params.transition_type = params.transition;
    load_url_params.frame_tree_node_id = params.frame_tree_node_id;
    load_url_params.referrer = params.referrer;
    load_url_params.redirect_chain = params.redirect_chain;
    load_url_params.extra_headers = params.extra_headers;
    load_url_params.is_renderer_initiated = params.is_renderer_initiated;
    load_url_params.should_replace_current_entry =
        params.should_replace_current_entry;

    if (params.uses_post) {
      load_url_params.load_type =
          content::NavigationController::LOAD_TYPE_HTTP_POST;
      load_url_params.post_data = params.post_data;
    }

    target->GetController().LoadURLWithParams(load_url_params);
    return target;
  }

 private:
  HeadlessBrowserImpl* browser() { return headless_web_contents_->browser(); }

  HeadlessWebContentsImpl* headless_web_contents_;  // Not owned.
  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

namespace {

void CreateTabSocketMojoServiceForContents(
    HeadlessWebContents* web_contents,
    mojo::ScopedMessagePipeHandle handle) {
  HeadlessWebContentsImpl::From(web_contents)
      ->CreateTabSocketMojoService(std::move(handle));
}

}  // namespace

struct HeadlessWebContentsImpl::PendingFrame {
 public:
  PendingFrame() {}
  ~PendingFrame() {}

  bool MaybeRunCallback() {
    if (wait_for_copy_result || !display_did_finish_frame)
      return false;
    callback.Run(has_damage, std::move(bitmap));
    return true;
  }

  uint64_t sequence_number = 0;
  bool wait_for_copy_result = false;
  bool display_did_finish_frame = false;
  bool has_damage = false;
  std::unique_ptr<SkBitmap> bitmap;
  FrameFinishedCallback callback;

 private:
  DISALLOW_COPY_AND_ASSIGN(PendingFrame);
};

// static
std::unique_ptr<HeadlessWebContentsImpl> HeadlessWebContentsImpl::Create(
    HeadlessWebContents::Builder* builder) {
  content::WebContents::CreateParams create_params(builder->browser_context_,
                                                   nullptr);
  create_params.initial_size = builder->window_size_;

  std::unique_ptr<HeadlessWebContentsImpl> headless_web_contents =
      base::WrapUnique(new HeadlessWebContentsImpl(
          content::WebContents::Create(create_params),
          builder->browser_context_));

  if (builder->tab_sockets_allowed_) {
    headless_web_contents->headless_tab_socket_ =
        base::MakeUnique<HeadlessTabSocketImpl>(
            headless_web_contents->web_contents_.get());
    headless_web_contents->inject_mojo_services_into_isolated_world_ = true;

    builder->mojo_services_.emplace_back(
        TabSocket::Name_, base::Bind(&CreateTabSocketMojoServiceForContents));
  }

  headless_web_contents->mojo_services_ = std::move(builder->mojo_services_);
  headless_web_contents->begin_frame_control_enabled_ =
      builder->enable_begin_frame_control_;
  headless_web_contents->InitializeWindow(gfx::Rect(builder->window_size_));
  if (!headless_web_contents->OpenURL(builder->initial_url_))
    return nullptr;
  return headless_web_contents;
}

// static
std::unique_ptr<HeadlessWebContentsImpl>
HeadlessWebContentsImpl::CreateForChildContents(
    HeadlessWebContentsImpl* parent,
    content::WebContents* child_contents) {
  auto child = base::WrapUnique(
      new HeadlessWebContentsImpl(child_contents, parent->browser_context()));

  // Child contents have their own root window and inherit the BeginFrameControl
  // setting.
  child->begin_frame_control_enabled_ = parent->begin_frame_control_enabled_;
  child->InitializeWindow(child_contents->GetContainerBounds());

  // Copy mojo services and tab socket settings from parent.
  child->mojo_services_ = parent->mojo_services_;
  if (parent->headless_tab_socket_) {
    child->headless_tab_socket_ =
        base::MakeUnique<HeadlessTabSocketImpl>(child_contents);
    child->inject_mojo_services_into_isolated_world_ =
        parent->inject_mojo_services_into_isolated_world_;
  }

  // There may already be frames, so make sure they also have our services.
  for (content::RenderFrameHost* frame_host : child_contents->GetAllFrames())
    child->RenderFrameCreated(frame_host);

  return child;
}

void HeadlessWebContentsImpl::InitializeWindow(
    const gfx::Rect& initial_bounds) {
  static int window_id = 1;
  window_id_ = window_id++;
  window_state_ = "normal";

  browser()->PlatformInitializeWebContents(this);
  SetBounds(initial_bounds);

  if (begin_frame_control_enabled_) {
    ui::Compositor* compositor = browser()->PlatformGetCompositor(this);
    DCHECK(compositor);
    compositor->SetExternalBeginFrameClient(this);
  }
}

void HeadlessWebContentsImpl::SetBounds(const gfx::Rect& bounds) {
  browser()->PlatformSetWebContentsBounds(this, bounds);
}

HeadlessWebContentsImpl::HeadlessWebContentsImpl(
    content::WebContents* web_contents,
    HeadlessBrowserContextImpl* browser_context)
    : content::WebContentsObserver(web_contents),
      web_contents_delegate_(new HeadlessWebContentsImpl::Delegate(this)),
      web_contents_(web_contents),
      agent_host_(content::DevToolsAgentHost::GetOrCreateFor(web_contents)),
      inject_mojo_services_into_isolated_world_(false),
      browser_context_(browser_context),
      render_process_host_(web_contents->GetMainFrame()->GetProcess()),
      weak_ptr_factory_(this) {
#if BUILDFLAG(ENABLE_BASIC_PRINTING) && !defined(CHROME_MULTIPLE_DLL_CHILD)
  HeadlessPrintManager::CreateForWebContents(web_contents);
#endif
  web_contents->GetMutableRendererPrefs()->accept_languages =
      browser_context->options()->accept_language();
  web_contents_->SetDelegate(web_contents_delegate_.get());
  render_process_host_->AddObserver(this);
  agent_host_->AddObserver(this);
}

HeadlessWebContentsImpl::~HeadlessWebContentsImpl() {
  agent_host_->RemoveObserver(this);
  if (render_process_host_)
    render_process_host_->RemoveObserver(this);
  if (begin_frame_control_enabled_) {
    ui::Compositor* compositor = browser()->PlatformGetCompositor(this);
    DCHECK(compositor);
    compositor->SetExternalBeginFrameClient(nullptr);
  }
}

void HeadlessWebContentsImpl::CreateTabSocketMojoService(
    mojo::ScopedMessagePipeHandle handle) {
  headless_tab_socket_->CreateMojoService(TabSocketRequest(std::move(handle)));
}

void HeadlessWebContentsImpl::CreateMojoService(
    const MojoService::ServiceFactoryCallback& service_factory,
    mojo::ScopedMessagePipeHandle handle) {
  service_factory.Run(this, std::move(handle));
}

void HeadlessWebContentsImpl::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  for (const MojoService& service : mojo_services_) {
    registry_.AddInterface(
        service.service_name,
        base::Bind(&HeadlessWebContentsImpl::CreateMojoService,
                   base::Unretained(this), service.service_factory),
        browser()->BrowserMainThread());
  }

  browser_context_->SetFrameTreeNodeId(render_frame_host->GetProcess()->GetID(),
                                       render_frame_host->GetRoutingID(),
                                       render_frame_host->GetFrameTreeNodeId());
  if (headless_tab_socket_)
    headless_tab_socket_->RenderFrameCreated(render_frame_host);
}

void HeadlessWebContentsImpl::OnInterfaceRequestFromFrame(
    content::RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  registry_.TryBindInterface(interface_name, interface_pipe);
}

void HeadlessWebContentsImpl::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (headless_tab_socket_)
    headless_tab_socket_->RenderFrameDeleted(render_frame_host);
  browser_context_->RemoveFrameTreeNode(
      render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRoutingID());
}

void HeadlessWebContentsImpl::RenderViewReady() {
  DCHECK(web_contents()->GetMainFrame()->IsRenderFrameLive());

  for (auto& observer : observers_)
    observer.DevToolsTargetReady();
}

int HeadlessWebContentsImpl::GetMainFrameRenderProcessId() const {
  return web_contents()->GetMainFrame()->GetProcess()->GetID();
}

int HeadlessWebContentsImpl::GetMainFrameTreeNodeId() const {
  return web_contents()->GetMainFrame()->GetFrameTreeNodeId();
}

bool HeadlessWebContentsImpl::OpenURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents_->GetController().LoadURLWithParams(params);
  web_contents_delegate_->ActivateContents(web_contents_.get());
  web_contents_->Focus();
  return true;
}

void HeadlessWebContentsImpl::Close() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  browser_context()->DestroyWebContents(this);
}

std::string HeadlessWebContentsImpl::GetDevToolsAgentHostId() {
  return agent_host_->GetId();
}

void HeadlessWebContentsImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void HeadlessWebContentsImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void HeadlessWebContentsImpl::DevToolsAgentHostAttached(
    content::DevToolsAgentHost* agent_host) {
  for (auto& observer : observers_)
    observer.DevToolsClientAttached();
}

void HeadlessWebContentsImpl::DevToolsAgentHostDetached(
    content::DevToolsAgentHost* agent_host) {
  for (auto& observer : observers_)
    observer.DevToolsClientDetached();
}

void HeadlessWebContentsImpl::RenderProcessExited(
    content::RenderProcessHost* host,
    base::TerminationStatus status,
    int exit_code) {
  DCHECK_EQ(render_process_host_, host);
  for (auto& observer : observers_)
    observer.RenderProcessExited(status, exit_code);
}

void HeadlessWebContentsImpl::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  DCHECK_EQ(render_process_host_, host);
  render_process_host_ = nullptr;
}

HeadlessDevToolsTarget* HeadlessWebContentsImpl::GetDevToolsTarget() {
  return web_contents()->GetMainFrame()->IsRenderFrameLive() ? this : nullptr;
}

bool HeadlessWebContentsImpl::AttachClient(HeadlessDevToolsClient* client) {
  return HeadlessDevToolsClientImpl::From(client)->AttachToHost(
      agent_host_.get());
}

void HeadlessWebContentsImpl::ForceAttachClient(
    HeadlessDevToolsClient* client) {
  HeadlessDevToolsClientImpl::From(client)->ForceAttachToHost(
      agent_host_.get());
}

void HeadlessWebContentsImpl::DetachClient(HeadlessDevToolsClient* client) {
  DCHECK(agent_host_);
  HeadlessDevToolsClientImpl::From(client)->DetachFromHost(agent_host_.get());
}

bool HeadlessWebContentsImpl::IsAttached() {
  DCHECK(agent_host_);
  return agent_host_->IsAttached();
}

content::WebContents* HeadlessWebContentsImpl::web_contents() const {
  return web_contents_.get();
}

HeadlessBrowserImpl* HeadlessWebContentsImpl::browser() const {
  return browser_context_->browser();
}

HeadlessBrowserContextImpl* HeadlessWebContentsImpl::browser_context() const {
  return browser_context_;
}

HeadlessTabSocket* HeadlessWebContentsImpl::GetHeadlessTabSocket() const {
  return headless_tab_socket_.get();
}

void HeadlessWebContentsImpl::OnDisplayDidFinishFrame(
    const viz::BeginFrameAck& ack) {
  TRACE_EVENT2("headless", "HeadlessWebContentsImpl::OnDisplayDidFinishFrame",
               "source_id", ack.source_id, "sequence_number",
               ack.sequence_number);

  auto it = pending_frames_.begin();
  while (it != pending_frames_.end()) {
    if (begin_frame_source_id_ == ack.source_id &&
        (*it)->sequence_number <= ack.sequence_number) {
      (*it)->has_damage = ack.has_damage;
      (*it)->display_did_finish_frame = true;
      if ((*it)->MaybeRunCallback()) {
        it = pending_frames_.erase(it);
      } else {
        ++it;
      }
    } else {
      ++it;
    }
  }
}

void HeadlessWebContentsImpl::OnNeedsExternalBeginFrames(
    bool needs_begin_frames) {
  TRACE_EVENT1("headless",
               "HeadlessWebContentsImpl::OnNeedsExternalBeginFrames",
               "needs_begin_frames", needs_begin_frames);

  needs_external_begin_frames_ = needs_begin_frames;
  for (int session_id : begin_frame_events_enabled_sessions_)
    SendNeedsBeginFramesEvent(session_id);
}

void HeadlessWebContentsImpl::SetBeginFrameEventsEnabled(int session_id,
                                                         bool enabled) {
  TRACE_EVENT2("headless",
               "HeadlessWebContentsImpl::SetBeginFrameEventsEnabled",
               "session_id", session_id, "enabled", enabled);

  if (enabled) {
    if (!base::ContainsValue(begin_frame_events_enabled_sessions_,
                             session_id)) {
      begin_frame_events_enabled_sessions_.push_back(session_id);

      // We only need to send an event if BeginFrames are needed, as clients
      // assume that they are not needed by default.
      if (needs_external_begin_frames_)
        SendNeedsBeginFramesEvent(session_id);
    }
  } else {
    begin_frame_events_enabled_sessions_.remove(session_id);
  }
}

void HeadlessWebContentsImpl::SendNeedsBeginFramesEvent(int session_id) {
  TRACE_EVENT2("headless", "HeadlessWebContentsImpl::SendNeedsBeginFramesEvent",
               "session_id", session_id, "needs_begin_frames",
               needs_external_begin_frames_);
  DCHECK(agent_host_);
  auto params = base::MakeUnique<base::DictionaryValue>();
  params->SetBoolean("needsBeginFrames", needs_external_begin_frames_);

  base::DictionaryValue event;
  event.SetString("method", "HeadlessExperimental.needsBeginFramesChanged");
  event.Set("params", std::move(params));

  std::string json_result;
  CHECK(base::JSONWriter::Write(event, &json_result));
  agent_host_->SendProtocolMessageToClient(session_id, json_result);
}

void HeadlessWebContentsImpl::DidReceiveCompositorFrame() {
  TRACE_EVENT0("headless",
               "HeadlessWebContentsImpl::DidReceiveCompositorFrame");
  DCHECK(agent_host_);

  if (!first_compositor_frame_received_) {
    first_compositor_frame_received_ = true;

    // Send an event to the devtools clients.
    base::DictionaryValue event;
    event.SetString("method",
                    "HeadlessExperimental.mainFrameReadyForScreenshots");
    event.Set("params", base::MakeUnique<base::DictionaryValue>());

    std::string json_result;
    CHECK(base::JSONWriter::Write(event, &json_result));
    for (int session_id : begin_frame_events_enabled_sessions_)
      agent_host_->SendProtocolMessageToClient(session_id, json_result);
  }
}

void HeadlessWebContentsImpl::PendingFrameReadbackComplete(
    HeadlessWebContentsImpl::PendingFrame* pending_frame,
    const SkBitmap& bitmap,
    content::ReadbackResponse response) {
  TRACE_EVENT2(
      "headless", "HeadlessWebContentsImpl::PendingFrameReadbackComplete",
      "sequence_number", pending_frame->sequence_number, "response", response);
  if (response == content::READBACK_SUCCESS) {
    pending_frame->bitmap = base::MakeUnique<SkBitmap>(bitmap);
  } else {
    LOG(WARNING) << "Readback from surface failed with response " << response;
  }

  pending_frame->wait_for_copy_result = false;

  // Run callback if the frame was already finished by the display.
  if (pending_frame->MaybeRunCallback()) {
    base::EraseIf(pending_frames_,
                  [pending_frame](const std::unique_ptr<PendingFrame>& frame) {
                    return frame.get() == pending_frame;
                  });
  }
}

void HeadlessWebContentsImpl::BeginFrame(
    const base::TimeTicks& frame_timeticks,
    const base::TimeTicks& deadline,
    const base::TimeDelta& interval,
    bool capture_screenshot,
    const FrameFinishedCallback& frame_finished_callback) {
  DCHECK(begin_frame_control_enabled_);
  TRACE_EVENT2("headless", "HeadlessWebContentsImpl::BeginFrame", "frame_time",
               frame_timeticks, "capture_screenshot", capture_screenshot);

  uint64_t sequence_number = begin_frame_sequence_number_++;

  auto pending_frame = base::MakeUnique<PendingFrame>();
  pending_frame->sequence_number = sequence_number;
  pending_frame->callback = frame_finished_callback;

  if (capture_screenshot) {
    pending_frame->wait_for_copy_result = true;
    content::RenderWidgetHostView* view =
        web_contents()->GetRenderWidgetHostView();
    if (view) {
      view->CopyFromSurface(
          gfx::Rect(), gfx::Size(),
          base::Bind(&HeadlessWebContentsImpl::PendingFrameReadbackComplete,
                     base::Unretained(this),
                     base::Unretained(pending_frame.get())),
          kN32_SkColorType);
    }
  }

  pending_frames_.push_back(std::move(pending_frame));

  ui::Compositor* compositor = browser()->PlatformGetCompositor(this);
  DCHECK(compositor);

  compositor->IssueExternalBeginFrame(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, begin_frame_source_id_, sequence_number,
      frame_timeticks, deadline, interval, viz::BeginFrameArgs::NORMAL));
}

HeadlessWebContents::Builder::Builder(
    HeadlessBrowserContextImpl* browser_context)
    : browser_context_(browser_context),
      window_size_(browser_context->options()->window_size()) {}

HeadlessWebContents::Builder::~Builder() = default;

HeadlessWebContents::Builder::Builder(Builder&&) = default;

HeadlessWebContents::Builder& HeadlessWebContents::Builder::SetInitialURL(
    const GURL& initial_url) {
  initial_url_ = initial_url;
  return *this;
}

HeadlessWebContents::Builder& HeadlessWebContents::Builder::SetWindowSize(
    const gfx::Size& size) {
  window_size_ = size;
  return *this;
}

HeadlessWebContents::Builder& HeadlessWebContents::Builder::SetAllowTabSockets(
    bool tab_sockets_allowed) {
  tab_sockets_allowed_ = tab_sockets_allowed;
  return *this;
}

HeadlessWebContents::Builder&
HeadlessWebContents::Builder::SetEnableBeginFrameControl(
    bool enable_begin_frame_control) {
  enable_begin_frame_control_ = enable_begin_frame_control;
  return *this;
}

HeadlessWebContents* HeadlessWebContents::Builder::Build() {
  return browser_context_->CreateWebContents(this);
}

HeadlessWebContents::Builder::MojoService::MojoService() {}

HeadlessWebContents::Builder::MojoService::MojoService(
    const MojoService& other) = default;

HeadlessWebContents::Builder::MojoService::MojoService(
    const std::string& service_name,
    const ServiceFactoryCallback& service_factory)
    : service_name(service_name), service_factory(service_factory) {}

HeadlessWebContents::Builder::MojoService::~MojoService() {}

}  // namespace headless
