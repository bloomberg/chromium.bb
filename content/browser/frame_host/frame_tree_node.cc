// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_tree_node.h"

#include <queue>
#include <utility>

#include "base/macros.h"
#include "base/profiler/scoped_tracker.h"
#include "base/stl_util.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/site_isolation_policy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/browser_side_navigation_policy.h"

namespace content {

namespace {

// This is a global map between frame_tree_node_ids and pointers to
// FrameTreeNodes.
typedef base::hash_map<int, FrameTreeNode*> FrameTreeNodeIdMap;

base::LazyInstance<FrameTreeNodeIdMap> g_frame_tree_node_id_map =
    LAZY_INSTANCE_INITIALIZER;

// These values indicate the loading progress status. The minimum progress
// value matches what Blink's ProgressTracker has traditionally used for a
// minimum progress value.
const double kLoadingProgressNotStarted = 0.0;
const double kLoadingProgressMinimum = 0.1;
const double kLoadingProgressDone = 1.0;

}  // namespace

// This observer watches the opener of its owner FrameTreeNode and clears the
// owner's opener if the opener is destroyed.
class FrameTreeNode::OpenerDestroyedObserver : public FrameTreeNode::Observer {
 public:
  OpenerDestroyedObserver(FrameTreeNode* owner) : owner_(owner) {}

  // FrameTreeNode::Observer
  void OnFrameTreeNodeDestroyed(FrameTreeNode* node) override {
    CHECK_EQ(owner_->opener(), node);
    owner_->SetOpener(nullptr);
  }

 private:
  FrameTreeNode* owner_;

  DISALLOW_COPY_AND_ASSIGN(OpenerDestroyedObserver);
};

int FrameTreeNode::next_frame_tree_node_id_ = 1;

// static
FrameTreeNode* FrameTreeNode::GloballyFindByID(int frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FrameTreeNodeIdMap* nodes = g_frame_tree_node_id_map.Pointer();
  FrameTreeNodeIdMap::iterator it = nodes->find(frame_tree_node_id);
  return it == nodes->end() ? nullptr : it->second;
}

FrameTreeNode::FrameTreeNode(
    FrameTree* frame_tree,
    Navigator* navigator,
    RenderFrameHostDelegate* render_frame_delegate,
    RenderViewHostDelegate* render_view_delegate,
    RenderWidgetHostDelegate* render_widget_delegate,
    RenderFrameHostManager::Delegate* manager_delegate,
    blink::WebTreeScopeType scope,
    const std::string& name,
    blink::WebSandboxFlags sandbox_flags,
    const blink::WebFrameOwnerProperties& frame_owner_properties)
    : frame_tree_(frame_tree),
      navigator_(navigator),
      render_manager_(this,
                      render_frame_delegate,
                      render_view_delegate,
                      render_widget_delegate,
                      manager_delegate),
      frame_tree_node_id_(next_frame_tree_node_id_++),
      parent_(NULL),
      opener_(nullptr),
      opener_observer_(nullptr),
      has_committed_real_load_(false),
      replication_state_(
          scope,
          name,
          sandbox_flags,
          false /* should enforce strict mixed content checking */),
      // Effective sandbox flags also need to be set, since initial sandbox
      // flags should apply to the initial empty document in the frame.
      effective_sandbox_flags_(sandbox_flags),
      frame_owner_properties_(frame_owner_properties),
      loading_progress_(kLoadingProgressNotStarted) {
  std::pair<FrameTreeNodeIdMap::iterator, bool> result =
      g_frame_tree_node_id_map.Get().insert(
          std::make_pair(frame_tree_node_id_, this));
  CHECK(result.second);
}

FrameTreeNode::~FrameTreeNode() {
  children_.clear();
  frame_tree_->FrameRemoved(this);
  FOR_EACH_OBSERVER(Observer, observers_, OnFrameTreeNodeDestroyed(this));

  if (opener_)
    opener_->RemoveObserver(opener_observer_.get());

  g_frame_tree_node_id_map.Get().erase(frame_tree_node_id_);
}

void FrameTreeNode::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FrameTreeNode::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FrameTreeNode::IsMainFrame() const {
  return frame_tree_->root() == this;
}

FrameTreeNode* FrameTreeNode::AddChild(scoped_ptr<FrameTreeNode> child,
                                       int process_id,
                                       int frame_routing_id) {
  // Child frame must always be created in the same process as the parent.
  CHECK_EQ(process_id, render_manager_.current_host()->GetProcess()->GetID());
  child->set_parent(this);

  // Initialize the RenderFrameHost for the new node.  We always create child
  // frames in the same SiteInstance as the current frame, and they can swap to
  // a different one if they navigate away.
  child->render_manager()->Init(
      render_manager_.current_host()->GetSiteInstance(),
      render_manager_.current_host()->GetRoutingID(), frame_routing_id,
      MSG_ROUTING_NONE);

  // Other renderer processes in this BrowsingInstance may need to find out
  // about the new frame.  Create a proxy for the child frame in all
  // SiteInstances that have a proxy for the frame's parent, since all frames
  // in a frame tree should have the same set of proxies.
  // TODO(alexmos, nick): We ought to do this for non-oopif too, for openers.
  if (SiteIsolationPolicy::AreCrossProcessFramesPossible())
    render_manager_.CreateProxiesForChildFrame(child.get());

  children_.push_back(std::move(child));
  return children_.back().get();
}

void FrameTreeNode::RemoveChild(FrameTreeNode* child) {
  for (auto iter = children_.begin(); iter != children_.end(); ++iter) {
    if (iter->get() == child) {
      // Subtle: we need to make sure the node is gone from the tree before
      // observers are notified of its deletion.
      scoped_ptr<FrameTreeNode> node_to_delete(std::move(*iter));
      children_.erase(iter);
      node_to_delete.reset();
      return;
    }
  }
}

void FrameTreeNode::ResetForNewProcess() {
  current_url_ = GURL();

  // Remove child nodes from the tree, then delete them. This destruction
  // operation will notify observers.
  std::vector<scoped_ptr<FrameTreeNode>>().swap(children_);
}

void FrameTreeNode::SetOpener(FrameTreeNode* opener) {
  if (opener_) {
    opener_->RemoveObserver(opener_observer_.get());
    opener_observer_.reset();
  }

  opener_ = opener;

  if (opener_) {
    if (!opener_observer_)
      opener_observer_ = make_scoped_ptr(new OpenerDestroyedObserver(this));
    opener_->AddObserver(opener_observer_.get());
  }
}

void FrameTreeNode::SetCurrentURL(const GURL& url) {
  if (!has_committed_real_load_ && url != GURL(url::kAboutBlankURL))
    has_committed_real_load_ = true;
  current_url_ = url;
}

void FrameTreeNode::SetCurrentOrigin(const url::Origin& origin) {
  if (!origin.IsSameOriginWith(replication_state_.origin))
    render_manager_.OnDidUpdateOrigin(origin);
  replication_state_.origin = origin;
}

void FrameTreeNode::SetFrameName(const std::string& name) {
  if (name != replication_state_.name)
    render_manager_.OnDidUpdateName(name);
  replication_state_.name = name;
}

void FrameTreeNode::SetEnforceStrictMixedContentChecking(bool should_enforce) {
  if (should_enforce ==
      replication_state_.should_enforce_strict_mixed_content_checking) {
    return;
  }
  render_manager_.OnEnforceStrictMixedContentChecking(should_enforce);
  replication_state_.should_enforce_strict_mixed_content_checking =
      should_enforce;
}

bool FrameTreeNode::IsDescendantOf(FrameTreeNode* other) const {
  if (!other || !other->child_count())
    return false;

  for (FrameTreeNode* node = parent(); node; node = node->parent()) {
    if (node == other)
      return true;
  }

  return false;
}

FrameTreeNode* FrameTreeNode::PreviousSibling() const {
  if (!parent_)
    return nullptr;

  for (size_t i = 0; i < parent_->child_count(); ++i) {
    if (parent_->child_at(i) == this)
      return (i == 0) ? nullptr : parent_->child_at(i - 1);
  }

  NOTREACHED() << "FrameTreeNode not found in its parent's children.";
  return nullptr;
}

bool FrameTreeNode::IsLoading() const {
  RenderFrameHostImpl* current_frame_host =
      render_manager_.current_frame_host();
  RenderFrameHostImpl* pending_frame_host =
      render_manager_.pending_frame_host();

  DCHECK(current_frame_host);

  if (IsBrowserSideNavigationEnabled()) {
    if (navigation_request_)
      return true;

    RenderFrameHostImpl* speculative_frame_host =
        render_manager_.speculative_frame_host();
    if (speculative_frame_host && speculative_frame_host->is_loading())
      return true;
  } else {
    if (pending_frame_host && pending_frame_host->is_loading())
      return true;
  }
  return current_frame_host->is_loading();
}

bool FrameTreeNode::CommitPendingSandboxFlags() {
  bool did_change_flags =
      effective_sandbox_flags_ != replication_state_.sandbox_flags;
  effective_sandbox_flags_ = replication_state_.sandbox_flags;
  return did_change_flags;
}

void FrameTreeNode::CreatedNavigationRequest(
    scoped_ptr<NavigationRequest> navigation_request) {
  CHECK(IsBrowserSideNavigationEnabled());
  ResetNavigationRequest(false);

  // Force the throbber to start to keep it in sync with what is happening in
  // the UI. Blink doesn't send throb notifications for JavaScript URLs, so it
  // is not done here either.
  if (!navigation_request->common_params().url.SchemeIs(
          url::kJavaScriptScheme)) {
    // TODO(fdegans): Check if this is a same-document navigation and set the
    // proper argument.
    DidStartLoading(true);
  }

  navigation_request_ = std::move(navigation_request);

  render_manager()->DidCreateNavigationRequest(*navigation_request_);
}

void FrameTreeNode::ResetNavigationRequest(bool is_commit) {
  CHECK(IsBrowserSideNavigationEnabled());
  if (!navigation_request_)
    return;
  navigation_request_.reset();

  // During commit, the clean up of a speculative RenderFrameHost is done in
  // RenderFrameHostManager::DidNavigateFrame. The load is also still being
  // tracked.
  if (is_commit)
    return;

  // If the reset corresponds to a cancelation, the RenderFrameHostManager
  // should clean up any speculative RenderFrameHost it created for the
  // navigation.
  DidStopLoading();
  render_manager_.CleanUpNavigation();
}

bool FrameTreeNode::has_started_loading() const {
  return loading_progress_ != kLoadingProgressNotStarted;
}

void FrameTreeNode::reset_loading_progress() {
  loading_progress_ = kLoadingProgressNotStarted;
}

void FrameTreeNode::DidStartLoading(bool to_different_document) {
  // Any main frame load to a new document should reset the load progress since
  // it will replace the current page and any frames. The WebContents will
  // be notified when DidChangeLoadProgress is called.
  if (to_different_document && IsMainFrame())
    frame_tree_->ResetLoadProgress();

  // Notify the WebContents.
  if (!frame_tree_->IsLoading())
    navigator()->GetDelegate()->DidStartLoading(this, to_different_document);

  // Set initial load progress and update overall progress. This will notify
  // the WebContents of the load progress change.
  DidChangeLoadProgress(kLoadingProgressMinimum);

  // Notify the RenderFrameHostManager of the event.
  render_manager()->OnDidStartLoading();
}

void FrameTreeNode::DidStopLoading() {
  // TODO(erikchen): Remove ScopedTracker below once crbug.com/465796 is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "465796 FrameTreeNode::DidStopLoading::Start"));

  // Set final load progress and update overall progress. This will notify
  // the WebContents of the load progress change.
  DidChangeLoadProgress(kLoadingProgressDone);

  // TODO(erikchen): Remove ScopedTracker below once crbug.com/465796 is fixed.
  tracked_objects::ScopedTracker tracking_profile2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "465796 FrameTreeNode::DidStopLoading::WCIDidStopLoading"));

  // Notify the WebContents.
  if (!frame_tree_->IsLoading())
    navigator()->GetDelegate()->DidStopLoading();

  // TODO(erikchen): Remove ScopedTracker below once crbug.com/465796 is fixed.
  tracked_objects::ScopedTracker tracking_profile3(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "465796 FrameTreeNode::DidStopLoading::RFHMDidStopLoading"));

  // Notify the RenderFrameHostManager of the event.
  render_manager()->OnDidStopLoading();

  // TODO(erikchen): Remove ScopedTracker below once crbug.com/465796 is fixed.
  tracked_objects::ScopedTracker tracking_profile4(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "465796 FrameTreeNode::DidStopLoading::End"));
}

void FrameTreeNode::DidChangeLoadProgress(double load_progress) {
  loading_progress_ = load_progress;
  frame_tree_->UpdateLoadProgress();
}

bool FrameTreeNode::StopLoading() {
  if (IsBrowserSideNavigationEnabled())
    ResetNavigationRequest(false);

  // TODO(nasko): see if child frames should send IPCs in site-per-process
  // mode.
  if (!IsMainFrame())
    return true;

  render_manager_.Stop();
  return true;
}

void FrameTreeNode::DidFocus() {
  last_focus_time_ = base::TimeTicks::Now();
  FOR_EACH_OBSERVER(Observer, observers_, OnFrameTreeNodeFocused(this));
}

}  // namespace content
