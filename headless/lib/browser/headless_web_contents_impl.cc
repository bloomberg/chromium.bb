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
#include "services/service_manager/public/cpp/binder_registry.h"

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

class WebContentsObserverAdapter : public content::WebContentsObserver {
 public:
  WebContentsObserverAdapter(content::WebContents* web_contents,
                             HeadlessWebContents::Observer* observer)
      : content::WebContentsObserver(web_contents), observer_(observer) {}

  ~WebContentsObserverAdapter() override {}

  void RenderViewReady() override {
    DCHECK(web_contents()->GetMainFrame()->IsRenderFrameLive());
    observer_->DevToolsTargetReady();
  }

  HeadlessWebContents::Observer* observer() { return observer_; }

 private:
  HeadlessWebContents::Observer* observer_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(WebContentsObserverAdapter);
};

class HeadlessWebContentsImpl::Delegate : public content::WebContentsDelegate {
 public:
  explicit Delegate(HeadlessBrowserContextImpl* browser_context)
      : browser_context_(browser_context) {}

  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override {
    std::unique_ptr<HeadlessWebContentsImpl> web_contents =
        HeadlessWebContentsImpl::CreateFromWebContents(new_contents,
                                                       browser_context_);

    DCHECK(new_contents->GetBrowserContext() == browser_context_);

    browser_context_->RegisterWebContents(std::move(web_contents));
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
    if (!browser_context_) {
      return;
    }

    std::vector<HeadlessWebContents*> all_contents =
        browser_context_->GetAllWebContents();

    for (HeadlessWebContents* wc : all_contents) {
      if (!wc) {
        continue;
      }
      HeadlessWebContentsImpl* hwc = HeadlessWebContentsImpl::From(wc);
      if (hwc->web_contents() == source) {
        wc->Close();
        return;
      }
    }
  }

 private:
  HeadlessBrowserContextImpl* browser_context_;  // Not owned.
  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

namespace {

void ForwardToServiceFactory(
    const base::Callback<void(TabSocketRequest)>& service_factory,
    mojo::ScopedMessagePipeHandle handle) {
  service_factory.Run(TabSocketRequest(std::move(handle)));
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

  if (builder->create_tab_socket_) {
    headless_web_contents->headless_tab_socket_ =
        base::MakeUnique<HeadlessTabSocketImpl>();

    builder->mojo_services_.emplace_back(
        TabSocket::Name_,
        base::Bind(
            &ForwardToServiceFactory,
            base::Bind(
                &HeadlessTabSocketImpl::CreateMojoService,
                base::Unretained(
                    headless_web_contents->headless_tab_socket_.get()))));
  }

  headless_web_contents->mojo_services_ = std::move(builder->mojo_services_);
  headless_web_contents->InitializeScreen(builder->window_size_);
  if (!headless_web_contents->OpenURL(builder->initial_url_))
    return nullptr;
  return headless_web_contents;
}

// static
std::unique_ptr<HeadlessWebContentsImpl>
HeadlessWebContentsImpl::CreateFromWebContents(
    content::WebContents* web_contents,
    HeadlessBrowserContextImpl* browser_context) {
  std::unique_ptr<HeadlessWebContentsImpl> headless_web_contents =
      base::WrapUnique(
          new HeadlessWebContentsImpl(web_contents, browser_context));

  return headless_web_contents;
}

void HeadlessWebContentsImpl::InitializeScreen(const gfx::Size& initial_size) {
  browser()->PlatformInitializeWebContents(initial_size, this);
}

HeadlessWebContentsImpl::HeadlessWebContentsImpl(
    content::WebContents* web_contents,
    HeadlessBrowserContextImpl* browser_context)
    : content::WebContentsObserver(web_contents),
      web_contents_delegate_(
          new HeadlessWebContentsImpl::Delegate(browser_context)),
      web_contents_(web_contents),
      agent_host_(content::DevToolsAgentHost::GetOrCreateFor(web_contents)),
      browser_context_(browser_context),
      render_process_host_(web_contents->GetRenderProcessHost()) {
#if BUILDFLAG(ENABLE_BASIC_PRINTING) && !defined(CHROME_MULTIPLE_DLL_CHILD)
  printing::HeadlessPrintManager::CreateForWebContents(web_contents);
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

void HeadlessWebContentsImpl::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (!mojo_services_.empty()) {
    render_frame_host->AllowBindings(content::BINDINGS_POLICY_HEADLESS);
  }

  service_manager::BinderRegistry* interface_registry =
      render_frame_host->GetInterfaceRegistry();

  for (const MojoService& service : mojo_services_) {
    interface_registry->AddInterface(service.service_name,
                                     service.service_factory,
                                     browser()->BrowserMainThread());
  }

  browser_context_->SetFrameTreeNodeId(render_frame_host->GetProcess()->GetID(),
                                       render_frame_host->GetRoutingID(),
                                       render_frame_host->GetFrameTreeNodeId());
}

void HeadlessWebContentsImpl::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  browser_context_->RemoveFrameTreeNode(
      render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRoutingID());
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
  DCHECK(observer_map_.find(observer) == observer_map_.end());
  observer_map_[observer] = base::MakeUnique<WebContentsObserverAdapter>(
      web_contents_.get(), observer);
}

void HeadlessWebContentsImpl::RemoveObserver(Observer* observer) {
  ObserverMap::iterator it = observer_map_.find(observer);
  DCHECK(it != observer_map_.end());
  observer_map_.erase(it);
}

void HeadlessWebContentsImpl::DevToolsAgentHostAttached(
    content::DevToolsAgentHost* agent_host) {
  for (const auto& pair : observer_map_) {
    pair.second->observer()->DevToolsClientAttached();
  }
}

void HeadlessWebContentsImpl::DevToolsAgentHostDetached(
    content::DevToolsAgentHost* agent_host) {
  for (const auto& pair : observer_map_) {
    pair.second->observer()->DevToolsClientDetached();
  }
}

void HeadlessWebContentsImpl::RenderProcessExited(
    content::RenderProcessHost* host,
    base::TerminationStatus status,
    int exit_code) {
  DCHECK_EQ(render_process_host_, host);
  for (const auto& pair : observer_map_) {
    pair.second->observer()->RenderProcessExited(status, exit_code);
  }
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

HeadlessWebContents::Builder& HeadlessWebContents::Builder::CreateTabSocket(
    bool create_tab_socket) {
  create_tab_socket_ = create_tab_socket;
  return *this;
}

HeadlessWebContents* HeadlessWebContents::Builder::Build() {
  return browser_context_->CreateWebContents(this);
}

HeadlessWebContents::Builder::MojoService::MojoService() {}

HeadlessWebContents::Builder::MojoService::MojoService(
    const std::string& service_name,
    const base::Callback<void(mojo::ScopedMessagePipeHandle)>& service_factory)
    : service_name(service_name), service_factory(service_factory) {}

HeadlessWebContents::Builder::MojoService::~MojoService() {}

}  // namespace headless
