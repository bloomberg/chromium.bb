// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_NODE_H_
#define CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_NODE_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_frame_host_manager.h"
#include "content/common/content_export.h"
#include "content/common/frame_replication_state.h"
#include "third_party/WebKit/public/web/WebFrameOwnerProperties.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class FrameTree;
class NavigationRequest;
class Navigator;
class RenderFrameHostImpl;

// When a page contains iframes, its renderer process maintains a tree structure
// of those frames. We are mirroring this tree in the browser process. This
// class represents a node in this tree and is a wrapper for all objects that
// are frame-specific (as opposed to page-specific).
class CONTENT_EXPORT FrameTreeNode {
 public:
  class Observer {
   public:
    // Invoked when a FrameTreeNode is being destroyed.
    virtual void OnFrameTreeNodeDestroyed(FrameTreeNode* node) {}

    // Invoked when a FrameTreeNode becomes focused.
    virtual void OnFrameTreeNodeFocused(FrameTreeNode* node) {}

    virtual ~Observer() {}
  };

  static const int kFrameTreeNodeInvalidId = -1;

  // Returns the FrameTreeNode with the given global |frame_tree_node_id|,
  // regardless of which FrameTree it is in.
  static FrameTreeNode* GloballyFindByID(int frame_tree_node_id);

  FrameTreeNode(FrameTree* frame_tree,
                Navigator* navigator,
                RenderFrameHostDelegate* render_frame_delegate,
                RenderViewHostDelegate* render_view_delegate,
                RenderWidgetHostDelegate* render_widget_delegate,
                RenderFrameHostManager::Delegate* manager_delegate,
                blink::WebTreeScopeType scope,
                const std::string& name,
                blink::WebSandboxFlags sandbox_flags,
                const blink::WebFrameOwnerProperties& frame_owner_properties);

  ~FrameTreeNode();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool IsMainFrame() const;

  FrameTreeNode* AddChild(scoped_ptr<FrameTreeNode> child,
                          int process_id,
                          int frame_routing_id);
  void RemoveChild(FrameTreeNode* child);

  // Clears process specific-state in this node to prepare for a new process.
  void ResetForNewProcess();

  FrameTree* frame_tree() const {
    return frame_tree_;
  }

  Navigator* navigator() {
    return navigator_.get();
  }

  RenderFrameHostManager* render_manager() {
    return &render_manager_;
  }

  int frame_tree_node_id() const {
    return frame_tree_node_id_;
  }

  const std::string& frame_name() const {
    return replication_state_.name;
  }

  const url::Origin& frame_origin() const { return replication_state_.origin; }

  size_t child_count() const {
    return children_.size();
  }

  FrameTreeNode* parent() const { return parent_; }

  FrameTreeNode* opener() const { return opener_; }

  // Assigns a new opener for this node and, if |opener| is non-null, registers
  // an observer that will clear this node's opener if |opener| is ever
  // destroyed.
  void SetOpener(FrameTreeNode* opener);

  FrameTreeNode* child_at(size_t index) const {
    return children_[index].get();
  }

  // Returns the URL of the last committed page in this frame.
  const GURL& current_url() const {
    return current_url_;
  }

  // Sets the last committed URL for this frame and updates
  // has_committed_real_load accordingly.
  void SetCurrentURL(const GURL& url);

  // Returns true iff SetCurrentURL has been called with a non-blank URL.
  bool has_committed_real_load() const {
    return has_committed_real_load_;
  }

  // Returns the origin of the last committed page in this frame.
  const url::Origin& current_origin() const {
    return replication_state_.origin;
  }

  // Set the current origin and notify proxies about the update.
  void SetCurrentOrigin(const url::Origin& origin);

  // Set the current name and notify proxies about the update.
  void SetFrameName(const std::string& name);

  // Sets the current enforcement of strict mixed content checking and
  // notifies proxies about the update.
  void SetEnforceStrictMixedContentChecking(bool should_enforce);

  blink::WebSandboxFlags effective_sandbox_flags() {
    return effective_sandbox_flags_;
  }

  void set_sandbox_flags(blink::WebSandboxFlags sandbox_flags) {
    replication_state_.sandbox_flags = sandbox_flags;
  }

  // Transfer any pending sandbox flags into |effective_sandbox_flags_|, and
  // return true if the sandbox flags were changed.
  bool CommitPendingSandboxFlags();

  const blink::WebFrameOwnerProperties& frame_owner_properties() {
    return frame_owner_properties_;
  }

  void set_frame_owner_properties(
      const blink::WebFrameOwnerProperties& frame_owner_properties) {
    frame_owner_properties_ = frame_owner_properties;
  }

  bool HasSameOrigin(const FrameTreeNode& node) const {
    return replication_state_.origin.IsSameOriginWith(
        node.replication_state_.origin);
  }

  const FrameReplicationState& current_replication_state() const {
    return replication_state_;
  }

  RenderFrameHostImpl* current_frame_host() const {
    return render_manager_.current_frame_host();
  }

  bool IsDescendantOf(FrameTreeNode* other) const;

  // Return the node immediately preceding this node in its parent's
  // |children_|, or nullptr if there is no such node.
  FrameTreeNode* PreviousSibling() const;

  // Returns true if this node is in a loading state.
  bool IsLoading() const;

  // Returns this node's loading progress.
  double loading_progress() const { return loading_progress_; }

  NavigationRequest* navigation_request() { return navigation_request_.get(); }

  // PlzNavigate
  // Takes ownership of |navigation_request| and makes it the current
  // NavigationRequest of this frame. This corresponds to the start of a new
  // navigation. If there was an ongoing navigation request before calling this
  // function, it is canceled. |navigation_request| should not be null.
  void CreatedNavigationRequest(
      scoped_ptr<NavigationRequest> navigation_request);

  // PlzNavigate
  // Resets the current navigation request. |is_commit| is true if the reset is
  // due to the commit of the navigation.
  void ResetNavigationRequest(bool is_commit);

  // Returns true if this node is in a state where the loading progress is being
  // tracked.
  bool has_started_loading() const;

  // Resets this node's loading progress.
  void reset_loading_progress();

  // A RenderFrameHost in this node started loading.
  // |to_different_document| will be true unless the load is a fragment
  // navigation, or triggered by history.pushState/replaceState.
  void DidStartLoading(bool to_different_document);

  // A RenderFrameHost in this node stopped loading.
  void DidStopLoading();

  // The load progress for a RenderFrameHost in this node was updated to
  // |load_progress|. This will notify the FrameTree which will in turn notify
  // the WebContents.
  void DidChangeLoadProgress(double load_progress);

  // Called when the user directed the page to stop loading. Stops all loads
  // happening in the FrameTreeNode. This method should be used with
  // FrameTree::ForEach to stop all loads in the entire FrameTree.
  bool StopLoading();

  // Returns the time this frame was last focused.
  base::TimeTicks last_focus_time() const { return last_focus_time_; }

  // Called when this node becomes focused.  Updates the node's last focused
  // time and notifies observers.
  void DidFocus();

 private:
  class OpenerDestroyedObserver;

  void set_parent(FrameTreeNode* parent) { parent_ = parent; }

  // The next available browser-global FrameTreeNode ID.
  static int next_frame_tree_node_id_;

  // The FrameTree that owns us.
  FrameTree* frame_tree_;  // not owned.

  // The Navigator object responsible for managing navigations at this node
  // of the frame tree.
  scoped_refptr<Navigator> navigator_;

  // Manages creation and swapping of RenderFrameHosts for this frame.  This
  // must be declared before |children_| so that it gets deleted after them.
  // That's currently necessary so that RenderFrameHostImpl's destructor can
  // call GetProcess.
  RenderFrameHostManager render_manager_;

  // A browser-global identifier for the frame in the page, which stays stable
  // even if the frame does a cross-process navigation.
  const int frame_tree_node_id_;

  // The parent node of this frame. NULL if this node is the root or if it has
  // not yet been attached to the frame tree.
  FrameTreeNode* parent_;

  // The frame that opened this frame, if any.  Will be set to null if the
  // opener is closed, or if this frame disowns its opener by setting its
  // window.opener to null.
  FrameTreeNode* opener_;

  // An observer that clears this node's |opener_| if the opener is destroyed.
  // This observer is added to the |opener_|'s observer list when the |opener_|
  // is set to a non-null node, and it is removed from that list when |opener_|
  // changes or when this node is destroyed.  It is also cleared if |opener_|
  // is disowned.
  scoped_ptr<OpenerDestroyedObserver> opener_observer_;

  // The immediate children of this specific frame.
  std::vector<scoped_ptr<FrameTreeNode>> children_;

  // Track the current frame's last committed URL.
  // TODO(creis): Consider storing a reference to the last committed
  // FrameNavigationEntry here once those are created in all modes.
  GURL current_url_;

  // Whether this frame has committed any real load, replacing its initial
  // about:blank page.
  bool has_committed_real_load_;

  // Track information that needs to be replicated to processes that have
  // proxies for this frame.
  FrameReplicationState replication_state_;

  // Track the effective sandbox flags for this frame.  When a parent frame
  // dynamically updates sandbox flags for a child frame, the child's updated
  // sandbox flags are stored in replication_state_.sandbox_flags. However, the
  // update only takes effect on the next frame navigation, so the effective
  // sandbox flags are tracked separately here.  When enforcing sandbox flags
  // directives in the browser process, |effective_sandbox_flags_| should be
  // used.  |effective_sandbox_flags_| is updated with any pending sandbox
  // flags when a navigation for this frame commits.
  blink::WebSandboxFlags effective_sandbox_flags_;

  // Tracks the scrolling and margin properties for this frame.  These
  // properties affect the child renderer but are stored on its parent's
  // frame element.  When this frame's parent dynamically updates these
  // properties, we update them here too.
  //
  // Note that dynamic updates only take effect on the next frame navigation.
  blink::WebFrameOwnerProperties frame_owner_properties_;

  // Used to track this node's loading progress (from 0 to 1).
  double loading_progress_;

  // PlzNavigate
  // Owns an ongoing NavigationRequest until it is ready to commit. It will then
  // be reset and a RenderFrameHost will be responsible for the navigation.
  scoped_ptr<NavigationRequest> navigation_request_;

  // List of objects observing this FrameTreeNode.
  base::ObserverList<Observer> observers_;

  base::TimeTicks last_focus_time_;

  DISALLOW_COPY_AND_ASSIGN(FrameTreeNode);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_NODE_H_
