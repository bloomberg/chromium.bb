// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_client_utils.h"

#include <algorithm>
#include <tuple>

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/service_worker/service_worker_client_info.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/child_process_host.h"
#include "url/gurl.h"

namespace content {
namespace service_worker_client_utils {

namespace {

using OpenURLCallback = base::Callback<void(int, int)>;
using GetWindowClientsCallback =
    base::Callback<void(std::unique_ptr<ServiceWorkerClients>)>;

// The OpenURLObserver class is a WebContentsObserver that will wait for a
// WebContents to be initialized, run the |callback| passed to its constructor
// then self destroy.
// The callback will receive the process and frame ids. If something went wrong
// those will be (kInvalidUniqueID, MSG_ROUTING_NONE).
// The callback will be called in the IO thread.
class OpenURLObserver : public WebContentsObserver {
 public:
  OpenURLObserver(WebContents* web_contents,
                  int frame_tree_node_id,
                  const OpenURLCallback& callback)
      : WebContentsObserver(web_contents),
        frame_tree_node_id_(frame_tree_node_id),
        callback_(callback) {}

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    DCHECK(web_contents());
    if (!navigation_handle->HasCommitted()) {
      // Return error.
      RunCallback(ChildProcessHost::kInvalidUniqueID, MSG_ROUTING_NONE);
      return;
    }

    if (navigation_handle->GetFrameTreeNodeId() != frame_tree_node_id_) {
      // Return error.
      RunCallback(ChildProcessHost::kInvalidUniqueID, MSG_ROUTING_NONE);
      return;
    }

    RenderFrameHost* render_frame_host =
        navigation_handle->GetRenderFrameHost();
    RunCallback(render_frame_host->GetProcess()->GetID(),
                render_frame_host->GetRoutingID());
  }

  void RenderProcessGone(base::TerminationStatus status) override {
    RunCallback(ChildProcessHost::kInvalidUniqueID, MSG_ROUTING_NONE);
  }

  void WebContentsDestroyed() override {
    RunCallback(ChildProcessHost::kInvalidUniqueID, MSG_ROUTING_NONE);
  }

 private:
  void RunCallback(int render_process_id, int render_frame_id) {
    // After running the callback, |this| will stop observing, thus
    // web_contents() should return nullptr and |RunCallback| should no longer
    // be called. Then, |this| will self destroy.
    DCHECK(web_contents());

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(callback_, render_process_id, render_frame_id));
    Observe(nullptr);
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }

  int frame_tree_node_id_;
  const OpenURLCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(OpenURLObserver);
};

ServiceWorkerClientInfo GetWindowClientInfoOnUI(
    int render_process_id,
    int render_frame_id,
    base::TimeTicks create_time,
    const std::string& client_uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return ServiceWorkerClientInfo();

  // TODO(mlamouri,michaeln): it is possible to end up collecting information
  // for a frame that is actually being navigated and isn't exactly what we are
  // expecting.
  return ServiceWorkerClientInfo(
      client_uuid, render_frame_host->GetVisibilityState(),
      render_frame_host->IsFocused(), render_frame_host->GetLastCommittedURL(),
      render_frame_host->GetParent() ? REQUEST_CONTEXT_FRAME_TYPE_NESTED
                                     : REQUEST_CONTEXT_FRAME_TYPE_TOP_LEVEL,
      render_frame_host->frame_tree_node()->last_focus_time(), create_time,
      blink::kWebServiceWorkerClientTypeWindow);
}

ServiceWorkerClientInfo FocusOnUI(int render_process_id,
                                  int render_frame_id,
                                  base::TimeTicks create_time,
                                  const std::string& client_uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(render_frame_host));

  if (!render_frame_host || !web_contents)
    return ServiceWorkerClientInfo();

  FrameTreeNode* frame_tree_node = render_frame_host->frame_tree_node();

  // Focus the frame in the frame tree node, in case it has changed.
  frame_tree_node->frame_tree()->SetFocusedFrame(
      frame_tree_node, render_frame_host->GetSiteInstance());

  // Focus the frame's view to make sure the frame is now considered as focused.
  render_frame_host->GetView()->Focus();

  // Move the web contents to the foreground.
  web_contents->Activate();

  return GetWindowClientInfoOnUI(render_process_id, render_frame_id,
                                 create_time, client_uuid);
}

// This is only called for main frame navigations in OpenWindowOnUI().
void DidOpenURLOnUI(const OpenURLCallback& callback,
                    WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!web_contents) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(callback, ChildProcessHost::kInvalidUniqueID,
                       MSG_ROUTING_NONE));
    return;
  }

  // ContentBrowserClient::OpenURL calls ui::BaseWindow::Show which
  // makes the destination window the main+key window, but won't make Chrome
  // the active application (https://crbug.com/470830). Since OpenWindow is
  // always called from a user gesture (e.g. notification click), we should
  // explicitly activate the window, which brings Chrome to the front.
  static_cast<WebContentsImpl*>(web_contents)->Activate();

  RenderFrameHostImpl* rfhi =
      static_cast<RenderFrameHostImpl*>(web_contents->GetMainFrame());
  new OpenURLObserver(web_contents,
                      rfhi->frame_tree_node()->frame_tree_node_id(), callback);
}

void OpenWindowOnUI(
    const GURL& url,
    const GURL& script_url,
    int worker_process_id,
    const scoped_refptr<ServiceWorkerContextWrapper>& context_wrapper,
    WindowOpenDisposition disposition,
    const OpenURLCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserContext* browser_context =
      context_wrapper->storage_partition()
          ? context_wrapper->storage_partition()->browser_context()
          : nullptr;
  // We are shutting down.
  if (!browser_context)
    return;

  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(worker_process_id);
  if (render_process_host->IsForGuestsOnly()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(callback, ChildProcessHost::kInvalidUniqueID,
                       MSG_ROUTING_NONE));
    return;
  }

  OpenURLParams params(
      url,
      Referrer::SanitizeForRequest(
          url, Referrer(script_url, blink::kWebReferrerPolicyDefault)),
      disposition, ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      true /* is_renderer_initiated */);

  GetContentClient()->browser()->OpenURL(browser_context, params,
                                         base::Bind(&DidOpenURLOnUI, callback));
}

void NavigateClientOnUI(const GURL& url,
                        const GURL& script_url,
                        int process_id,
                        int frame_id,
                        const OpenURLCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHostImpl* rfhi = RenderFrameHostImpl::FromID(process_id, frame_id);
  WebContents* web_contents = WebContents::FromRenderFrameHost(rfhi);

  if (!rfhi || !web_contents) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(callback, ChildProcessHost::kInvalidUniqueID,
                       MSG_ROUTING_NONE));
    return;
  }

  ui::PageTransition transition = rfhi->GetParent()
                                      ? ui::PAGE_TRANSITION_AUTO_SUBFRAME
                                      : ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  int frame_tree_node_id = rfhi->frame_tree_node()->frame_tree_node_id();

  OpenURLParams params(
      url,
      Referrer::SanitizeForRequest(
          url, Referrer(script_url, blink::kWebReferrerPolicyDefault)),
      frame_tree_node_id, WindowOpenDisposition::CURRENT_TAB, transition,
      true /* is_renderer_initiated */);
  web_contents->OpenURL(params);
  new OpenURLObserver(web_contents, frame_tree_node_id, callback);
}

void DidNavigate(const base::WeakPtr<ServiceWorkerContextCore>& context,
                 const GURL& origin,
                 const NavigationCallback& callback,
                 int render_process_id,
                 int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!context) {
    callback.Run(SERVICE_WORKER_ERROR_ABORT, ServiceWorkerClientInfo());
    return;
  }

  if (render_process_id == ChildProcessHost::kInvalidUniqueID &&
      render_frame_id == MSG_ROUTING_NONE) {
    callback.Run(SERVICE_WORKER_ERROR_FAILED, ServiceWorkerClientInfo());
    return;
  }

  for (std::unique_ptr<ServiceWorkerContextCore::ProviderHostIterator> it =
           context->GetClientProviderHostIterator(origin);
       !it->IsAtEnd(); it->Advance()) {
    ServiceWorkerProviderHost* provider_host = it->GetProviderHost();
    if (provider_host->process_id() != render_process_id ||
        provider_host->frame_id() != render_frame_id) {
      continue;
    }
    BrowserThread::PostTaskAndReplyWithResult(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&GetWindowClientInfoOnUI, provider_host->process_id(),
                   provider_host->route_id(), provider_host->create_time(),
                   provider_host->client_uuid()),
        base::Bind(callback, SERVICE_WORKER_OK));
    return;
  }

  // If here, it means that no provider_host was found, in which case, the
  // renderer should still be informed that the window was opened.
  callback.Run(SERVICE_WORKER_OK, ServiceWorkerClientInfo());
}

void AddWindowClient(
    ServiceWorkerProviderHost* host,
    std::vector<std::tuple<int, int, base::TimeTicks, std::string>>*
        client_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (host->client_type() != blink::kWebServiceWorkerClientTypeWindow)
    return;
  client_info->push_back(std::make_tuple(host->process_id(), host->frame_id(),
                                         host->create_time(),
                                         host->client_uuid()));
}

void AddNonWindowClient(ServiceWorkerProviderHost* host,
                        const ServiceWorkerClientQueryOptions& options,
                        ServiceWorkerClients* clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  blink::WebServiceWorkerClientType host_client_type = host->client_type();
  if (host_client_type == blink::kWebServiceWorkerClientTypeWindow)
    return;
  if (options.client_type != blink::kWebServiceWorkerClientTypeAll &&
      options.client_type != host_client_type)
    return;

  ServiceWorkerClientInfo client_info(
      host->client_uuid(), blink::kWebPageVisibilityStateHidden,
      false,  // is_focused
      host->document_url(), REQUEST_CONTEXT_FRAME_TYPE_NONE, base::TimeTicks(),
      host->create_time(), host_client_type);
  clients->push_back(client_info);
}

void OnGetWindowClientsOnUI(
    // The tuple contains process_id, frame_id, create_time, client_uuid.
    const std::vector<std::tuple<int, int, base::TimeTicks, std::string>>&
        clients_info,
    const GURL& script_url,
    const GetWindowClientsCallback& callback,
    std::unique_ptr<ServiceWorkerClients> clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (const auto& it : clients_info) {
    ServiceWorkerClientInfo info = GetWindowClientInfoOnUI(
        std::get<0>(it), std::get<1>(it), std::get<2>(it), std::get<3>(it));

    // If the request to the provider_host returned an empty
    // ServiceWorkerClientInfo, that means that it wasn't possible to associate
    // it with a valid RenderFrameHost. It might be because the frame was killed
    // or navigated in between.
    if (info.IsEmpty())
      continue;

    // We can get info for a frame that was navigating end ended up with a
    // different URL than expected. In such case, we should make sure to not
    // expose cross-origin WindowClient.
    if (info.url.GetOrigin() != script_url.GetOrigin())
      continue;

    clients->push_back(info);
  }

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::BindOnce(callback, base::Passed(&clients)));
}

struct ServiceWorkerClientInfoSort {
  bool operator()(const ServiceWorkerClientInfo& a,
                  const ServiceWorkerClientInfo& b) const {
    // Clients for windows should be appeared earlier.
    if (a.client_type == blink::kWebServiceWorkerClientTypeWindow &&
        b.client_type != blink::kWebServiceWorkerClientTypeWindow) {
      return true;
    }
    if (a.client_type != blink::kWebServiceWorkerClientTypeWindow &&
        b.client_type == blink::kWebServiceWorkerClientTypeWindow) {
      return false;
    }

    // Clients focused recently should be appeared earlier.
    if (a.last_focus_time != b.last_focus_time)
      return a.last_focus_time > b.last_focus_time;

    // Clients created before should be appeared earlier.
    return a.create_time < b.create_time;
  }
};

void DidGetClients(const ClientsCallback& callback,
                   std::unique_ptr<ServiceWorkerClients> clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::sort(clients->begin(), clients->end(), ServiceWorkerClientInfoSort());

  callback.Run(std::move(clients));
}

void GetNonWindowClients(const base::WeakPtr<ServiceWorkerVersion>& controller,
                         const ServiceWorkerClientQueryOptions& options,
                         const ClientsCallback& callback,
                         std::unique_ptr<ServiceWorkerClients> clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!options.include_uncontrolled) {
    for (auto& controllee : controller->controllee_map())
      AddNonWindowClient(controllee.second, options, clients.get());
  } else if (controller->context()) {
    GURL origin = controller->script_url().GetOrigin();
    for (auto it = controller->context()->GetClientProviderHostIterator(origin);
         !it->IsAtEnd(); it->Advance()) {
      AddNonWindowClient(it->GetProviderHost(), options, clients.get());
    }
  }
  DidGetClients(callback, std::move(clients));
}

void DidGetWindowClients(const base::WeakPtr<ServiceWorkerVersion>& controller,
                         const ServiceWorkerClientQueryOptions& options,
                         const ClientsCallback& callback,
                         std::unique_ptr<ServiceWorkerClients> clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (options.client_type == blink::kWebServiceWorkerClientTypeAll) {
    GetNonWindowClients(controller, options, callback, std::move(clients));
    return;
  }
  DidGetClients(callback, std::move(clients));
}

void GetWindowClients(const base::WeakPtr<ServiceWorkerVersion>& controller,
                      const ServiceWorkerClientQueryOptions& options,
                      const ClientsCallback& callback,
                      std::unique_ptr<ServiceWorkerClients> clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(options.client_type == blink::kWebServiceWorkerClientTypeWindow ||
         options.client_type == blink::kWebServiceWorkerClientTypeAll);

  std::vector<std::tuple<int, int, base::TimeTicks, std::string>> clients_info;
  if (!options.include_uncontrolled) {
    for (auto& controllee : controller->controllee_map())
      AddWindowClient(controllee.second, &clients_info);
  } else if (controller->context()) {
    GURL origin = controller->script_url().GetOrigin();
    for (auto it = controller->context()->GetClientProviderHostIterator(origin);
         !it->IsAtEnd(); it->Advance()) {
      AddWindowClient(it->GetProviderHost(), &clients_info);
    }
  }

  if (clients_info.empty()) {
    DidGetWindowClients(controller, options, callback, std::move(clients));
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(
          &OnGetWindowClientsOnUI, clients_info, controller->script_url(),
          base::Bind(&DidGetWindowClients, controller, options, callback),
          base::Passed(&clients)));
}

}  // namespace

void FocusWindowClient(ServiceWorkerProviderHost* provider_host,
                       const ClientCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(blink::kWebServiceWorkerClientTypeWindow,
            provider_host->client_type());
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&FocusOnUI, provider_host->process_id(),
                 provider_host->frame_id(), provider_host->create_time(),
                 provider_host->client_uuid()),
      callback);
}

void OpenWindow(const GURL& url,
                const GURL& script_url,
                int worker_process_id,
                const base::WeakPtr<ServiceWorkerContextCore>& context,
                WindowOpenDisposition disposition,
                const NavigationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(
          &OpenWindowOnUI, url, script_url, worker_process_id,
          base::WrapRefCounted(context->wrapper()), disposition,
          base::Bind(&DidNavigate, context, script_url.GetOrigin(), callback)));
}

void NavigateClient(const GURL& url,
                    const GURL& script_url,
                    int process_id,
                    int frame_id,
                    const base::WeakPtr<ServiceWorkerContextCore>& context,
                    const NavigationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(
          &NavigateClientOnUI, url, script_url, process_id, frame_id,
          base::Bind(&DidNavigate, context, script_url.GetOrigin(), callback)));
}

void GetClient(ServiceWorkerProviderHost* provider_host,
               const ClientCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  blink::WebServiceWorkerClientType client_type = provider_host->client_type();
  DCHECK(client_type == blink::kWebServiceWorkerClientTypeWindow ||
         client_type == blink::kWebServiceWorkerClientTypeWorker ||
         client_type == blink::kWebServiceWorkerClientTypeSharedWorker)
      << client_type;

  if (client_type == blink::kWebServiceWorkerClientTypeWindow) {
    BrowserThread::PostTaskAndReplyWithResult(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&GetWindowClientInfoOnUI, provider_host->process_id(),
                   provider_host->route_id(), provider_host->create_time(),
                   provider_host->client_uuid()),
        callback);
    return;
  }

  ServiceWorkerClientInfo client_info(
      provider_host->client_uuid(), blink::kWebPageVisibilityStateHidden,
      false,  // is_focused
      provider_host->document_url(), REQUEST_CONTEXT_FRAME_TYPE_NONE,
      base::TimeTicks(), provider_host->create_time(),
      provider_host->client_type());
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::BindOnce(callback, client_info));
}

void GetClients(const base::WeakPtr<ServiceWorkerVersion>& controller,
                const ServiceWorkerClientQueryOptions& options,
                const ClientsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto clients = base::MakeUnique<ServiceWorkerClients>();
  if (!controller->HasControllee() && !options.include_uncontrolled) {
    DidGetClients(callback, std::move(clients));
    return;
  }

  // For Window clients we want to query the info on the UI thread first.
  if (options.client_type == blink::kWebServiceWorkerClientTypeWindow ||
      options.client_type == blink::kWebServiceWorkerClientTypeAll) {
    GetWindowClients(controller, options, callback, std::move(clients));
    return;
  }

  GetNonWindowClients(controller, options, callback, std::move(clients));
}

}  // namespace service_worker_client_utils
}  // namespace content
