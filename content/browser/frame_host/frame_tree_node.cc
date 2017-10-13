// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_tree_node.h"

#include <math.h>

#include <queue>
#include <utility>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/site_isolation_policy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "third_party/WebKit/public/web/WebSandboxFlags.h"

namespace content {

namespace {

// This is a global map between frame_tree_node_ids and pointers to
// FrameTreeNodes.
typedef base::hash_map<int, FrameTreeNode*> FrameTreeNodeIdMap;

base::LazyInstance<FrameTreeNodeIdMap>::DestructorAtExit
    g_frame_tree_node_id_map = LAZY_INSTANCE_INITIALIZER;

// These values indicate the loading progress status. The minimum progress
// value matches what Blink's ProgressTracker has traditionally used for a
// minimum progress value.
const double kLoadingProgressNotStarted = 0.0;
const double kLoadingProgressMinimum = 0.1;
const double kLoadingProgressDone = 1.0;

void RecordUniqueNameSize(FrameTreeNode* node) {
  const auto& unique_name = node->current_replication_state().unique_name;

  // Don't record numbers for the root node, which always has an empty unique
  // name.
  if (!node->parent()) {
    DCHECK(unique_name.empty());
    return;
  }

  // The original requested name is derived from the browsing context name and
  // is essentially unbounded in size...
  UMA_HISTOGRAM_COUNTS_1M(
      "SessionRestore.FrameUniqueNameOriginalRequestedNameSize",
      node->current_replication_state().name.size());
  // If the name is a frame path, attempt to normalize the statistics based on
  // the number of frames in the frame path.
  if (base::StartsWith(unique_name, "<!--framePath //",
                       base::CompareCase::SENSITIVE)) {
    size_t depth = 1;
    while (node->parent()) {
      ++depth;
      node = node->parent();
    }
    // The max possible size of a unique name is 80 characters, so the expected
    // size per component shouldn't be much more than that.
    UMA_HISTOGRAM_COUNTS_100(
        "SessionRestore.FrameUniqueNameWithFramePathSizePerComponent",
        round(unique_name.size() / static_cast<float>(depth)));
    // Blink allows a maximum of ~1024 subframes in a document, so this should
    // be less than (80 character name + 1 character delimiter) * 1024.
    UMA_HISTOGRAM_COUNTS_100000(
        "SessionRestore.FrameUniqueNameWithFramePathSize", unique_name.size());
  } else {
    UMA_HISTOGRAM_COUNTS_100(
        "SessionRestore.FrameUniqueNameFromRequestedNameSize",
        unique_name.size());
  }
}

}  // namespace

// This observer watches the opener of its owner FrameTreeNode and clears the
// owner's opener if the opener is destroyed.
class FrameTreeNode::OpenerDestroyedObserver : public FrameTreeNode::Observer {
 public:
  OpenerDestroyedObserver(FrameTreeNode* owner, bool observing_original_opener)
      : owner_(owner), observing_original_opener_(observing_original_opener) {}

  // FrameTreeNode::Observer
  void OnFrameTreeNodeDestroyed(FrameTreeNode* node) override {
    if (observing_original_opener_) {
      // The "original owner" is special. It's used for attribution, and clients
      // walk down the original owner chain. Therefore, if a link in the chain
      // is being destroyed, reconnect the observation to the parent of the link
      // being destroyed.
      CHECK_EQ(owner_->original_opener(), node);
      owner_->SetOriginalOpener(node->original_opener());
      // |this| is deleted at this point.
    } else {
      CHECK_EQ(owner_->opener(), node);
      owner_->SetOpener(nullptr);
      // |this| is deleted at this point.
    }
  }

 private:
  FrameTreeNode* owner_;
  bool observing_original_opener_;

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

FrameTreeNode::FrameTreeNode(FrameTree* frame_tree,
                             Navigator* navigator,
                             RenderFrameHostDelegate* render_frame_delegate,
                             RenderWidgetHostDelegate* render_widget_delegate,
                             RenderFrameHostManager::Delegate* manager_delegate,
                             FrameTreeNode* parent,
                             blink::WebTreeScopeType scope,
                             const std::string& name,
                             const std::string& unique_name,
                             const base::UnguessableToken& devtools_frame_token,
                             const FrameOwnerProperties& frame_owner_properties)
    : frame_tree_(frame_tree),
      navigator_(navigator),
      render_manager_(this,
                      render_frame_delegate,
                      render_widget_delegate,
                      manager_delegate),
      frame_tree_node_id_(next_frame_tree_node_id_++),
      parent_(parent),
      opener_(nullptr),
      original_opener_(nullptr),
      has_committed_real_load_(false),
      is_collapsed_(false),
      replication_state_(
          scope,
          name,
          unique_name,
          false /* should enforce strict mixed content checking */,
          false /* is a potentially trustworthy unique origin */,
          false /* has received a user gesture */),
      devtools_frame_token_(devtools_frame_token),
      frame_owner_properties_(frame_owner_properties),
      loading_progress_(kLoadingProgressNotStarted),
      blame_context_(frame_tree_node_id_, parent) {
  std::pair<FrameTreeNodeIdMap::iterator, bool> result =
      g_frame_tree_node_id_map.Get().insert(
          std::make_pair(frame_tree_node_id_, this));
  CHECK(result.second);

  RecordUniqueNameSize(this);

  // Note: this should always be done last in the constructor.
  blame_context_.Initialize();
}

FrameTreeNode::~FrameTreeNode() {
  std::vector<std::unique_ptr<FrameTreeNode>>().swap(children_);
  frame_tree_->FrameRemoved(this);
  for (auto& observer : observers_)
    observer.OnFrameTreeNodeDestroyed(this);

  if (opener_)
    opener_->RemoveObserver(opener_observer_.get());
  if (original_opener_)
    original_opener_->RemoveObserver(original_opener_observer_.get());

  g_frame_tree_node_id_map.Get().erase(frame_tree_node_id_);

  if (navigation_request_) {
    // PlzNavigate: if a frame with a pending navigation is detached, make sure
    // the WebContents (and its observers) update their loading state.
    navigation_request_.reset();
    DidStopLoading();
  }
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

FrameTreeNode* FrameTreeNode::AddChild(std::unique_ptr<FrameTreeNode> child,
                                       int process_id,
                                       int frame_routing_id) {
  // Child frame must always be created in the same process as the parent.
  CHECK_EQ(process_id, render_manager_.current_host()->GetProcess()->GetID());

  // Initialize the RenderFrameHost for the new node.  We always create child
  // frames in the same SiteInstance as the current frame, and they can swap to
  // a different one if they navigate away.
  child->render_manager()->Init(
      render_manager_.current_host()->GetSiteInstance(),
      render_manager_.current_host()->GetRoutingID(), frame_routing_id,
      MSG_ROUTING_NONE, false);

  // Other renderer processes in this BrowsingInstance may need to find out
  // about the new frame.  Create a proxy for the child frame in all
  // SiteInstances that have a proxy for the frame's parent, since all frames
  // in a frame tree should have the same set of proxies.
  render_manager_.CreateProxiesForChildFrame(child.get());

  children_.push_back(std::move(child));
  return children_.back().get();
}

void FrameTreeNode::RemoveChild(FrameTreeNode* child) {
  for (auto iter = children_.begin(); iter != children_.end(); ++iter) {
    if (iter->get() == child) {
      // Subtle: we need to make sure the node is gone from the tree before
      // observers are notified of its deletion.
      std::unique_ptr<FrameTreeNode> node_to_delete(std::move(*iter));
      children_.erase(iter);
      node_to_delete.reset();
      return;
    }
  }
}

void FrameTreeNode::ResetForNewProcess() {
  current_frame_host()->SetLastCommittedUrl(GURL());
  blame_context_.TakeSnapshot();

  // Remove child nodes from the tree, then delete them. This destruction
  // operation will notify observers.
  std::vector<std::unique_ptr<FrameTreeNode>>().swap(children_);
}

void FrameTreeNode::SetOpener(FrameTreeNode* opener) {
  if (opener_) {
    opener_->RemoveObserver(opener_observer_.get());
    opener_observer_.reset();
  }

  opener_ = opener;

  if (opener_) {
    opener_observer_ = base::MakeUnique<OpenerDestroyedObserver>(this, false);
    opener_->AddObserver(opener_observer_.get());
  }
}

void FrameTreeNode::SetOriginalOpener(FrameTreeNode* opener) {
  // The original opener tracks main frames only.
  DCHECK(opener == nullptr || !opener->parent());

  if (original_opener_) {
    original_opener_->RemoveObserver(original_opener_observer_.get());
    original_opener_observer_.reset();
  }

  original_opener_ = opener;

  if (original_opener_) {
    original_opener_observer_ =
        base::MakeUnique<OpenerDestroyedObserver>(this, true);
    original_opener_->AddObserver(original_opener_observer_.get());
  }
}

void FrameTreeNode::SetCurrentURL(const GURL& url) {
  if (!has_committed_real_load_ && url != url::kAboutBlankURL)
    has_committed_real_load_ = true;
  current_frame_host()->SetLastCommittedUrl(url);
  blame_context_.TakeSnapshot();
}

void FrameTreeNode::SetCurrentOrigin(
    const url::Origin& origin,
    bool is_potentially_trustworthy_unique_origin) {
  if (!origin.IsSameOriginWith(replication_state_.origin) ||
      replication_state_.has_potentially_trustworthy_unique_origin !=
          is_potentially_trustworthy_unique_origin) {
    render_manager_.OnDidUpdateOrigin(origin,
                                      is_potentially_trustworthy_unique_origin);
  }
  replication_state_.origin = origin;
  replication_state_.has_potentially_trustworthy_unique_origin =
      is_potentially_trustworthy_unique_origin;
}

void FrameTreeNode::SetCollapsed(bool collapsed) {
  DCHECK(!IsMainFrame());
  if (is_collapsed_ == collapsed)
    return;

  is_collapsed_ = collapsed;
  render_manager_.OnDidChangeCollapsedState(collapsed);
}

void FrameTreeNode::SetFrameName(const std::string& name,
                                 const std::string& unique_name) {
  if (name == replication_state_.name) {
    // |unique_name| shouldn't change unless |name| changes.
    DCHECK_EQ(unique_name, replication_state_.unique_name);
    return;
  }

  if (parent()) {
    // Non-main frames should have a non-empty unique name.
    DCHECK(!unique_name.empty());
  } else {
    // Unique name of main frames should always stay empty.
    DCHECK(unique_name.empty());
  }

  // Note the unique name should only be able to change before the first real
  // load is committed, but that's not strongly enforced here.
  if (unique_name != replication_state_.unique_name)
    RecordUniqueNameSize(this);
  render_manager_.OnDidUpdateName(name, unique_name);
  replication_state_.name = name;
  replication_state_.unique_name = unique_name;
}

void FrameTreeNode::SetFeaturePolicyHeader(
    const ParsedFeaturePolicyHeader& parsed_header) {
  replication_state_.feature_policy_header = parsed_header;
}

void FrameTreeNode::ResetFeaturePolicyHeader() {
  replication_state_.feature_policy_header.clear();
}

void FrameTreeNode::AddContentSecurityPolicies(
    const std::vector<ContentSecurityPolicyHeader>& headers) {
  replication_state_.accumulated_csp_headers.insert(
      replication_state_.accumulated_csp_headers.end(), headers.begin(),
      headers.end());
  render_manager_.OnDidAddContentSecurityPolicies(headers);
}

void FrameTreeNode::ResetCspHeaders() {
  replication_state_.accumulated_csp_headers.clear();
  render_manager_.OnDidResetContentSecurityPolicy();
}

void FrameTreeNode::SetInsecureRequestPolicy(
    blink::WebInsecureRequestPolicy policy) {
  if (policy == replication_state_.insecure_request_policy)
    return;
  render_manager_.OnEnforceInsecureRequestPolicy(policy);
  replication_state_.insecure_request_policy = policy;
}

void FrameTreeNode::SetPendingFramePolicy(FramePolicy frame_policy) {
  pending_frame_policy_.sandbox_flags = frame_policy.sandbox_flags;

  if (parent()) {
    // Subframes should always inherit their parent's sandbox flags.
    pending_frame_policy_.sandbox_flags |=
        parent()->effective_frame_policy().sandbox_flags;
    // This is only applied on subframes; container policy is not mutable on
    // main frame.
    pending_frame_policy_.container_policy = frame_policy.container_policy;
  }
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
  return GetSibling(-1);
}

FrameTreeNode* FrameTreeNode::NextSibling() const {
  return GetSibling(1);
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

bool FrameTreeNode::CommitPendingFramePolicy() {
  bool did_change_flags = pending_frame_policy_.sandbox_flags !=
                          replication_state_.frame_policy.sandbox_flags;
  bool did_change_container_policy =
      pending_frame_policy_.container_policy !=
      replication_state_.frame_policy.container_policy;
  if (did_change_flags)
    replication_state_.frame_policy.sandbox_flags =
        pending_frame_policy_.sandbox_flags;
  if (did_change_container_policy)
    replication_state_.frame_policy.container_policy =
        pending_frame_policy_.container_policy;
  return did_change_flags || did_change_container_policy;
}

void FrameTreeNode::CreatedNavigationRequest(
    std::unique_ptr<NavigationRequest> navigation_request) {
  CHECK(IsBrowserSideNavigationEnabled());

  // This is never called when navigating to a Javascript URL. For the loading
  // state, this matches what Blink is doing: Blink doesn't send throbber
  // notifications for Javascript URLS.
  DCHECK(!navigation_request->common_params().url.SchemeIs(
      url::kJavaScriptScheme));

  bool was_previously_loading = frame_tree()->IsLoading();

  // There's no need to reset the state: there's still an ongoing load, and the
  // RenderFrameHostManager will take care of updates to the speculative
  // RenderFrameHost in DidCreateNavigationRequest below.
  if (was_previously_loading) {
    if (navigation_request_ && navigation_request_->navigation_handle()) {
      // Mark the old request as aborted.
      navigation_request_->navigation_handle()->set_net_error_code(
          net::ERR_ABORTED);
    }
    ResetNavigationRequest(true, true);
  }

  navigation_request_ = std::move(navigation_request);
  render_manager()->DidCreateNavigationRequest(navigation_request_.get());

  bool to_different_document = !FrameMsg_Navigate_Type::IsSameDocument(
      navigation_request_->common_params().navigation_type);

  DidStartLoading(to_different_document, was_previously_loading);
}

void FrameTreeNode::ResetNavigationRequest(bool keep_state,
                                           bool inform_renderer) {
  CHECK(IsBrowserSideNavigationEnabled());
  if (!navigation_request_)
    return;

  // The renderer should be informed if the caller allows to do so and the
  // navigation came from a BeginNavigation IPC.
  int need_to_inform_renderer =
      inform_renderer && navigation_request_->from_begin_navigation();

  NavigationRequest::AssociatedSiteInstanceType site_instance_type =
      navigation_request_->associated_site_instance_type();
  navigation_request_.reset();

  if (keep_state)
    return;

  // The RenderFrameHostManager should clean up any speculative RenderFrameHost
  // it created for the navigation. Also register that the load stopped.
  DidStopLoading();
  render_manager_.CleanUpNavigation();

  // When reusing the same SiteInstance, a pending WebUI may have been created
  // on behalf of the navigation in the current RenderFrameHost. Clear it.
  if (site_instance_type ==
      NavigationRequest::AssociatedSiteInstanceType::CURRENT) {
    current_frame_host()->ClearPendingWebUI();
  }

  // If the navigation is renderer-initiated, the renderer should also be
  // informed that the navigation stopped if needed. In the case the renderer
  // process asked for the navigation to be aborted, e.g. following a
  // document.open, do not send an IPC to the renderer process as it already
  // expects the navigation to stop.
  if (need_to_inform_renderer) {
    current_frame_host()->Send(
        new FrameMsg_DroppedNavigation(current_frame_host()->GetRoutingID()));
  }

}

bool FrameTreeNode::has_started_loading() const {
  return loading_progress_ != kLoadingProgressNotStarted;
}

void FrameTreeNode::reset_loading_progress() {
  loading_progress_ = kLoadingProgressNotStarted;
}

void FrameTreeNode::DidStartLoading(bool to_different_document,
                                    bool was_previously_loading) {
  // Any main frame load to a new document should reset the load progress since
  // it will replace the current page and any frames. The WebContents will
  // be notified when DidChangeLoadProgress is called.
  if (to_different_document && IsMainFrame())
    frame_tree_->ResetLoadProgress();

  // Notify the WebContents.
  if (!was_previously_loading)
    navigator()->GetDelegate()->DidStartLoading(this, to_different_document);

  // Set initial load progress and update overall progress. This will notify
  // the WebContents of the load progress change.
  DidChangeLoadProgress(kLoadingProgressMinimum);

  // Notify the RenderFrameHostManager of the event.
  render_manager()->OnDidStartLoading();
}

void FrameTreeNode::DidStopLoading() {
  // Set final load progress and update overall progress. This will notify
  // the WebContents of the load progress change.
  DidChangeLoadProgress(kLoadingProgressDone);

  // Notify the WebContents.
  if (!frame_tree_->IsLoading())
    navigator()->GetDelegate()->DidStopLoading();

  // Notify the RenderFrameHostManager of the event.
  render_manager()->OnDidStopLoading();
}

void FrameTreeNode::DidChangeLoadProgress(double load_progress) {
  loading_progress_ = load_progress;
  frame_tree_->UpdateLoadProgress();
}

bool FrameTreeNode::StopLoading() {
  if (IsBrowserSideNavigationEnabled()) {
    if (navigation_request_) {
      int expected_pending_nav_entry_id = navigation_request_->nav_entry_id();
      if (navigation_request_->navigation_handle()) {
        navigation_request_->navigation_handle()->set_net_error_code(
            net::ERR_ABORTED);
        expected_pending_nav_entry_id =
            navigation_request_->navigation_handle()->pending_nav_entry_id();
      }
      navigator_->DiscardPendingEntryIfNeeded(expected_pending_nav_entry_id);
    }
    ResetNavigationRequest(false, true);
  }

  // TODO(nasko): see if child frames should send IPCs in site-per-process
  // mode.
  if (!IsMainFrame())
    return true;

  render_manager_.Stop();
  return true;
}

void FrameTreeNode::DidFocus() {
  last_focus_time_ = base::TimeTicks::Now();
  for (auto& observer : observers_)
    observer.OnFrameTreeNodeFocused(this);
}

void FrameTreeNode::BeforeUnloadCanceled() {
  // TODO(clamy): Support BeforeUnload in subframes.
  if (!IsMainFrame())
    return;

  RenderFrameHostImpl* current_frame_host =
      render_manager_.current_frame_host();
  DCHECK(current_frame_host);
  current_frame_host->ResetLoadingState();

  if (IsBrowserSideNavigationEnabled()) {
    RenderFrameHostImpl* speculative_frame_host =
        render_manager_.speculative_frame_host();
    if (speculative_frame_host)
      speculative_frame_host->ResetLoadingState();
    // Note: there is no need to set an error code on the NavigationHandle here
    // as it has not been created yet. It is only created when the
    // BeforeUnloadACK is received.
    if (navigation_request_)
      ResetNavigationRequest(false, true);
  } else {
    RenderFrameHostImpl* pending_frame_host =
        render_manager_.pending_frame_host();
    if (pending_frame_host)
      pending_frame_host->ResetLoadingState();
  }
}

void FrameTreeNode::OnSetHasReceivedUserGesture() {
  render_manager_.OnSetHasReceivedUserGesture();
  replication_state_.has_received_user_gesture = true;
}

FrameTreeNode* FrameTreeNode::GetSibling(int relative_offset) const {
  if (!parent_ || !parent_->child_count())
    return nullptr;

  for (size_t i = 0; i < parent_->child_count(); ++i) {
    if (parent_->child_at(i) == this) {
      if ((relative_offset < 0 && static_cast<size_t>(-relative_offset) > i) ||
          i + relative_offset >= parent_->child_count()) {
        return nullptr;
      }
      return parent_->child_at(i + relative_offset);
    }
  }

  NOTREACHED() << "FrameTreeNode not found in its parent's children.";
  return nullptr;
}

}  // namespace content
