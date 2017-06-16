// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/render_frame_devtools_agent_host.h"

#include <tuple>
#include <utility>

#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/devtools_frame_trace_recorder.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/page_navigation_throttle.h"
#include "content/browser/devtools/protocol/dom_handler.h"
#include "content/browser/devtools/protocol/emulation_handler.h"
#include "content/browser/devtools/protocol/input_handler.h"
#include "content/browser/devtools/protocol/inspector_handler.h"
#include "content/browser/devtools/protocol/io_handler.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/page_handler.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/browser/devtools/protocol/schema_handler.h"
#include "content/browser/devtools/protocol/security_handler.h"
#include "content/browser/devtools/protocol/service_worker_handler.h"
#include "content/browser/devtools/protocol/storage_handler.h"
#include "content/browser/devtools/protocol/target_handler.h"
#include "content/browser/devtools/protocol/tracing_handler.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/browser_side_navigation_policy.h"

#if defined(OS_ANDROID)
#include "content/public/browser/render_widget_host_view.h"
#include "device/wake_lock/public/interfaces/wake_lock_context.mojom.h"
#endif

namespace content {

typedef std::vector<RenderFrameDevToolsAgentHost*> Instances;

namespace {
base::LazyInstance<Instances>::Leaky g_instances = LAZY_INSTANCE_INITIALIZER;

RenderFrameDevToolsAgentHost* FindAgentHost(FrameTreeNode* frame_tree_node) {
  if (g_instances == NULL)
    return NULL;
  for (Instances::iterator it = g_instances.Get().begin();
       it != g_instances.Get().end(); ++it) {
    if ((*it)->frame_tree_node() == frame_tree_node)
      return *it;
  }
  return NULL;
}

bool ShouldCreateDevToolsForHost(RenderFrameHost* rfh) {
  return rfh->IsCrossProcessSubframe() || !rfh->GetParent();
}

bool ShouldCreateDevToolsForNode(FrameTreeNode* ftn) {
  return ShouldCreateDevToolsForHost(ftn->current_frame_host());
}

FrameTreeNode* GetFrameTreeNodeAncestor(FrameTreeNode* frame_tree_node) {
  while (frame_tree_node && !ShouldCreateDevToolsForNode(frame_tree_node))
    frame_tree_node = frame_tree_node->parent();
  DCHECK(frame_tree_node);
  return frame_tree_node;
}

}  // namespace

// RenderFrameDevToolsAgentHost::FrameHostHolder -------------------------------

class RenderFrameDevToolsAgentHost::FrameHostHolder {
 public:
  FrameHostHolder(
      RenderFrameDevToolsAgentHost* agent, RenderFrameHostImpl* host);
  ~FrameHostHolder();

  RenderFrameHostImpl* host() const { return host_; }

  void Attach(DevToolsSession* session);
  void Reattach(FrameHostHolder* old);
  void Detach(int session_id);
  void DispatchProtocolMessage(int session_id,
                               int call_id,
                               const std::string& method,
                               const std::string& message);
  void InspectElement(int session_id, int x, int y);
  bool ProcessChunkedMessageFromAgent(const DevToolsMessageChunk& chunk);
  void Suspend();
  void Resume();
  std::string StateCookie() const { return chunk_processor_.state_cookie(); }
  void ReattachWithCookie(std::string cookie);

 private:
  void SendMessageToClient(int session_id, const std::string& message);

  RenderFrameDevToolsAgentHost* agent_;
  RenderFrameHostImpl* host_;
  bool attached_;
  bool suspended_;
  DevToolsMessageChunkProcessor chunk_processor_;
  // <session_id, message>
  std::vector<std::pair<int, std::string>> pending_messages_;
  // <call_id> -> PendingMessage
  std::map<int, Message> sent_messages_;
  // These are sent messages for which we got a reply while suspended.
  std::map<int, Message> sent_messages_whose_reply_came_while_suspended_;
};

RenderFrameDevToolsAgentHost::FrameHostHolder::FrameHostHolder(
    RenderFrameDevToolsAgentHost* agent, RenderFrameHostImpl* host)
    : agent_(agent),
      host_(host),
      attached_(false),
      suspended_(false),
      chunk_processor_(base::Bind(
           &RenderFrameDevToolsAgentHost::FrameHostHolder::SendMessageToClient,
           base::Unretained(this))) {
  DCHECK(agent_);
  DCHECK(host_);
}

RenderFrameDevToolsAgentHost::FrameHostHolder::~FrameHostHolder() {
  if (attached_)
    agent_->RevokePolicy(host_);
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::Attach(
    DevToolsSession* session) {
  host_->Send(new DevToolsAgentMsg_Attach(
      host_->GetRoutingID(), agent_->GetId(), session->session_id()));
  agent_->GrantPolicy(host_);
  attached_ = true;
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::Reattach(
    FrameHostHolder* old) {
  std::string cookie = old ? old->chunk_processor_.state_cookie() : "";
  ReattachWithCookie(std::move(cookie));
  if (!old)
    return;
  if (IsBrowserSideNavigationEnabled()) {
    for (const auto& pair :
         old->sent_messages_whose_reply_came_while_suspended_) {
      DispatchProtocolMessage(pair.second.session_id, pair.first,
                              pair.second.method, pair.second.message);
    }
  }
  for (const auto& pair : old->sent_messages_) {
    DispatchProtocolMessage(pair.second.session_id, pair.first,
                            pair.second.method, pair.second.message);
  }
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::ReattachWithCookie(
    std::string cookie) {
  chunk_processor_.set_state_cookie(cookie);
  host_->Send(
      new DevToolsAgentMsg_Reattach(host_->GetRoutingID(), agent_->GetId(),
                                    agent_->session()->session_id(), cookie));
  agent_->GrantPolicy(host_);
  attached_ = true;
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::Detach(int session_id) {
  host_->Send(new DevToolsAgentMsg_Detach(host_->GetRoutingID(), session_id));
  agent_->RevokePolicy(host_);
  attached_ = false;
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::DispatchProtocolMessage(
    int session_id,
    int call_id,
    const std::string& method,
    const std::string& message) {
  host_->Send(new DevToolsAgentMsg_DispatchOnInspectorBackend(
      host_->GetRoutingID(), session_id, call_id, method, message));
  sent_messages_[call_id] = {session_id, call_id, method, message};
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::InspectElement(
    int session_id, int x, int y) {
  DCHECK(attached_);
  host_->Send(new DevToolsAgentMsg_InspectElement(
      host_->GetRoutingID(), session_id, x, y));
}

bool
RenderFrameDevToolsAgentHost::FrameHostHolder::ProcessChunkedMessageFromAgent(
    const DevToolsMessageChunk& chunk) {
  return chunk_processor_.ProcessChunkedMessageFromAgent(chunk);
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::SendMessageToClient(
    int session_id,
    const std::string& message) {
  int id = chunk_processor_.last_call_id();
  Message sent_message = sent_messages_[id];
  sent_messages_.erase(id);
  if (suspended_) {
    sent_messages_whose_reply_came_while_suspended_[id] = sent_message;
    pending_messages_.push_back(std::make_pair(session_id, message));
  } else {
    agent_->SendMessageToClient(session_id, message);
    // |this| may be deleted at this point.
  }
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::Suspend() {
  suspended_ = true;
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::Resume() {
  suspended_ = false;
  for (const auto& pair : pending_messages_)
    agent_->SendMessageToClient(pair.first, pair.second);
  std::vector<std::pair<int, std::string>> empty;
  pending_messages_.swap(empty);
  sent_messages_whose_reply_came_while_suspended_.clear();
}

// RenderFrameDevToolsAgentHost ------------------------------------------------

// static
scoped_refptr<DevToolsAgentHost>
DevToolsAgentHost::GetOrCreateFor(WebContents* web_contents) {
  FrameTreeNode* node =
      static_cast<WebContentsImpl*>(web_contents)->GetFrameTree()->root();
  // TODO(dgozman): this check should not be necessary. See
  // http://crbug.com/489664.
  if (!node)
    return nullptr;
  return RenderFrameDevToolsAgentHost::GetOrCreateFor(node);
}

// static
scoped_refptr<DevToolsAgentHost> RenderFrameDevToolsAgentHost::GetOrCreateFor(
    FrameTreeNode* frame_tree_node) {
  frame_tree_node = GetFrameTreeNodeAncestor(frame_tree_node);
  RenderFrameDevToolsAgentHost* result = FindAgentHost(frame_tree_node);
  if (!result)
    result = new RenderFrameDevToolsAgentHost(frame_tree_node);
  return result;
}

// static
bool DevToolsAgentHost::HasFor(WebContents* web_contents) {
  FrameTreeNode* node =
      static_cast<WebContentsImpl*>(web_contents)->GetFrameTree()->root();
  return node ? FindAgentHost(node) != nullptr : false;
}

// static
bool DevToolsAgentHost::IsDebuggerAttached(WebContents* web_contents) {
  FrameTreeNode* node =
      static_cast<WebContentsImpl*>(web_contents)->GetFrameTree()->root();
  RenderFrameDevToolsAgentHost* host = node ? FindAgentHost(node) : nullptr;
  return host && host->IsAttached();
}

// static
void RenderFrameDevToolsAgentHost::AddAllAgentHosts(
    DevToolsAgentHost::List* result) {
  for (WebContentsImpl* wc : WebContentsImpl::GetAllWebContents()) {
    for (FrameTreeNode* node : wc->GetFrameTree()->Nodes()) {
      if (!node->current_frame_host() || !ShouldCreateDevToolsForNode(node))
        continue;
      if (!node->current_frame_host()->IsRenderFrameLive())
        continue;
      result->push_back(RenderFrameDevToolsAgentHost::GetOrCreateFor(node));
    }
  }
}

// static
void RenderFrameDevToolsAgentHost::OnCancelPendingNavigation(
    RenderFrameHost* pending,
    RenderFrameHost* current) {
  if (IsBrowserSideNavigationEnabled())
    return;

  RenderFrameDevToolsAgentHost* agent_host = FindAgentHost(
      static_cast<RenderFrameHostImpl*>(pending)->frame_tree_node());
  if (!agent_host)
    return;
  if (agent_host->pending_ && agent_host->pending_->host() == pending) {
    DCHECK(agent_host->current_ && agent_host->current_->host() == current);
    agent_host->DiscardPending();
    DCHECK(agent_host->CheckConsistency());
  }
}

// static
void RenderFrameDevToolsAgentHost::OnBeforeNavigation(
    RenderFrameHost* current, RenderFrameHost* pending) {
  RenderFrameDevToolsAgentHost* agent_host = FindAgentHost(
      static_cast<RenderFrameHostImpl*>(current)->frame_tree_node());
  if (agent_host)
    agent_host->AboutToNavigateRenderFrame(current, pending);
}

// static
void RenderFrameDevToolsAgentHost::OnFailedNavigation(
    RenderFrameHost* host,
    const CommonNavigationParams& common_params,
    const BeginNavigationParams& begin_params,
    net::Error error_code) {
  RenderFrameDevToolsAgentHost* agent_host =
      FindAgentHost(static_cast<RenderFrameHostImpl*>(host)->frame_tree_node());
  if (!agent_host)
    return;
  for (auto* network : protocol::NetworkHandler::ForAgentHost(agent_host))
    network->NavigationFailed(common_params, begin_params, error_code);
}

// static
std::unique_ptr<NavigationThrottle>
RenderFrameDevToolsAgentHost::CreateThrottleForNavigation(
    NavigationHandle* navigation_handle) {
  FrameTreeNode* frame_tree_node =
      static_cast<NavigationHandleImpl*>(navigation_handle)->frame_tree_node();
  while (frame_tree_node && frame_tree_node->parent()) {
    frame_tree_node = frame_tree_node->parent();
  }
  RenderFrameDevToolsAgentHost* agent_host = FindAgentHost(frame_tree_node);
  // Note Page.setControlNavigations is intended to control navigations in the
  // main frame and all child frames and |page_handler_| only exists for the
  // main frame.
  if (!agent_host)
    return nullptr;
  for (auto* page_handler : protocol::PageHandler::ForAgentHost(agent_host)) {
    std::unique_ptr<NavigationThrottle> throttle =
        page_handler->CreateThrottleForNavigation(navigation_handle);
    if (throttle)
      return throttle;
  }
  return nullptr;
}

// static
bool RenderFrameDevToolsAgentHost::IsNetworkHandlerEnabled(
    FrameTreeNode* frame_tree_node) {
  frame_tree_node = GetFrameTreeNodeAncestor(frame_tree_node);
  RenderFrameDevToolsAgentHost* agent_host = FindAgentHost(frame_tree_node);
  if (!agent_host)
    return false;
  for (auto* network : protocol::NetworkHandler::ForAgentHost(agent_host)) {
    if (network->enabled())
      return true;
  }
  return false;
}

// static
std::string RenderFrameDevToolsAgentHost::UserAgentOverride(
    FrameTreeNode* frame_tree_node) {
  frame_tree_node = GetFrameTreeNodeAncestor(frame_tree_node);
  RenderFrameDevToolsAgentHost* agent_host = FindAgentHost(frame_tree_node);
  if (!agent_host)
    return std::string();
  for (auto* network : protocol::NetworkHandler::ForAgentHost(agent_host)) {
    std::string override = network->UserAgentOverride();
    if (!override.empty())
      return override;
  }
  return std::string();
}

// static
void RenderFrameDevToolsAgentHost::WebContentsCreated(
    WebContents* web_contents) {
  if (ShouldForceCreation()) {
    // Force agent host.
    DevToolsAgentHost::GetOrCreateFor(web_contents);
  }
}

RenderFrameDevToolsAgentHost::RenderFrameDevToolsAgentHost(
    FrameTreeNode* frame_tree_node)
    : DevToolsAgentHostImpl(base::GenerateGUID()),
      frame_trace_recorder_(nullptr),
      handlers_frame_host_(nullptr),
      current_frame_crashed_(false),
      frame_tree_node_(frame_tree_node) {
  if (IsBrowserSideNavigationEnabled()) {
    frame_host_ = frame_tree_node->current_frame_host();
    render_frame_alive_ = frame_host_ && frame_host_->IsRenderFrameLive();
  } else {
    if (frame_tree_node->current_frame_host()) {
      SetPending(frame_tree_node->current_frame_host());
      CommitPending();
    }
  }
  WebContentsObserver::Observe(
      WebContentsImpl::FromFrameTreeNode(frame_tree_node));

  if (web_contents() && web_contents()->GetCrashedStatus() !=
      base::TERMINATION_STATUS_STILL_RUNNING) {
    current_frame_crashed_ = true;
  }

  g_instances.Get().push_back(this);
  AddRef();  // Balanced in RenderFrameHostDestroyed.

  NotifyCreated();
}

void RenderFrameDevToolsAgentHost::SetPending(RenderFrameHostImpl* host) {
  DCHECK(!IsBrowserSideNavigationEnabled());
  DCHECK(!pending_);
  current_frame_crashed_ = false;
  pending_.reset(new FrameHostHolder(this, host));
  if (IsAttached())
    pending_->Reattach(current_.get());

  if (current_)
    current_->Suspend();
  pending_->Suspend();

  UpdateProtocolHandlers(host);
}

void RenderFrameDevToolsAgentHost::CommitPending() {
  DCHECK(!IsBrowserSideNavigationEnabled());
  DCHECK(pending_);
  current_frame_crashed_ = false;

  if (!ShouldCreateDevToolsForHost(pending_->host())) {
    DestroyOnRenderFrameGone();
    // |this| may be deleted at this point.
    return;
  }

  current_ = std::move(pending_);
  UpdateProtocolHandlers(current_->host());
  current_->Resume();
}

void RenderFrameDevToolsAgentHost::DiscardPending() {
  DCHECK(!IsBrowserSideNavigationEnabled());
  DCHECK(pending_);
  DCHECK(current_);
  pending_.reset();
  UpdateProtocolHandlers(current_->host());
  current_->Resume();
}

BrowserContext* RenderFrameDevToolsAgentHost::GetBrowserContext() {
  WebContents* contents = web_contents();
  return contents ? contents->GetBrowserContext() : nullptr;
}

WebContents* RenderFrameDevToolsAgentHost::GetWebContents() {
  return web_contents();
}

void RenderFrameDevToolsAgentHost::AttachSession(DevToolsSession* session) {
  session->SetFallThroughForNotFound(true);
  if (IsBrowserSideNavigationEnabled())
    session->SetRenderFrameHost(frame_host_);
  else
    session->SetRenderFrameHost(handlers_frame_host_);
  if (frame_tree_node_ && !frame_tree_node_->parent()) {
    session->AddHandler(base::WrapUnique(new protocol::PageHandler()));
    session->AddHandler(base::WrapUnique(new protocol::SecurityHandler()));
  }
  session->AddHandler(base::WrapUnique(new protocol::DOMHandler()));
  session->AddHandler(base::WrapUnique(new protocol::EmulationHandler()));
  session->AddHandler(base::WrapUnique(new protocol::InputHandler()));
  session->AddHandler(base::WrapUnique(new protocol::InspectorHandler()));
  session->AddHandler(base::WrapUnique(new protocol::IOHandler(
      GetIOContext())));
  session->AddHandler(base::WrapUnique(new protocol::NetworkHandler()));
  session->AddHandler(base::WrapUnique(new protocol::SchemaHandler()));
  session->AddHandler(base::WrapUnique(new protocol::ServiceWorkerHandler()));
  session->AddHandler(base::WrapUnique(new protocol::StorageHandler()));
  session->AddHandler(base::WrapUnique(new protocol::TargetHandler()));
  session->AddHandler(base::WrapUnique(new protocol::TracingHandler(
      protocol::TracingHandler::Renderer,
      frame_tree_node_ ? frame_tree_node_->frame_tree_node_id() : 0,
      GetIOContext())));

  if (IsBrowserSideNavigationEnabled()) {
    if (frame_host_) {
      frame_host_->Send(new DevToolsAgentMsg_Attach(
          frame_host_->GetRoutingID(), GetId(), session->session_id()));
    }
  } else {
    if (current_)
      current_->Attach(session);
    if (pending_)
      pending_->Attach(session);
  }
  OnClientAttached();
}

void RenderFrameDevToolsAgentHost::DetachSession(int session_id) {
  if (IsBrowserSideNavigationEnabled()) {
    if (frame_host_) {
      frame_host_->Send(
          new DevToolsAgentMsg_Detach(frame_host_->GetRoutingID(), session_id));
    }
  } else {
    if (current_)
      current_->Detach(session_id);
    if (pending_)
      pending_->Detach(session_id);
  }
  OnClientDetached();
}

bool RenderFrameDevToolsAgentHost::DispatchProtocolMessage(
    DevToolsSession* session,
    const std::string& message) {
  int call_id = 0;
  std::string method;
  int session_id = session->session_id();
  if (session->Dispatch(message, &call_id, &method) !=
      protocol::Response::kFallThrough) {
    return true;
  }

  if (IsBrowserSideNavigationEnabled()) {
    if (!navigation_handles_.empty()) {
      suspended_messages_.push_back({session_id, call_id, method, message});
      return true;
    }
    if (frame_host_) {
      frame_host_->Send(new DevToolsAgentMsg_DispatchOnInspectorBackend(
          frame_host_->GetRoutingID(), session_id, call_id, method, message));
    }
    session->waiting_messages()[call_id] = {method, message};
  } else {
    if (current_)
      current_->DispatchProtocolMessage(session_id, call_id, method, message);
    if (pending_)
      pending_->DispatchProtocolMessage(session_id, call_id, method, message);
  }
  return true;
}

void RenderFrameDevToolsAgentHost::InspectElement(
    DevToolsSession* session,
    int x,
    int y) {
  if (IsBrowserSideNavigationEnabled()) {
    if (frame_host_) {
      frame_host_->Send(new DevToolsAgentMsg_InspectElement(
          frame_host_->GetRoutingID(), session->session_id(), x, y));
    }
  } else {
    if (current_)
      current_->InspectElement(session->session_id(), x, y);
    if (pending_)
      pending_->InspectElement(session->session_id(), x, y);
  }
}

void RenderFrameDevToolsAgentHost::OnClientAttached() {
  if (!web_contents())
    return;

  frame_trace_recorder_.reset(new DevToolsFrameTraceRecorder());
#if defined(OS_ANDROID)
  GetWakeLock()->RequestWakeLock();
#endif
  if (IsBrowserSideNavigationEnabled())
    GrantPolicy(frame_host_);
}

void RenderFrameDevToolsAgentHost::OnClientDetached() {
#if defined(OS_ANDROID)
  GetWakeLock()->CancelWakeLock();
#endif
  frame_trace_recorder_.reset();
  suspended_messages_.clear();
  if (IsBrowserSideNavigationEnabled())
    RevokePolicy(frame_host_);
}

RenderFrameDevToolsAgentHost::~RenderFrameDevToolsAgentHost() {
  Instances::iterator it = std::find(g_instances.Get().begin(),
                                     g_instances.Get().end(),
                                     this);
  if (it != g_instances.Get().end())
    g_instances.Get().erase(it);
}

void RenderFrameDevToolsAgentHost::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  if (!IsBrowserSideNavigationEnabled())
    return;
  NavigationHandleImpl* handle =
      static_cast<NavigationHandleImpl*>(navigation_handle);
  if (handle->frame_tree_node() != frame_tree_node_)
    return;

  // UpdateFrameHost may destruct |this|.
  scoped_refptr<RenderFrameDevToolsAgentHost> protect(this);
  UpdateFrameHost(handle->GetRenderFrameHost());
  DCHECK(CheckConsistency());
}

void RenderFrameDevToolsAgentHost::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!IsBrowserSideNavigationEnabled()) {
    // CommitPending may destruct |this|.
    scoped_refptr<RenderFrameDevToolsAgentHost> protect(this);
    if (navigation_handle->HasCommitted() &&
        !navigation_handle->IsErrorPage()) {
      if (pending_ &&
          pending_->host() == navigation_handle->GetRenderFrameHost()) {
        CommitPending();
      }
      for (auto* target : protocol::TargetHandler::ForAgentHost(this))
        target->UpdateServiceWorkers();
    } else if (pending_ && pending_->host()->GetFrameTreeNodeId() ==
                               navigation_handle->GetFrameTreeNodeId()) {
      DiscardPending();
    }
    DCHECK(CheckConsistency());
    return;
  }

  NavigationHandleImpl* handle =
      static_cast<NavigationHandleImpl*>(navigation_handle);
  if (handle->frame_tree_node() != frame_tree_node_)
    return;
  navigation_handles_.erase(handle);

  // UpdateFrameHost may destruct |this|.
  scoped_refptr<RenderFrameDevToolsAgentHost> protect(this);
  if (handle->HasCommitted() && !handle->IsErrorPage())
    UpdateFrameHost(handle->GetRenderFrameHost());
  DCHECK(CheckConsistency());
  if (navigation_handles_.empty()) {
    if (session()) {
      for (const Message& message : suspended_messages_) {
        if (frame_host_) {
          frame_host_->Send(new DevToolsAgentMsg_DispatchOnInspectorBackend(
              frame_host_->GetRoutingID(), message.session_id, message.call_id,
              message.method, message.message));
        }
        session()->waiting_messages()[message.call_id] = {message.method,
                                                          message.message};
      }
    }
    suspended_messages_.clear();
  }
  if (handle->HasCommitted()) {
    for (auto* target : protocol::TargetHandler::ForAgentHost(this))
      target->UpdateServiceWorkers();
  }
}

void RenderFrameDevToolsAgentHost::UpdateFrameHost(
    RenderFrameHostImpl* frame_host) {
  if (frame_host == frame_host_) {
    if (frame_host && !render_frame_alive_) {
      render_frame_alive_ = true;
      MaybeReattachToRenderFrame();
    }
    return;
  }

  if (session())
    RevokePolicy(frame_host_);

  if (frame_host && !ShouldCreateDevToolsForHost(frame_host)) {
    DestroyOnRenderFrameGone();
    // |this| may be deleted at this point.
    return;
  }

  frame_host_ = frame_host;
  render_frame_alive_ = true;
  if (session()) {
    GrantPolicy(frame_host_);
    session()->SetRenderFrameHost(frame_host);
    MaybeReattachToRenderFrame();
  }
}

void RenderFrameDevToolsAgentHost::MaybeReattachToRenderFrame() {
  DCHECK(IsBrowserSideNavigationEnabled());
  if (!session() || !frame_host_)
    return;
  int session_id = session()->session_id();
  frame_host_->Send(new DevToolsAgentMsg_Reattach(frame_host_->GetRoutingID(),
                                                  GetId(), session_id,
                                                  session()->state_cookie()));
  for (const auto& pair : session()->waiting_messages()) {
    int call_id = pair.first;
    const DevToolsSession::Message& message = pair.second;
    frame_host_->Send(new DevToolsAgentMsg_DispatchOnInspectorBackend(
        frame_host_->GetRoutingID(), session_id, call_id, message.method,
        message.message));
  }
}

void RenderFrameDevToolsAgentHost::GrantPolicy(RenderFrameHostImpl* host) {
  if (!host)
    return;
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadRawCookies(
      host->GetProcess()->GetID());
}

void RenderFrameDevToolsAgentHost::RevokePolicy(RenderFrameHostImpl* host) {
  if (!host)
    return;

  bool process_has_agents = false;
  RenderProcessHost* process_host = host->GetProcess();
  for (RenderFrameDevToolsAgentHost* agent : g_instances.Get()) {
    if (!agent->IsAttached())
      continue;
    if (IsBrowserSideNavigationEnabled()) {
      if (agent->frame_host_ && agent->frame_host_ != host &&
          agent->frame_host_->GetProcess() == process_host) {
        process_has_agents = true;
      }
      continue;
    }
    if (agent->current_ && agent->current_->host() != host &&
        agent->current_->host()->GetProcess() == process_host) {
      process_has_agents = true;
    }
    if (agent->pending_ && agent->pending_->host() != host &&
        agent->pending_->host()->GetProcess() == process_host) {
      process_has_agents = true;
    }
  }

  // We are the last to disconnect from the renderer -> revoke permissions.
  if (!process_has_agents) {
    ChildProcessSecurityPolicyImpl::GetInstance()->RevokeReadRawCookies(
        process_host->GetID());
  }
}

void RenderFrameDevToolsAgentHost::AboutToNavigateRenderFrame(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  if (IsBrowserSideNavigationEnabled())
    return;

  // CommitPending may destruct |this|.
  scoped_refptr<RenderFrameDevToolsAgentHost> protect(this);

  DCHECK(!pending_ || pending_->host() != old_host);
  if (!current_ || current_->host() != old_host) {
    DCHECK(CheckConsistency());
    return;
  }
  if (old_host == new_host && !current_frame_crashed_) {
    DCHECK(CheckConsistency());
    return;
  }
  DCHECK(!pending_);
  SetPending(static_cast<RenderFrameHostImpl*>(new_host));
  // Commit when navigating the same frame after crash, avoiding the same
  // host in current_ and pending_.
  if (old_host == new_host)
    CommitPending();
  DCHECK(CheckConsistency());
}

void RenderFrameDevToolsAgentHost::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  if (!IsBrowserSideNavigationEnabled())
    return;
  NavigationHandleImpl* handle =
      static_cast<NavigationHandleImpl*>(navigation_handle);
  if (handle->frame_tree_node() != frame_tree_node_)
    return;
  navigation_handles_.insert(handle);
  DCHECK(CheckConsistency());
}

void RenderFrameDevToolsAgentHost::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  for (auto* target : protocol::TargetHandler::ForAgentHost(this))
    target->UpdateFrames();

  if (IsBrowserSideNavigationEnabled()) {
    if (old_host != frame_host_)
      return;

    // UpdateFrameHost may destruct |this|.
    scoped_refptr<RenderFrameDevToolsAgentHost> protect(this);
    UpdateFrameHost(nullptr);
    DCHECK(CheckConsistency());
    return;
  }

  // CommitPending may destruct |this|.
  scoped_refptr<RenderFrameDevToolsAgentHost> protect(this);

  DCHECK(!pending_ || pending_->host() != old_host);
  if (!current_ || current_->host() != old_host) {
    DCHECK(CheckConsistency());
    return;
  }

  // AboutToNavigateRenderFrame was not called for renderer-initiated
  // navigation.
  if (!pending_)
    SetPending(static_cast<RenderFrameHostImpl*>(new_host));
  CommitPending();
  DCHECK(CheckConsistency());
}

void RenderFrameDevToolsAgentHost::FrameDeleted(RenderFrameHost* rfh) {
  if (IsBrowserSideNavigationEnabled()) {
    if (static_cast<RenderFrameHostImpl*>(rfh)->frame_tree_node() ==
        frame_tree_node_)
      DestroyOnRenderFrameGone();  // |this| may be deleted at this point.
    else
      DCHECK(CheckConsistency());
    return;
  }

  if (pending_ && pending_->host() == rfh) {
    if (!IsBrowserSideNavigationEnabled())
      DiscardPending();
    DCHECK(CheckConsistency());
    return;
  }

  if (current_ && current_->host() == rfh)
    DestroyOnRenderFrameGone();  // |this| may be deleted at this point.
}

void RenderFrameDevToolsAgentHost::RenderFrameDeleted(RenderFrameHost* rfh) {
  if (IsBrowserSideNavigationEnabled()) {
    if (rfh == frame_host_)
      render_frame_alive_ = false;
    DCHECK(CheckConsistency());
    return;
  }

  if (!current_frame_crashed_)
    FrameDeleted(rfh);
  else
    DCHECK(CheckConsistency());
}

void RenderFrameDevToolsAgentHost::DestroyOnRenderFrameGone() {
  scoped_refptr<RenderFrameDevToolsAgentHost> protect(this);
  UpdateProtocolHandlers(nullptr);
  if (IsAttached())
    OnClientDetached();
  ForceDetach(false);
  frame_host_ = nullptr;
  pending_.reset();
  current_.reset();
  frame_tree_node_ = nullptr;
  WebContentsObserver::Observe(nullptr);
  Release();
}

bool RenderFrameDevToolsAgentHost::CheckConsistency() {
  if (!IsBrowserSideNavigationEnabled()) {
    if (current_ && pending_ && current_->host() == pending_->host())
      return false;
  } else {
    if (current_ || pending_)
      return false;
  }

  if (!frame_tree_node_)
    return !frame_host_;

  RenderFrameHostManager* manager = frame_tree_node_->render_manager();
  RenderFrameHostImpl* current = manager->current_frame_host();
  RenderFrameHostImpl* pending = manager->pending_frame_host();
  RenderFrameHostImpl* speculative = manager->speculative_frame_host();
  RenderFrameHostImpl* host =
      IsBrowserSideNavigationEnabled() ? frame_host_ : handlers_frame_host_;
  return host == current || host == pending || host == speculative;
}

#if defined(OS_ANDROID)
device::mojom::WakeLock* RenderFrameDevToolsAgentHost::GetWakeLock() {
  // Here is a lazy binding, and will not reconnect after connection error.
  if (!wake_lock_) {
    device::mojom::WakeLockRequest request = mojo::MakeRequest(&wake_lock_);
    device::mojom::WakeLockContext* wake_lock_context =
        web_contents()->GetWakeLockContext();
    if (wake_lock_context) {
      wake_lock_context->GetWakeLock(
          device::mojom::WakeLockType::PreventDisplaySleep,
          device::mojom::WakeLockReason::ReasonOther, "DevTools",
          std::move(request));
    }
  }
  return wake_lock_.get();
}
#endif

void RenderFrameDevToolsAgentHost::RenderProcessGone(
    base::TerminationStatus status) {
  switch(status) {
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
#if defined(OS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
#if defined(OS_ANDROID)
    case base::TERMINATION_STATUS_OOM_PROTECTED:
#endif
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      for (auto* inspector : protocol::InspectorHandler::ForAgentHost(this))
        inspector->TargetCrashed();
      current_frame_crashed_ = true;
      break;
    default:
      for (auto* inspector : protocol::InspectorHandler::ForAgentHost(this))
        inspector->TargetDetached("Render process gone.");
      break;
  }
  DCHECK(CheckConsistency());
}

bool RenderFrameDevToolsAgentHost::OnMessageReceived(
    const IPC::Message& message,
    RenderFrameHost* render_frame_host) {
  if (IsBrowserSideNavigationEnabled()) {
    if (render_frame_host != frame_host_)
      return false;
  } else {
    bool is_current = current_ && current_->host() == render_frame_host;
    bool is_pending = pending_ && pending_->host() == render_frame_host;
    if (!is_current && !is_pending)
      return false;
  }
  if (!IsAttached())
    return false;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(RenderFrameDevToolsAgentHost, message,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(DevToolsClientMsg_DispatchOnInspectorFrontend,
                        OnDispatchOnInspectorFrontend)
    IPC_MESSAGE_HANDLER(DevToolsAgentHostMsg_RequestNewWindow,
                        OnRequestNewWindow)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderFrameDevToolsAgentHost::DidAttachInterstitialPage() {
  for (auto* page : protocol::PageHandler::ForAgentHost(this))
    page->DidAttachInterstitialPage();

  if (IsBrowserSideNavigationEnabled())
    return;

  // TODO(dgozman): this may break for cross-process subframes.
  if (!pending_) {
    DCHECK(CheckConsistency());
    return;
  }
  // Pending set in AboutToNavigateRenderFrame turned out to be interstitial.
  // Connect back to the real one.
  DiscardPending();
  DCHECK(CheckConsistency());
}

void RenderFrameDevToolsAgentHost::DidDetachInterstitialPage() {
  for (auto* page : protocol::PageHandler::ForAgentHost(this))
    page->DidDetachInterstitialPage();
}

void RenderFrameDevToolsAgentHost::WasShown() {
#if defined(OS_ANDROID)
  GetWakeLock()->RequestWakeLock();
#endif
}

void RenderFrameDevToolsAgentHost::WasHidden() {
#if defined(OS_ANDROID)
  GetWakeLock()->CancelWakeLock();
#endif
}

void RenderFrameDevToolsAgentHost::DidReceiveCompositorFrame() {
  const cc::CompositorFrameMetadata& metadata =
      RenderWidgetHostImpl::From(
          web_contents()->GetRenderViewHost()->GetWidget())
          ->last_frame_metadata();
  for (auto* page : protocol::PageHandler::ForAgentHost(this))
    page->OnSwapCompositorFrame(metadata.Clone());
  for (auto* input : protocol::InputHandler::ForAgentHost(this))
    input->OnSwapCompositorFrame(metadata);

  if (!frame_trace_recorder_)
    return;
  bool did_initiate_recording = false;
  for (auto* tracing : protocol::TracingHandler::ForAgentHost(this))
    did_initiate_recording |= tracing->did_initiate_recording();
  if (did_initiate_recording) {
    if (IsBrowserSideNavigationEnabled()) {
      frame_trace_recorder_->OnSwapCompositorFrame(frame_host_, metadata);
    } else {
      frame_trace_recorder_->OnSwapCompositorFrame(
          current_ ? current_->host() : nullptr, metadata);
    }
  }
}

void RenderFrameDevToolsAgentHost::UpdateProtocolHandlers(
    RenderFrameHostImpl* host) {
  if (IsBrowserSideNavigationEnabled())
    return;
#if DCHECK_IS_ON()
  // Check that we don't have stale host object here by accessing some random
  // properties inside.
  if (handlers_frame_host_ && handlers_frame_host_->GetRenderWidgetHost())
    handlers_frame_host_->GetRenderWidgetHost()->GetRoutingID();
#endif
  handlers_frame_host_ = host;
  if (session())
    session()->SetRenderFrameHost(host);
}

void RenderFrameDevToolsAgentHost::DisconnectWebContents() {
  if (IsBrowserSideNavigationEnabled()) {
    UpdateFrameHost(nullptr);
    frame_tree_node_ = nullptr;
    navigation_handles_.clear();
    WebContentsObserver::Observe(nullptr);
    return;
  }
  if (pending_)
    DiscardPending();
  UpdateProtocolHandlers(nullptr);
  if (session()) {
    disconnected_cookie_ = current_->StateCookie();
    current_->Detach(session()->session_id());
  }
  current_.reset();
  frame_tree_node_ = nullptr;
  WebContentsObserver::Observe(nullptr);
}

void RenderFrameDevToolsAgentHost::ConnectWebContents(WebContents* wc) {
  if (IsBrowserSideNavigationEnabled()) {
    RenderFrameHostImpl* host =
        static_cast<RenderFrameHostImpl*>(wc->GetMainFrame());
    DCHECK(host);
    frame_tree_node_ = host->frame_tree_node();
    WebContentsObserver::Observe(wc);
    UpdateFrameHost(host);
    return;
  }

  // CommitPending may destruct |this|.
  scoped_refptr<RenderFrameDevToolsAgentHost> protect(this);

  DCHECK(!current_);
  DCHECK(!pending_);
  RenderFrameHostImpl* host =
      static_cast<RenderFrameHostImpl*>(wc->GetMainFrame());
  DCHECK(host);
  current_frame_crashed_ = false;
  current_.reset(new FrameHostHolder(this, host));
  std::string cookie = std::move(disconnected_cookie_);
  if (IsAttached())
    current_->ReattachWithCookie(std::move(cookie));

  UpdateProtocolHandlers(host);
  frame_tree_node_ = host->frame_tree_node();
  WebContentsObserver::Observe(WebContents::FromRenderFrameHost(host));
}

std::string RenderFrameDevToolsAgentHost::GetParentId() {
  if (IsChildFrame()) {
    FrameTreeNode* frame_tree_node =
        GetFrameTreeNodeAncestor(frame_tree_node_->parent());
    return RenderFrameDevToolsAgentHost::GetOrCreateFor(frame_tree_node)
        ->GetId();
  }

  WebContentsImpl* contents = static_cast<WebContentsImpl*>(web_contents());
  if (!contents)
    return "";
  contents = contents->GetOuterWebContents();
  if (contents)
    return DevToolsAgentHost::GetOrCreateFor(contents)->GetId();
  return "";
}

std::string RenderFrameDevToolsAgentHost::GetType() {
  if (web_contents() &&
      static_cast<WebContentsImpl*>(web_contents())->GetOuterWebContents()) {
    return kTypeGuest;
  }
  if (IsChildFrame())
    return kTypeFrame;
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->delegate() && web_contents()) {
    std::string type = manager->delegate()->GetTargetType(web_contents());
    if (!type.empty())
      return type;
  }
  return kTypePage;
}

std::string RenderFrameDevToolsAgentHost::GetTitle() {
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->delegate() && web_contents()) {
    std::string title = manager->delegate()->GetTargetTitle(web_contents());
    if (!title.empty())
      return title;
  }
  if (IsBrowserSideNavigationEnabled()) {
    if (IsChildFrame() && frame_host_)
      return frame_host_->GetLastCommittedURL().spec();
  } else {
    if (current_ && current_->host()->GetParent())
      return current_->host()->GetLastCommittedURL().spec();
  }
  if (web_contents())
    return base::UTF16ToUTF8(web_contents()->GetTitle());
  return GetURL().spec();
}

std::string RenderFrameDevToolsAgentHost::GetDescription() {
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->delegate() && web_contents())
    return manager->delegate()->GetTargetDescription(web_contents());
  return std::string();
}

GURL RenderFrameDevToolsAgentHost::GetURL() {
  // Order is important here.
  WebContents* web_contents = GetWebContents();
  if (web_contents && !IsChildFrame())
    return web_contents->GetVisibleURL();
  if (IsBrowserSideNavigationEnabled()) {
    if (frame_host_)
      return frame_host_->GetLastCommittedURL();
  } else {
    if (pending_)
      return pending_->host()->GetLastCommittedURL();
    if (current_)
      return current_->host()->GetLastCommittedURL();
  }
  return GURL();
}

GURL RenderFrameDevToolsAgentHost::GetFaviconURL() {
  WebContents* wc = web_contents();
  if (!wc)
    return GURL();
  NavigationEntry* entry = wc->GetController().GetLastCommittedEntry();
  if (entry)
    return entry->GetFavicon().url;
  return GURL();
}

bool RenderFrameDevToolsAgentHost::Activate() {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(web_contents());
  if (wc) {
    wc->Activate();
    return true;
  }
  return false;
}

void RenderFrameDevToolsAgentHost::Reload() {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(web_contents());
  if (wc)
    wc->GetController().Reload(ReloadType::NORMAL, true);
}

bool RenderFrameDevToolsAgentHost::Close() {
  if (web_contents()) {
    web_contents()->ClosePage();
    return true;
  }
  return false;
}

base::TimeTicks RenderFrameDevToolsAgentHost::GetLastActivityTime() {
  if (content::WebContents* contents = web_contents())
    return contents->GetLastActiveTime();
  return base::TimeTicks();
}

void RenderFrameDevToolsAgentHost::SignalSynchronousSwapCompositorFrame(
    RenderFrameHost* frame_host,
    cc::CompositorFrameMetadata frame_metadata) {
  scoped_refptr<RenderFrameDevToolsAgentHost> dtah(FindAgentHost(
      static_cast<RenderFrameHostImpl*>(frame_host)->frame_tree_node()));
  if (dtah) {
    // Unblock the compositor.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &RenderFrameDevToolsAgentHost::SynchronousSwapCompositorFrame,
            dtah.get(),
            base::Passed(std::move(frame_metadata))));
  }
}

void RenderFrameDevToolsAgentHost::SynchronousSwapCompositorFrame(
    cc::CompositorFrameMetadata frame_metadata) {
  for (auto* page : protocol::PageHandler::ForAgentHost(this))
    page->OnSynchronousSwapCompositorFrame(frame_metadata.Clone());
  for (auto* input : protocol::InputHandler::ForAgentHost(this))
    input->OnSwapCompositorFrame(frame_metadata);

  if (!frame_trace_recorder_)
    return;
  bool did_initiate_recording = false;
  for (auto* tracing : protocol::TracingHandler::ForAgentHost(this))
    did_initiate_recording |= tracing->did_initiate_recording();
  if (did_initiate_recording) {
    if (IsBrowserSideNavigationEnabled()) {
      frame_trace_recorder_->OnSynchronousSwapCompositorFrame(frame_host_,
                                                              frame_metadata);
    } else {
      frame_trace_recorder_->OnSynchronousSwapCompositorFrame(
          current_ ? current_->host() : nullptr, frame_metadata);
    }
  }
}

void RenderFrameDevToolsAgentHost::OnDispatchOnInspectorFrontend(
    RenderFrameHost* sender,
    const DevToolsMessageChunk& message) {
  bool success = true;
  if (IsBrowserSideNavigationEnabled()) {
    if (sender == frame_host_)
      success = session()->ReceiveMessageChunk(message);
  } else {
    if (current_ && current_->host() == sender)
      success = current_->ProcessChunkedMessageFromAgent(message);
    else if (pending_ && pending_->host() == sender)
      success = pending_->ProcessChunkedMessageFromAgent(message);
  }
  if (!success) {
    bad_message::ReceivedBadMessage(
        sender->GetProcess(),
        bad_message::RFH_INCONSISTENT_DEVTOOLS_MESSAGE);
  }
}

void RenderFrameDevToolsAgentHost::OnRequestNewWindow(
    RenderFrameHost* sender,
    int new_routing_id) {
  RenderFrameHostImpl* frame_host = RenderFrameHostImpl::FromID(
      sender->GetProcess()->GetID(), new_routing_id);

  bool success = false;
  if (IsAttached() && sender->GetRoutingID() != new_routing_id && frame_host) {
    scoped_refptr<DevToolsAgentHost> agent =
        RenderFrameDevToolsAgentHost::GetOrCreateFor(
            frame_host->frame_tree_node());
    success = static_cast<DevToolsAgentHostImpl*>(agent.get())->Inspect();
  }

  sender->Send(new DevToolsAgentMsg_RequestNewWindow_ACK(
      sender->GetRoutingID(), success));
}

bool RenderFrameDevToolsAgentHost::IsChildFrame() {
  return frame_tree_node_ && frame_tree_node_->parent();
}

}  // namespace content
