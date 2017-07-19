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
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/origin_util.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_browser_main_parts.h"
#include "headless/lib/browser/headless_devtools_client_impl.h"
#include "headless/lib/browser/headless_tab_socket_impl.h"
#include "printing/features/features.h"

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

  // Child contents should have their own root window.
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
      render_process_host_(web_contents->GetRenderProcessHost()),
      weak_ptr_factory_(this) {
#if BUILDFLAG(ENABLE_BASIC_PRINTING) && !defined(CHROME_MULTIPLE_DLL_CHILD)
  HeadlessPrintManager::CreateForWebContents(web_contents);
#endif
  web_contents_->SetDelegate(web_contents_delegate_.get());
  render_process_host_->AddObserver(this);
  agent_host_->AddObserver(this);
}

HeadlessWebContentsImpl::~HeadlessWebContentsImpl() {
  agent_host_->RemoveObserver(this);
  if (render_process_host_)
    render_process_host_->RemoveObserver(this);
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
  service_manager::BinderRegistry* interface_registry =
      render_frame_host->GetInterfaceRegistry();

  for (const MojoService& service : mojo_services_) {
    interface_registry->AddInterface(
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

std::string
HeadlessWebContentsImpl::GetUntrustedDevToolsFrameIdForFrameTreeNodeId(
    int process_id,
    int frame_tree_node_id) const {
  return content::DevToolsAgentHost::
      GetUntrustedDevToolsFrameIdForFrameTreeNodeId(process_id,
                                                    frame_tree_node_id);
}

int HeadlessWebContentsImpl::GetMainFrameRenderProcessId() const {
  return web_contents()->GetMainFrame()->GetProcess()->GetID();
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
