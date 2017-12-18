// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/render_frame_devtools_agent_host.h"

#include <tuple>
#include <utility>

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
#include "content/browser/devtools/protocol/browser_handler.h"
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
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "third_party/WebKit/common/associated_interfaces/associated_interface_provider.h"

#if defined(OS_ANDROID)
#include "content/public/browser/render_widget_host_view.h"
#include "services/device/public/interfaces/wake_lock_context.mojom.h"
#endif

namespace content {

typedef std::vector<RenderFrameDevToolsAgentHost*> RenderFrameDevToolsArray;

namespace {
base::LazyInstance<RenderFrameDevToolsArray>::Leaky g_agent_host_instances =
    LAZY_INSTANCE_INITIALIZER;

RenderFrameDevToolsAgentHost* FindAgentHost(FrameTreeNode* frame_tree_node) {
  if (!g_agent_host_instances.IsCreated())
    return nullptr;
  for (RenderFrameDevToolsArray::iterator it =
           g_agent_host_instances.Get().begin();
       it != g_agent_host_instances.Get().end(); ++it) {
    if ((*it)->frame_tree_node() == frame_tree_node)
      return *it;
  }
  return nullptr;
}

bool ShouldCreateDevToolsForHost(RenderFrameHost* rfh) {
  return rfh->IsCrossProcessSubframe() || !rfh->GetParent();
}

bool ShouldCreateDevToolsForNode(FrameTreeNode* ftn) {
  return !ftn->parent() || ftn->current_frame_host()->IsCrossProcessSubframe();
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
  void Suspend();
  void Resume();
  std::string StateCookie(int session_id) const;
  void ReattachWithCookie(DevToolsSession* session, std::string cookie);

 private:
  struct Message {
    std::string method;
    std::string message;
  };

  class SessionHost : public mojom::DevToolsSessionHost {
   public:
    SessionHost(FrameHostHolder* owner,
                int session_id,
                const base::Optional<std::string>& reattach_state)
        : owner_(owner),
          session_id_(session_id),
          binding_(this),
          chunk_processor_(
              base::Callback<void(int, const std::string&)>(),
              base::Bind(&RenderFrameDevToolsAgentHost::FrameHostHolder::
                             SessionHost::SendMessageFromProcessor,
                         base::Unretained(this))) {
      if (reattach_state.has_value())
        chunk_processor_.set_state_cookie(reattach_state.value());
      mojom::DevToolsSessionHostAssociatedPtrInfo host_ptr_info;
      binding_.Bind(mojo::MakeRequest(&host_ptr_info));
      owner_->agent_ptr_->AttachDevToolsSession(
          std::move(host_ptr_info), mojo::MakeRequest(&session_ptr_),
          mojo::MakeRequest(&io_session_ptr_), reattach_state);
    }

    void RedispatchProtocolMessagesFrom(SessionHost* other) {
      for (const auto& pair : other->sent_messages_) {
        DispatchProtocolMessage(pair.first, pair.second.method,
                                pair.second.message);
      }
    }

    const std::string& state_cookie() const {
      return chunk_processor_.state_cookie();
    }

    void DispatchProtocolMessage(int call_id,
                                 const std::string& method,
                                 const std::string& message) {
      if (DevToolsSession::ShouldSendOnIO(method))
        io_session_ptr_->DispatchProtocolMessage(call_id, method, message);
      else
        session_ptr_->DispatchProtocolMessage(call_id, method, message);
      sent_messages_[call_id] = {method, message};
    }

    void InspectElement(int x, int y) {
      session_ptr_->InspectElement(gfx::Point(x, y));
    }

    // mojom::DevToolsSessionHost implementation.
    void DispatchProtocolMessage(
        mojom::DevToolsMessageChunkPtr chunk) override {
      if (chunk_processor_.ProcessChunkedMessageFromAgent(std::move(chunk)))
        return;

      binding_.Close();
      if (owner_->host_->GetProcess()) {
        bad_message::ReceivedBadMessage(
            owner_->host_->GetProcess(),
            bad_message::RFH_INCONSISTENT_DEVTOOLS_MESSAGE);
      }
    }

    void RequestNewWindow(int32_t frame_routing_id,
                          RequestNewWindowCallback callback) override {
      bool success = false;
      RenderFrameHostImpl* frame_host =
          owner_->host_->GetProcess()
              ? RenderFrameHostImpl::FromID(
                    owner_->host_->GetProcess()->GetID(), frame_routing_id)
              : nullptr;
      if (frame_host) {
        scoped_refptr<DevToolsAgentHost> agent =
            RenderFrameDevToolsAgentHost::GetOrCreateFor(
                frame_host->frame_tree_node());
        success = static_cast<DevToolsAgentHostImpl*>(agent.get())->Inspect();
      }
      std::move(callback).Run(success);
    }

    void SendMessageFromProcessor(const std::string& message) {
      int id = chunk_processor_.last_call_id();
      Message sent_message = std::move(sent_messages_[id]);
      sent_messages_.erase(id);
      if (suspended_) {
        pending_messages_.push_back(message);
      } else {
        DevToolsSession* session = owner_->agent_->SessionById(session_id_);
        if (session)
          session->SendMessageToClient(message);
        // |this| may be deleted at this point.
      }
    }

    void Suspend() { suspended_ = true; }

    void Resume() {
      suspended_ = false;
      DevToolsSession* session = owner_->agent_->SessionById(session_id_);
      std::vector<std::string> messages;
      messages.swap(pending_messages_);
      for (std::string& message : messages)
        session->SendMessageToClient(message);
    }

   private:
    FrameHostHolder* owner_;
    int session_id_;
    mojo::AssociatedBinding<mojom::DevToolsSessionHost> binding_;
    mojom::DevToolsSessionAssociatedPtr session_ptr_;
    mojom::DevToolsSessionPtr io_session_ptr_;
    DevToolsMessageChunkProcessor chunk_processor_;
    std::vector<std::string> pending_messages_;
    using CallId = int;
    std::map<CallId, Message> sent_messages_;
    bool suspended_ = false;

    DISALLOW_COPY_AND_ASSIGN(SessionHost);
  };

  RenderFrameDevToolsAgentHost* agent_;
  RenderFrameHostImpl* host_;
  mojom::DevToolsAgentAssociatedPtr agent_ptr_;
  base::flat_map<int, std::unique_ptr<SessionHost>> session_hosts_;
};

RenderFrameDevToolsAgentHost::FrameHostHolder::FrameHostHolder(
    RenderFrameDevToolsAgentHost* agent,
    RenderFrameHostImpl* host)
    : agent_(agent), host_(host) {
  DCHECK(!IsBrowserSideNavigationEnabled());
  DCHECK(agent_);
  DCHECK(host_);
}

RenderFrameDevToolsAgentHost::FrameHostHolder::~FrameHostHolder() {
  if (!session_hosts_.empty())
    agent_->RevokePolicy(host_);
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::Attach(
    DevToolsSession* session) {
  agent_->GrantPolicy(host_);
  // |agent_ptr_| is used by SessionHost in constructor to attach.
  if (!agent_ptr_)
    host_->GetRemoteAssociatedInterfaces()->GetInterface(&agent_ptr_);
  session_hosts_[session->session_id()].reset(
      new SessionHost(this, session->session_id(), base::nullopt));
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::Reattach(
    FrameHostHolder* old) {
  for (DevToolsSession* session : agent_->sessions()) {
    int session_id = session->session_id();
    std::string cookie = old ? old->StateCookie(session_id) : std::string();
    ReattachWithCookie(session, std::move(cookie));
    if (!old)
      continue;
    auto it = old->session_hosts_.find(session_id);
    if (it != old->session_hosts_.end()) {
      session_hosts_[session_id]->RedispatchProtocolMessagesFrom(
          it->second.get());
    }
  }
}

std::string RenderFrameDevToolsAgentHost::FrameHostHolder::StateCookie(
    int session_id) const {
  auto it = session_hosts_.find(session_id);
  return it == session_hosts_.end() ? std::string()
                                    : it->second->state_cookie();
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::ReattachWithCookie(
    DevToolsSession* session,
    std::string cookie) {
  agent_->GrantPolicy(host_);
  // |agent_ptr_| is used by SessionHost in constructor to attach.
  if (!agent_ptr_)
    host_->GetRemoteAssociatedInterfaces()->GetInterface(&agent_ptr_);
  session_hosts_[session->session_id()].reset(
      new SessionHost(this, session->session_id(), cookie));
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::Detach(int session_id) {
  agent_->RevokePolicy(host_);
  session_hosts_.erase(session_id);
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::DispatchProtocolMessage(
    int session_id,
    int call_id,
    const std::string& method,
    const std::string& message) {
  auto it = session_hosts_.find(session_id);
  if (it != session_hosts_.end())
    it->second->DispatchProtocolMessage(call_id, method, message);
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::InspectElement(
    int session_id, int x, int y) {
  auto it = session_hosts_.find(session_id);
  if (it != session_hosts_.end())
    it->second->InspectElement(x, y);
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::Suspend() {
  for (auto& pair : session_hosts_)
    pair.second->Suspend();
}

void RenderFrameDevToolsAgentHost::FrameHostHolder::Resume() {
  for (auto& pair : session_hosts_)
    pair.second->Resume();
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
scoped_refptr<DevToolsAgentHost>
RenderFrameDevToolsAgentHost::GetOrCreateForDangling(
    FrameTreeNode* frame_tree_node) {
  // Note that this method does not use FrameTreeNode::current_frame_host(),
  // since it is used while the frame host may not be set as current yet,
  // for example right before commit time.
  // So the caller must be sure that passed frame will indeed be a correct
  // devtools target (see ShouldCreateDevToolsForNode above).
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
void RenderFrameDevToolsAgentHost::OnResetNavigationRequest(
    NavigationRequest* navigation_request) {
  RenderFrameDevToolsAgentHost* agent_host =
      FindAgentHost(navigation_request->frame_tree_node());
  if (!agent_host)
    return;
  if (navigation_request->net_error() != net::OK) {
    for (auto* network : protocol::NetworkHandler::ForAgentHost(agent_host))
      network->NavigationFailed(navigation_request);
  }
  for (auto* page : protocol::PageHandler::ForAgentHost(agent_host))
    page->NavigationReset(navigation_request);
}

// static
std::vector<std::unique_ptr<NavigationThrottle>>
RenderFrameDevToolsAgentHost::CreateNavigationThrottles(
    NavigationHandle* navigation_handle) {
  std::vector<std::unique_ptr<NavigationThrottle>> result;
  FrameTreeNode* frame_tree_node =
      static_cast<NavigationHandleImpl*>(navigation_handle)->frame_tree_node();

  // Interception might throttle navigations in inspected frames.
  RenderFrameDevToolsAgentHost* agent_host = FindAgentHost(frame_tree_node);
  if (agent_host) {
    for (auto* network_handler :
         protocol::NetworkHandler::ForAgentHost(agent_host)) {
      std::unique_ptr<NavigationThrottle> throttle =
          network_handler->CreateThrottleForNavigation(navigation_handle);
      if (throttle)
        result.push_back(std::move(throttle));
    }
  }

  agent_host = nullptr;
  if (frame_tree_node->parent()) {
    // Target domain of the parent frame's DevTools may want to pause
    // this frame to do some setup.
    agent_host =
        FindAgentHost(GetFrameTreeNodeAncestor(frame_tree_node->parent()));
  }
  if (agent_host) {
    for (auto* target_handler :
         protocol::TargetHandler::ForAgentHost(agent_host)) {
      std::unique_ptr<NavigationThrottle> throttle =
          target_handler->CreateThrottleForNavigation(navigation_handle);
      if (throttle)
        result.push_back(std::move(throttle));
    }
  }

  return result;
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
void RenderFrameDevToolsAgentHost::AppendDevToolsHeaders(
    FrameTreeNode* frame_tree_node,
    net::HttpRequestHeaders* headers) {
  frame_tree_node = GetFrameTreeNodeAncestor(frame_tree_node);
  RenderFrameDevToolsAgentHost* agent_host = FindAgentHost(frame_tree_node);
  if (!agent_host)
    return;
  for (auto* network : protocol::NetworkHandler::ForAgentHost(agent_host))
    network->AppendDevToolsHeaders(headers);
}

// static
bool RenderFrameDevToolsAgentHost::ShouldBypassServiceWorker(
    FrameTreeNode* frame_tree_node) {
  frame_tree_node = GetFrameTreeNodeAncestor(frame_tree_node);
  RenderFrameDevToolsAgentHost* agent_host = FindAgentHost(frame_tree_node);
  if (!agent_host)
    return false;
  for (auto* network : protocol::NetworkHandler::ForAgentHost(agent_host)) {
    if (network->ShouldBypassServiceWorker())
      return true;
  }
  return false;
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
    : DevToolsAgentHostImpl(frame_tree_node->devtools_frame_token().ToString()),
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

  g_agent_host_instances.Get().push_back(this);
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

  protocol::EmulationHandler* emulation_handler =
      new protocol::EmulationHandler();
  session->AddHandler(base::WrapUnique(new protocol::BrowserHandler()));
  session->AddHandler(base::WrapUnique(new protocol::DOMHandler()));
  session->AddHandler(base::WrapUnique(emulation_handler));
  session->AddHandler(base::WrapUnique(new protocol::InputHandler()));
  session->AddHandler(base::WrapUnique(new protocol::InspectorHandler()));
  session->AddHandler(base::WrapUnique(new protocol::IOHandler(
      GetIOContext())));
  session->AddHandler(base::WrapUnique(new protocol::NetworkHandler(GetId())));
  session->AddHandler(base::WrapUnique(new protocol::SchemaHandler()));
  session->AddHandler(base::WrapUnique(new protocol::ServiceWorkerHandler()));
  session->AddHandler(base::WrapUnique(new protocol::StorageHandler()));
  session->AddHandler(base::WrapUnique(new protocol::TargetHandler()));
  session->AddHandler(base::WrapUnique(new protocol::TracingHandler(
      protocol::TracingHandler::Renderer,
      frame_tree_node_ ? frame_tree_node_->frame_tree_node_id() : 0,
      GetIOContext())));
  if (frame_tree_node_ && !frame_tree_node_->parent()) {
    session->AddHandler(
        base::WrapUnique(new protocol::PageHandler(emulation_handler)));
    session->AddHandler(base::WrapUnique(new protocol::SecurityHandler()));
  }

  if (IsBrowserSideNavigationEnabled()) {
    if (EnsureAgent())
      session->AttachToAgent(agent_ptr_);
  } else {
    if (current_)
      current_->Attach(session);
    if (pending_)
      pending_->Attach(session);
  }
  if (sessions().size() == 1)
    OnClientsAttached();
}

void RenderFrameDevToolsAgentHost::DetachSession(int session_id) {
  if (IsBrowserSideNavigationEnabled()) {
    // Destroying session automatically detaches in renderer.
    suspended_messages_by_session_id_.erase(session_id);
  } else {
    if (current_)
      current_->Detach(session_id);
    if (pending_)
      pending_->Detach(session_id);
  }
  if (sessions().empty())
    OnClientsDetached();
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
      suspended_messages_by_session_id_[session_id].push_back(
          {call_id, method, message});
      return true;
    }
    session->DispatchProtocolMessageToAgent(call_id, method, message);
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
    session->InspectElement(gfx::Point(x, y));
  } else {
    if (current_)
      current_->InspectElement(session->session_id(), x, y);
    if (pending_)
      pending_->InspectElement(session->session_id(), x, y);
  }
}

void RenderFrameDevToolsAgentHost::OnClientsAttached() {
  frame_trace_recorder_.reset(new DevToolsFrameTraceRecorder());
#if defined(OS_ANDROID)
  GetWakeLock()->RequestWakeLock();
#endif
  if (IsBrowserSideNavigationEnabled())
    GrantPolicy(frame_host_);
}

void RenderFrameDevToolsAgentHost::OnClientsDetached() {
#if defined(OS_ANDROID)
  GetWakeLock()->CancelWakeLock();
#endif
  frame_trace_recorder_.reset();
  if (IsBrowserSideNavigationEnabled())
    RevokePolicy(frame_host_);
}

RenderFrameDevToolsAgentHost::~RenderFrameDevToolsAgentHost() {
  RenderFrameDevToolsArray::iterator it =
      std::find(g_agent_host_instances.Get().begin(),
                g_agent_host_instances.Get().end(), this);
  if (it != g_agent_host_instances.Get().end())
    g_agent_host_instances.Get().erase(it);
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
  NotifyNavigated();
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
        target->DidCommitNavigation();
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
  UpdateFrameHost(frame_tree_node_->current_frame_host());
  DCHECK(CheckConsistency());

  if (navigation_handles_.empty()) {
    for (auto& pair : suspended_messages_by_session_id_) {
      int session_id = pair.first;
      DevToolsSession* session = SessionById(session_id);
      for (const Message& message : pair.second) {
        session->DispatchProtocolMessageToAgent(message.call_id, message.method,
                                                message.message);
        session->waiting_messages()[message.call_id] = {message.method,
                                                        message.message};
      }
    }
    suspended_messages_by_session_id_.clear();
  }
  if (handle->HasCommitted()) {
    for (auto* target : protocol::TargetHandler::ForAgentHost(this))
      target->DidCommitNavigation();
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

  if (IsAttached())
    RevokePolicy(frame_host_);

  if (frame_host && !ShouldCreateDevToolsForHost(frame_host)) {
    DestroyOnRenderFrameGone();
    // |this| may be deleted at this point.
    return;
  }

  frame_host_ = frame_host;
  agent_ptr_.reset();
  render_frame_alive_ = true;
  if (IsAttached()) {
    GrantPolicy(frame_host_);
    for (DevToolsSession* session : sessions())
      session->SetRenderFrameHost(frame_host);
    MaybeReattachToRenderFrame();
  }
}

void RenderFrameDevToolsAgentHost::MaybeReattachToRenderFrame() {
  DCHECK(IsBrowserSideNavigationEnabled());
  if (!EnsureAgent())
    return;
  for (DevToolsSession* session : sessions()) {
    session->ReattachToAgent(agent_ptr_);
    for (const auto& pair : session->waiting_messages()) {
      int call_id = pair.first;
      const DevToolsSession::Message& message = pair.second;
      session->DispatchProtocolMessageToAgent(call_id, message.method,
                                              message.message);
    }
  }
}

void RenderFrameDevToolsAgentHost::GrantPolicy(RenderFrameHostImpl* host) {
  if (!host)
    return;
  uint32_t process_id = host->GetProcess()->GetID();
  if (base::FeatureList::IsEnabled(features::kNetworkService))
    GetNetworkService()->SetRawHeadersAccess(process_id, true);
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadRawCookies(
      process_id);
}

void RenderFrameDevToolsAgentHost::RevokePolicy(RenderFrameHostImpl* host) {
  if (!host)
    return;

  bool process_has_agents = false;
  RenderProcessHost* process_host = host->GetProcess();
  for (RenderFrameDevToolsAgentHost* agent : g_agent_host_instances.Get()) {
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
    if (base::FeatureList::IsEnabled(features::kNetworkService))
      GetNetworkService()->SetRawHeadersAccess(process_host->GetID(), false);
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
    target->RenderFrameHostChanged();

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
    if (rfh == frame_host_) {
      render_frame_alive_ = false;
      agent_ptr_.reset();
    }
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
    OnClientsDetached();
  ForceDetachAllClients();
  frame_host_ = nullptr;
  agent_ptr_.reset();
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
          device::mojom::WakeLockType::kPreventDisplaySleep,
          device::mojom::WakeLockReason::kOther, "DevTools",
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
  const viz::CompositorFrameMetadata& metadata =
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
  for (DevToolsSession* session : sessions())
    session->SetRenderFrameHost(host);
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
  for (DevToolsSession* session : sessions()) {
    int session_id = session->session_id();
    disconnected_cookie_for_session_[session_id] =
        current_->StateCookie(session_id);
    current_->Detach(session_id);
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
  for (DevToolsSession* session : sessions()) {
    current_->ReattachWithCookie(
        session,
        std::move(disconnected_cookie_for_session_[session->session_id()]));
  }
  disconnected_cookie_for_session_.clear();

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

std::string RenderFrameDevToolsAgentHost::GetOpenerId() {
  if (!frame_tree_node_)
    return std::string();
  FrameTreeNode* opener = frame_tree_node_->original_opener();
  return opener ? opener->devtools_frame_token().ToString() : std::string();
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
  if (WebContents* contents = web_contents())
    return contents->GetLastActiveTime();
  return base::TimeTicks();
}

void RenderFrameDevToolsAgentHost::SignalSynchronousSwapCompositorFrame(
    RenderFrameHost* frame_host,
    viz::CompositorFrameMetadata frame_metadata) {
  scoped_refptr<RenderFrameDevToolsAgentHost> dtah(FindAgentHost(
      static_cast<RenderFrameHostImpl*>(frame_host)->frame_tree_node()));
  if (dtah) {
    // Unblock the compositor.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(
            &RenderFrameDevToolsAgentHost::SynchronousSwapCompositorFrame,
            dtah.get(), base::Passed(std::move(frame_metadata))));
  }
}

void RenderFrameDevToolsAgentHost::SynchronousSwapCompositorFrame(
    viz::CompositorFrameMetadata frame_metadata) {
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

bool RenderFrameDevToolsAgentHost::EnsureAgent() {
  if (!frame_host_ || !render_frame_alive_)
    return false;
  if (!agent_ptr_)
    frame_host_->GetRemoteAssociatedInterfaces()->GetInterface(&agent_ptr_);
  return true;
}

bool RenderFrameDevToolsAgentHost::IsChildFrame() {
  return frame_tree_node_ && frame_tree_node_->parent();
}

}  // namespace content
