// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_client_navigation_utils.h"

#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/service_worker/service_worker_client_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/child_process_host.h"
#include "url/gurl.h"

namespace content {
namespace service_worker_client_navigation_utils {

namespace {

using OpenURLCallback = base::Callback<void(int, int)>;

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

  void DidCommitProvisionalLoadForFrame(
      RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      ui::PageTransition transition_type) override {
    DCHECK(web_contents());

    RenderFrameHostImpl* rfhi =
        static_cast<RenderFrameHostImpl*>(render_frame_host);
    if (rfhi->frame_tree_node()->frame_tree_node_id() != frame_tree_node_id_)
      return;

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
        base::Bind(callback_, render_process_id, render_frame_id));
    Observe(nullptr);
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }

  int frame_tree_node_id_;
  const OpenURLCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(OpenURLObserver);
};

// This is only called for main frame navigations in OpenWindowOnUI().
void DidOpenURL(const OpenURLCallback& callback, WebContents* web_contents) {
  DCHECK(web_contents);

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
        base::Bind(callback, ChildProcessHost::kInvalidUniqueID,
                   MSG_ROUTING_NONE));
    return;
  }

  OpenURLParams params(
      url, Referrer::SanitizeForRequest(
               url, Referrer(script_url, blink::WebReferrerPolicyDefault)),
      NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      true /* is_renderer_initiated */);

  GetContentClient()->browser()->OpenURL(browser_context, params,
                                         base::Bind(&DidOpenURL, callback));
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
        base::Bind(callback, ChildProcessHost::kInvalidUniqueID,
                   MSG_ROUTING_NONE));
    return;
  }

  ui::PageTransition transition = rfhi->GetParent()
                                      ? ui::PAGE_TRANSITION_AUTO_SUBFRAME
                                      : ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  int frame_tree_node_id = rfhi->frame_tree_node()->frame_tree_node_id();

  OpenURLParams params(
      url, Referrer::SanitizeForRequest(
               url, Referrer(script_url, blink::WebReferrerPolicyDefault)),
      frame_tree_node_id, CURRENT_TAB, transition,
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
    callback.Run(SERVICE_WORKER_ERROR_ABORT, std::string(),
                 ServiceWorkerClientInfo());
    return;
  }

  if (render_process_id == ChildProcessHost::kInvalidUniqueID &&
      render_frame_id == MSG_ROUTING_NONE) {
    callback.Run(SERVICE_WORKER_ERROR_FAILED, std::string(),
                 ServiceWorkerClientInfo());
    return;
  }

  for (scoped_ptr<ServiceWorkerContextCore::ProviderHostIterator> it =
           context->GetClientProviderHostIterator(origin);
       !it->IsAtEnd(); it->Advance()) {
    ServiceWorkerProviderHost* provider_host = it->GetProviderHost();
    if (provider_host->process_id() != render_process_id ||
        provider_host->frame_id() != render_frame_id) {
      continue;
    }
    provider_host->GetWindowClientInfo(
        base::Bind(callback, SERVICE_WORKER_OK, provider_host->client_uuid()));
    return;
  }

  // If here, it means that no provider_host was found, in which case, the
  // renderer should still be informed that the window was opened.
  callback.Run(SERVICE_WORKER_OK, std::string(), ServiceWorkerClientInfo());
}

}  // namespace

void OpenWindow(const GURL& url,
                const GURL& script_url,
                int worker_process_id,
                const base::WeakPtr<ServiceWorkerContextCore>& context,
                const NavigationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &OpenWindowOnUI, url, script_url, worker_process_id,
          make_scoped_refptr(context->wrapper()),
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
      base::Bind(
          &NavigateClientOnUI, url, script_url, process_id, frame_id,
          base::Bind(&DidNavigate, context, script_url.GetOrigin(), callback)));
}

}  // namespace service_worker_client_navigation_utils
}  // namespace content
