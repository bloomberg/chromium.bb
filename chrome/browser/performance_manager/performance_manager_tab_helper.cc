// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/performance_manager_tab_helper.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/stl_util.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/performance_manager.h"
#include "chrome/browser/performance_manager/render_process_user_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager {

PerformanceManagerTabHelper* PerformanceManagerTabHelper::first_ = nullptr;

// static
bool PerformanceManagerTabHelper::GetCoordinationIDForWebContents(
    content::WebContents* web_contents,
    resource_coordinator::CoordinationUnitID* id) {
  PerformanceManagerTabHelper* helper = FromWebContents(web_contents);
  if (!helper)
    return false;
  *id = helper->page_node_->id();

  return true;
}

// static
void PerformanceManagerTabHelper::DetachAndDestroyAll() {
  while (first_)
    first_->web_contents()->RemoveUserData(UserDataKey());
}

PerformanceManagerTabHelper::PerformanceManagerTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      performance_manager_(PerformanceManager::GetInstance()),
      page_node_(performance_manager_->CreatePageNode()) {
  // Make sure to set the visibility property when we create
  // |page_resource_coordinator_|.
  UpdatePageNodeVisibility(web_contents->GetVisibility());

  // Dispatch creation notifications for any pre-existing frames.
  // This seems to occur only in tests, but dealing with this allows asserting
  // a strong invariant on the |frames_| collection.
  // TODO(siggi): Eliminate this once test injection is all or nothing.
  std::vector<content::RenderFrameHost*> existing_frames =
      web_contents->GetAllFrames();
  for (content::RenderFrameHost* frame : existing_frames) {
    // Only send notifications for live frames, the non-live ones will generate
    // creation notifications when animated.
    if (frame->IsRenderFrameLive())
      RenderFrameCreated(frame);
  }

  // Push this instance to the list.
  next_ = first_;
  if (next_)
    next_->prev_ = this;
  prev_ = nullptr;
  first_ = this;
}

PerformanceManagerTabHelper::~PerformanceManagerTabHelper() {
  performance_manager_->DeleteNode(std::move(page_node_));
  for (auto& kv : frames_)
    performance_manager_->DeleteNode(std::move(kv.second));

  if (first_ == this)
    first_ = next_;

  if (prev_) {
    DCHECK_EQ(prev_->next_, this);
    prev_->next_ = next_;
  }
  if (next_) {
    DCHECK_EQ(next_->prev_, this);
    next_->prev_ = prev_;
  }
}

void PerformanceManagerTabHelper::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_NE(nullptr, render_frame_host);
  // This must not exist in the map yet.
  DCHECK(!base::ContainsKey(frames_, render_frame_host));

  std::unique_ptr<FrameNodeImpl> frame =
      performance_manager_->CreateFrameNode();
  content::RenderFrameHost* parent = render_frame_host->GetParent();
  if (parent) {
    DCHECK(base::ContainsKey(frames_, parent));
    auto& parent_frame_node = frames_[parent];
    performance_manager_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&FrameNodeImpl::AddChildFrame,
                       base::Unretained(parent_frame_node.get()), frame->id()));
  }

  RenderProcessUserData* user_data =
      RenderProcessUserData::GetForRenderProcessHost(
          render_frame_host->GetProcess());
  // In unittests the user data isn't populated as the relevant main parts
  // is not in play.
  // TODO(siggi): Figure out how to assert on this when the main parts are
  //     registered with the content browser client.
  if (user_data) {
    performance_manager_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&FrameNodeImpl::SetProcess,
                                  base::Unretained(frame.get()),
                                  user_data->process_node()->id()));
  }

  frames_[render_frame_host] = std::move(frame);
}

void PerformanceManagerTabHelper::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  auto it = frames_.find(render_frame_host);
  DCHECK(it != frames_.end());

  performance_manager_->DeleteNode(std::move(it->second));
  frames_.erase(it);
}

void PerformanceManagerTabHelper::DidStartLoading() {
  performance_manager_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&PageNodeImpl::SetIsLoading,
                                base::Unretained(page_node_.get()), true));
}

void PerformanceManagerTabHelper::DidStopLoading() {
  performance_manager_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&PageNodeImpl::SetIsLoading,
                                base::Unretained(page_node_.get()), false));
}

void PerformanceManagerTabHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  UpdatePageNodeVisibility(visibility);
}

void PerformanceManagerTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Grab the current time up front, as this is as close as we'll get to the
  // original commit time.
  base::TimeTicks navigation_committed_time = base::TimeTicks::Now();

  content::RenderFrameHost* render_frame_host =
      navigation_handle->GetRenderFrameHost();
  // Make sure the hierarchical structure is constructed before sending signal
  // to Resource Coordinator.
  // TODO(siggi): Ideally this would be a DCHECK, but it seems it's possible
  //     to get a DidFinishNavigation notification for a deleted frame with
  //     the network service.
  auto it = frames_.find(render_frame_host);
  if (it != frames_.end()) {
    // TODO(siggi): See whether this can be done in RenderFrameCreated.
    performance_manager_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&PageNodeImpl::AddFrame,
                       base::Unretained(page_node_.get()), it->second->id()));

    if (navigation_handle->IsInMainFrame()) {
      OnMainFrameNavigation(navigation_handle->GetNavigationId());
      performance_manager_->task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(&PageNodeImpl::OnMainFrameNavigationCommitted,
                         base::Unretained(page_node_.get()),
                         navigation_committed_time,
                         navigation_handle->GetNavigationId(),
                         navigation_handle->GetURL().spec()));
    }
  }
}

void PerformanceManagerTabHelper::TitleWasSet(content::NavigationEntry* entry) {
  // TODO(siggi): This logic belongs in the policy layer rather than here.
  if (!first_time_title_set_) {
    first_time_title_set_ = true;
    return;
  }
  performance_manager_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&PageNodeImpl::OnTitleUpdated,
                                base::Unretained(page_node_.get())));
}

void PerformanceManagerTabHelper::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  // TODO(siggi): This logic belongs in the policy layer rather than here.
  if (!first_time_favicon_set_) {
    first_time_favicon_set_ = true;
    return;
  }
  performance_manager_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&PageNodeImpl::OnFaviconUpdated,
                                base::Unretained(page_node_.get())));
}

void PerformanceManagerTabHelper::OnInterfaceRequestFromFrame(
    content::RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  if (interface_name !=
      resource_coordinator::mojom::FrameCoordinationUnit::Name_)
    return;

  auto it = frames_.find(render_frame_host);
  DCHECK(it != frames_.end());
  performance_manager_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&FrameNodeImpl::AddBinding,
                     base::Unretained(it->second.get()),
                     resource_coordinator::mojom::FrameCoordinationUnitRequest(
                         std::move(*interface_pipe))));
}

void PerformanceManagerTabHelper::OnMainFrameNavigation(int64_t navigation_id) {
  ukm_source_id_ =
      ukm::ConvertToSourceId(navigation_id, ukm::SourceIdType::NAVIGATION_ID);
  performance_manager_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&PageNodeImpl::SetUKMSourceId,
                     base::Unretained(page_node_.get()), ukm_source_id_));

  first_time_title_set_ = false;
  first_time_favicon_set_ = false;
}

void PerformanceManagerTabHelper::UpdatePageNodeVisibility(
    content::Visibility visibility) {
  // TODO(fdoray): An OCCLUDED tab should not be considered visible.
  const bool is_visible = visibility != content::Visibility::HIDDEN;
  performance_manager_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&PageNodeImpl::SetVisibility,
                     base::Unretained(page_node_.get()), is_visible));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PerformanceManagerTabHelper)

}  // namespace performance_manager
