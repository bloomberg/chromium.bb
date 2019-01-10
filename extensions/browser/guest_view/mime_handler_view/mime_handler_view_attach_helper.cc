// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_attach_helper.h"

#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/common/guest_view/extensions_guest_view_messages.h"

using content::BrowserThread;
using content::NavigationHandle;
using content::NavigationThrottle;
using content::RenderFrameHost;
using content::SiteInstance;

namespace extensions {

namespace {
// Arbitrary delay to quit attaching the MimeHandlerViewGuest's WebContents to
// the outer WebContents if no about:blank navigation is committed. The reason
// for this delay is to allow user to decide on the outcome of 'beforeunload'.
const int64_t kAttachFailureDelayMS = 30000;

// Cancels the given navigation handle unconditionally.
class CancelAndIgnoreNavigationForPluginFrameThrottle
    : public NavigationThrottle {
 public:
  explicit CancelAndIgnoreNavigationForPluginFrameThrottle(
      NavigationHandle* handle)
      : NavigationThrottle(handle) {}
  ~CancelAndIgnoreNavigationForPluginFrameThrottle() override {}

  const char* GetNameForLogging() override {
    return "CancelAndIgnoreNavigationForPluginFrameThrottle";
  }
  ThrottleCheckResult WillStartRequest() override { return CANCEL_AND_IGNORE; }
  ThrottleCheckResult WillProcessResponse() override { return BLOCK_RESPONSE; }
};

using ProcessIdToHelperMap =
    base::flat_map<int32_t, std::unique_ptr<MimeHandlerViewAttachHelper>>;
ProcessIdToHelperMap* GetProcessIdToHelperMap() {
  static base::NoDestructor<ProcessIdToHelperMap> instance;
  return instance.get();
}

}  // namespace

// Helper class which navigates a given FrameTreeNode to "about:blank". This is
// used for scenarios where the plugin element's content frame has a different
// SiteInstance from its parent frame, or, the frame's origin is not
// "about:blank". Since this class triggers a navigation, all the document
// unload events will be dispatched and handled. During the lifetime of this
// helper class, all other navigations for the corresponding FrameTreeNode will
// be throttled and ignored.
class MimeHandlerViewAttachHelper::FrameNavigationHelper
    : public content::WebContentsObserver {
 public:
  FrameNavigationHelper(RenderFrameHost* plugin_rfh,
                        int32_t guest_instance_id,
                        int32_t element_instance_id,
                        bool is_full_page_plugin,
                        base::WeakPtr<MimeHandlerViewAttachHelper> helper);
  ~FrameNavigationHelper() override;

  void FrameDeleted(RenderFrameHost* render_frame_host) override;
  void DidFinishNavigation(NavigationHandle* handle) override;
  // During attaching, we should ignore any navigation which is not a navigation
  // to "about:blank" from the parent frame's SiteInstance.
  bool ShouldCancelAndIgnore(NavigationHandle* handle);

  MimeHandlerViewGuest* GetGuestView() const;

  int32_t guest_instance_id() const { return guest_instance_id_; }
  bool is_full_page_plugin() const { return is_full_page_plugin_; }
  SiteInstance* parent_site_instance() const {
    return parent_site_instance_.get();
  }

 private:
  void NavigateToAboutBlank();
  void CancelPendingTask();

  int32_t frame_tree_node_id_;
  const int32_t guest_instance_id_;
  const int32_t element_instance_id_;
  const bool is_full_page_plugin_;
  base::WeakPtr<MimeHandlerViewAttachHelper> helper_;
  scoped_refptr<SiteInstance> parent_site_instance_;

  base::WeakPtrFactory<FrameNavigationHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FrameNavigationHelper);
};

MimeHandlerViewAttachHelper::FrameNavigationHelper::FrameNavigationHelper(
    RenderFrameHost* plugin_rfh,
    int32_t guest_instance_id,
    int32_t element_instance_id,
    bool is_full_page_plugin,
    base::WeakPtr<MimeHandlerViewAttachHelper> helper)
    : content::WebContentsObserver(
          content::WebContents::FromRenderFrameHost(plugin_rfh)),
      frame_tree_node_id_(plugin_rfh->GetFrameTreeNodeId()),
      guest_instance_id_(guest_instance_id),
      element_instance_id_(element_instance_id),
      is_full_page_plugin_(is_full_page_plugin),
      helper_(helper),
      parent_site_instance_(plugin_rfh->GetParent()->GetSiteInstance()),
      weak_factory_(this) {
  DCHECK(GetGuestView());
  NavigateToAboutBlank();
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&MimeHandlerViewAttachHelper::FrameNavigationHelper::
                         CancelPendingTask,
                     weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kAttachFailureDelayMS));
}

MimeHandlerViewAttachHelper::FrameNavigationHelper::~FrameNavigationHelper() {}

void MimeHandlerViewAttachHelper::FrameNavigationHelper::FrameDeleted(
    RenderFrameHost* render_frame_host) {
  if (render_frame_host->GetFrameTreeNodeId() != frame_tree_node_id_)
    return;
  // It is possible that the plugin frame is deleted before a NavigationHandle
  // is created; one such case is to immediately delete the plugin element right
  // after MimeHandlerViewFrameContainer requests to create the
  // MimeHandlerViewGuest on the browser side.
  helper_->ResumeAttachOrDestroy(element_instance_id_,
                                 MSG_ROUTING_NONE /* no plugin frame */);
}

void MimeHandlerViewAttachHelper::FrameNavigationHelper::DidFinishNavigation(
    NavigationHandle* handle) {
  if (handle->GetFrameTreeNodeId() != frame_tree_node_id_)
    return;
  if (!handle->HasCommitted())
    return;
  if (handle->GetRenderFrameHost()->GetSiteInstance() != parent_site_instance_)
    return;
  if (!handle->GetURL().IsAboutBlank())
    return;
  if (!handle->GetRenderFrameHost()->PrepareForInnerWebContentsAttach()) {
    helper_->ResumeAttachOrDestroy(element_instance_id_,
                                   MSG_ROUTING_NONE /* no plugin frame */);
  }
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&MimeHandlerViewAttachHelper::ResumeAttachOrDestroy,
                     helper_, element_instance_id_,
                     handle->GetRenderFrameHost()->GetRoutingID()));
}

bool MimeHandlerViewAttachHelper::FrameNavigationHelper::ShouldCancelAndIgnore(
    NavigationHandle* handle) {
  return handle->GetFrameTreeNodeId() == frame_tree_node_id_;
}

void MimeHandlerViewAttachHelper::FrameNavigationHelper::
    NavigateToAboutBlank() {
  // Immediately start a navigation to "about:blank".
  GURL about_blank(url::kAboutBlankURL);
  content::NavigationController::LoadURLParams params(about_blank);
  params.frame_tree_node_id = frame_tree_node_id_;
  // The goal is to have a plugin frame which is same-origin with parent, i.e.,
  // 'about:blank' and share the same SiteInstance.
  params.source_site_instance = parent_site_instance_;
  // The renderer (parent of the plugin frame) tries to load a MimeHandlerView
  // and therefore this navigation should be treated as renderer initiated.
  params.is_renderer_initiated = true;
  params.initiator_origin =
      url::Origin::Create(parent_site_instance_->GetSiteURL());
  web_contents()->GetController().LoadURLWithParams(params);
}

void MimeHandlerViewAttachHelper::FrameNavigationHelper::CancelPendingTask() {
  helper_->ResumeAttachOrDestroy(element_instance_id_,
                                 MSG_ROUTING_NONE /* no plugin frame */);
}

MimeHandlerViewGuest*
MimeHandlerViewAttachHelper::FrameNavigationHelper::GetGuestView() const {
  return MimeHandlerViewGuest::From(
             parent_site_instance_->GetProcess()->GetID(), guest_instance_id_)
      ->As<MimeHandlerViewGuest>();
}

MimeHandlerViewAttachHelper* MimeHandlerViewAttachHelper::Get(
    int32_t render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto& map = *GetProcessIdToHelperMap();
  if (!base::ContainsKey(map, render_process_id)) {
    auto* process_host = content::RenderProcessHost::FromID(render_process_id);
    if (!process_host)
      return nullptr;
    map[render_process_id] = base::WrapUnique<MimeHandlerViewAttachHelper>(
        new MimeHandlerViewAttachHelper(process_host));
  }
  return map[render_process_id].get();
}

std::unique_ptr<content::NavigationThrottle>
MimeHandlerViewAttachHelper::MaybeCreateThrottle(
    content::NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!handle->GetParentFrame()) {
    // A plugin element cannot be the FrameOwner to a main frame.
    return nullptr;
  }
  int32_t parent_process_id = handle->GetParentFrame()->GetProcess()->GetID();
  auto& map = *GetProcessIdToHelperMap();
  if (!base::ContainsKey(map, parent_process_id) || !map[parent_process_id]) {
    // This happens if the RenderProcessHost has not been initialized yet.
    return nullptr;
  }
  for (auto& pair : map[parent_process_id]->frame_navigation_helpers_) {
    if (!pair.second->ShouldCancelAndIgnore(handle))
      continue;
    // Any navigation of the corresponding FrameTreeNode which is not to
    // "about:blank" or is not initiated by parent SiteInstance should be
    // ignored.
    return std::make_unique<CancelAndIgnoreNavigationForPluginFrameThrottle>(
        handle);
  }
  return nullptr;
}

void MimeHandlerViewAttachHelper::RenderProcessHostDestroyed(
    content::RenderProcessHost* render_process_host) {
  if (render_process_host != render_process_host_)
    return;
  render_process_host->RemoveObserver(this);
  GetProcessIdToHelperMap()->erase(render_process_host_->GetID());
}

void MimeHandlerViewAttachHelper::AttachToOuterWebContents(
    MimeHandlerViewGuest* guest_view,
    int32_t embedder_render_process_id,
    int32_t plugin_frame_routing_id,
    int32_t element_instance_id,
    bool is_full_page_plugin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* plugin_rfh = RenderFrameHost::FromID(embedder_render_process_id,
                                             plugin_frame_routing_id);
  if (!plugin_rfh) {
    // The plugin element has a proxy instead.
    plugin_rfh = RenderFrameHost::FromPlaceholderId(embedder_render_process_id,
                                                    plugin_frame_routing_id);
  }
  if (!plugin_rfh) {
    // This should only happen if the original plugin frame was cross-process
    // and a concurrent navigation in its process won the race and ended up
    // destroying the proxy whose routing ID was sent here by the
    // MimeHandlerViewFrameContainer. We should ask the embedder to retry
    // creating the guest.
    guest_view->GetEmbedderFrame()->Send(
        new ExtensionsGuestViewMsg_RetryCreatingMimeHandlerViewGuest(
            element_instance_id));
    guest_view->Destroy(true);
    return;
  }

  if (guest_view->web_contents()->CanAttachToOuterContentsFrame(plugin_rfh)) {
    guest_view->AttachToOuterWebContentsFrame(plugin_rfh, element_instance_id,
                                              is_full_page_plugin);

  } else {
    // TODO(ekaramad): Replace this navigation logic with an asynchronous
    // attach API in content layer (https://crbug.com/911161).
    // The current API for attaching guests requires the frame in outer
    // WebContents to be same-origin with parent. The current frame could also
    // have beforeunload handlers. Considering these issues, we should first
    // navigate the frame to "about:blank" and put it in the same SiteInstance
    // as parent before using it for attach API.
    frame_navigation_helpers_[element_instance_id] =
        std::make_unique<FrameNavigationHelper>(
            plugin_rfh, guest_view->guest_instance_id(), element_instance_id,
            is_full_page_plugin, weak_factory_.GetWeakPtr());
  }
}

MimeHandlerViewAttachHelper::MimeHandlerViewAttachHelper(
    content::RenderProcessHost* render_process_host)
    : render_process_host_(render_process_host), weak_factory_(this) {
  render_process_host->AddObserver(this);
}

MimeHandlerViewAttachHelper::~MimeHandlerViewAttachHelper() {}

void MimeHandlerViewAttachHelper::ResumeAttachOrDestroy(
    int32_t element_instance_id,
    int32_t plugin_frame_routing_id) {
  auto it = frame_navigation_helpers_.find(element_instance_id);
  if (it == frame_navigation_helpers_.end()) {
    // This is the timeout callback. The guest is either attached or destroyed.
    return;
  }
  auto* plugin_rfh = content::RenderFrameHost::FromID(
      render_process_host_->GetID(), plugin_frame_routing_id);
  auto* helper = it->second.get();
  auto* guest_view = helper->GetGuestView();
  if (!guest_view)
    return;

  if (plugin_rfh) {
    DCHECK(
        guest_view->web_contents()->CanAttachToOuterContentsFrame(plugin_rfh));
    guest_view->AttachToOuterWebContentsFrame(plugin_rfh, element_instance_id,
                                              helper->is_full_page_plugin());
  } else {
    guest_view->GetEmbedderFrame()->Send(
        new ExtensionsGuestViewMsg_DestroyFrameContainer(element_instance_id));
    guest_view->Destroy(true);
  }
  frame_navigation_helpers_.erase(element_instance_id);
}
}  // namespace extensions
