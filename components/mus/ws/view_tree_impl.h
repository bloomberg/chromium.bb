// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_VIEW_TREE_IMPL_H_
#define COMPONENTS_MUS_WS_VIEW_TREE_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/surface_id.mojom.h"
#include "components/mus/public/interfaces/view_tree.mojom.h"
#include "components/mus/ws/access_policy_delegate.h"
#include "components/mus/ws/ids.h"

namespace gfx {
class Rect;
}

namespace mus {

class AccessPolicy;
class ConnectionManager;
class ServerView;
class ViewTreeHostImpl;

// An instance of ViewTreeImpl is created for every ViewTree request.
// ViewTreeImpl tracks all the state and views created by a client. ViewTreeImpl
// coordinates with ConnectionManager to update the client (and internal state)
// as necessary.
class ViewTreeImpl : public mojo::ViewTree, public AccessPolicyDelegate {
 public:
  ViewTreeImpl(ConnectionManager* connection_manager,
               ConnectionSpecificId creator_id,
               const ViewId& root_id,
               uint32_t policy_bitmask);
  ~ViewTreeImpl() override;

  // |services| and |exposed_services| are the ServiceProviders to pass to the
  // client via OnEmbed().
  void Init(mojo::ViewTreeClient* client, mojo::ViewTreePtr tree);

  ConnectionSpecificId id() const { return id_; }
  ConnectionSpecificId creator_id() const { return creator_id_; }

  mojo::ViewTreeClient* client() { return client_; }

  // Returns the View with the specified id.
  ServerView* GetView(const ViewId& id) {
    return const_cast<ServerView*>(
        const_cast<const ViewTreeImpl*>(this)->GetView(id));
  }
  const ServerView* GetView(const ViewId& id) const;

  // Returns true if this connection's root is |id|.
  bool IsRoot(const ViewId& id) const;

  // Returns the id of the root node. This is null if the root has been
  // destroyed but the connection is still valid.
  const ViewId* root() const { return root_.get(); }

  bool is_embed_root() const { return is_embed_root_; }

  ViewTreeHostImpl* GetHost();

  // Invoked when a connection is about to be destroyed.
  void OnWillDestroyViewTreeImpl(ViewTreeImpl* connection);

  // These functions are synchronous variants of those defined in the mojom. The
  // ViewTree implementations all call into these. See the mojom for details.
  mojo::ErrorCode CreateView(const ViewId& view_id);
  bool AddView(const ViewId& parent_id, const ViewId& child_id);
  std::vector<const ServerView*> GetViewTree(const ViewId& view_id) const;
  bool SetViewVisibility(const ViewId& view_id, bool visible);
  bool Embed(const ViewId& view_id,
             mojo::ViewTreeClientPtr client,
             uint32_t policy_bitmask,
             ConnectionSpecificId* connection_id);
  void Embed(const ViewId& view_id, mojo::URLRequestPtr request);

  // The following methods are invoked after the corresponding change has been
  // processed. They do the appropriate bookkeeping and update the client as
  // necessary.
  void ProcessViewBoundsChanged(const ServerView* view,
                                const gfx::Rect& old_bounds,
                                const gfx::Rect& new_bounds,
                                bool originated_change);
  void ProcessViewportMetricsChanged(const mojo::ViewportMetrics& old_metrics,
                                     const mojo::ViewportMetrics& new_metrics,
                                     bool originated_change);
  void ProcessWillChangeViewHierarchy(const ServerView* view,
                                      const ServerView* new_parent,
                                      const ServerView* old_parent,
                                      bool originated_change);
  void ProcessViewPropertyChanged(const ServerView* view,
                                  const std::string& name,
                                  const std::vector<uint8_t>* new_data,
                                  bool originated_change);
  void ProcessViewHierarchyChanged(const ServerView* view,
                                   const ServerView* new_parent,
                                   const ServerView* old_parent,
                                   bool originated_change);
  void ProcessViewReorder(const ServerView* view,
                          const ServerView* relative_view,
                          mojo::OrderDirection direction,
                          bool originated_change);
  void ProcessViewDeleted(const ViewId& view, bool originated_change);
  void ProcessWillChangeViewVisibility(const ServerView* view,
                                       bool originated_change);
  void ProcessFocusChanged(const ServerView* old_focused_view,
                           const ServerView* new_focused_view);

 private:
  using ViewIdSet = base::hash_set<Id>;
  using ViewMap = std::map<ConnectionSpecificId, ServerView*>;

  bool IsViewKnown(const ServerView* view) const;

  // These functions return true if the corresponding mojom function is allowed
  // for this connection.
  bool CanReorderView(const ServerView* view,
                      const ServerView* relative_view,
                      mojo::OrderDirection direction) const;

  // Deletes a view owned by this connection. Returns true on success. |source|
  // is the connection that originated the change.
  bool DeleteViewImpl(ViewTreeImpl* source, ServerView* view);

  // If |view| is known (in |known_views_|) does nothing. Otherwise adds |view|
  // to |views|, marks |view| as known and recurses.
  void GetUnknownViewsFrom(const ServerView* view,
                           std::vector<const ServerView*>* views);

  // Removes |view| and all its descendants from |known_views_|. This does not
  // recurse through views that were created by this connection. All views owned
  // by this connection are added to |local_views|.
  void RemoveFromKnown(const ServerView* view,
                       std::vector<ServerView*>* local_views);

  // Resets the root of this connection.
  void RemoveRoot();

  // Converts View(s) to ViewData(s) for transport. This assumes all the views
  // are valid for the client. The parent of views the client is not allowed to
  // see are set to NULL (in the returned ViewData(s)).
  mojo::Array<mojo::ViewDataPtr> ViewsToViewDatas(
      const std::vector<const ServerView*>& views);
  mojo::ViewDataPtr ViewToViewData(const ServerView* view);

  // Implementation of GetViewTree(). Adds |view| to |views| and recurses if
  // CanDescendIntoViewForViewTree() returns true.
  void GetViewTreeImpl(const ServerView* view,
                       std::vector<const ServerView*>* views) const;

  // Notify the client if the drawn state of any of the roots changes.
  // |view| is the view that is changing to the drawn state |new_drawn_value|.
  void NotifyDrawnStateChanged(const ServerView* view, bool new_drawn_value);

  // Deletes all Views we own.
  void DestroyViews();

  bool CanEmbed(const ViewId& view_id, uint32_t policy_bitmask) const;
  void PrepareForEmbed(const ViewId& view_id);
  void RemoveChildrenAsPartOfEmbed(const ViewId& view_id);

  // ViewTree:
  void CreateView(
      Id transport_view_id,
      const mojo::Callback<void(mojo::ErrorCode)>& callback) override;
  void DeleteView(Id transport_view_id,
                  const mojo::Callback<void(bool)>& callback) override;
  void AddView(Id parent_id,
               Id child_id,
               const mojo::Callback<void(bool)>& callback) override;
  void RemoveViewFromParent(
      Id view_id,
      const mojo::Callback<void(bool)>& callback) override;
  void ReorderView(Id view_id,
                   Id relative_view_id,
                   mojo::OrderDirection direction,
                   const mojo::Callback<void(bool)>& callback) override;
  void GetViewTree(Id view_id,
                   const mojo::Callback<void(mojo::Array<mojo::ViewDataPtr>)>&
                       callback) override;
  void SetViewBounds(Id view_id,
                     mojo::RectPtr bounds,
                     const mojo::Callback<void(bool)>& callback) override;
  void SetViewVisibility(Id view_id,
                         bool visible,
                         const mojo::Callback<void(bool)>& callback) override;
  void SetViewProperty(Id view_id,
                       const mojo::String& name,
                       mojo::Array<uint8_t> value,
                       const mojo::Callback<void(bool)>& callback) override;
  void RequestSurface(Id view_id,
                      mojo::InterfaceRequest<mojo::Surface> surface,
                      mojo::SurfaceClientPtr client) override;
  void Embed(Id transport_view_id,
             mojo::ViewTreeClientPtr client,
             uint32_t policy_bitmask,
             const EmbedCallback& callback) override;
  void SetFocus(uint32_t view_id) override;
  void SetViewTextInputState(uint32_t view_id,
                             mojo::TextInputStatePtr state) override;
  void SetImeVisibility(Id transport_view_id,
                        bool visible,
                        mojo::TextInputStatePtr state) override;

  // AccessPolicyDelegate:
  bool IsRootForAccessPolicy(const ViewId& id) const override;
  bool IsViewKnownForAccessPolicy(const ServerView* view) const override;
  bool IsViewRootOfAnotherConnectionForAccessPolicy(
      const ServerView* view) const override;
  bool IsDescendantOfEmbedRoot(const ServerView* view) override;

  ConnectionManager* connection_manager_;

  // Id of this connection as assigned by ConnectionManager.
  const ConnectionSpecificId id_;

  // ID of the connection that created us. If 0 it indicates either we were
  // created by the root, or the connection that created us has been destroyed.
  ConnectionSpecificId creator_id_;

  mojo::ViewTreeClient* client_;

  scoped_ptr<mus::AccessPolicy> access_policy_;

  // The views created by this connection. This connection owns these objects.
  ViewMap view_map_;

  // The set of views that has been communicated to the client.
  ViewIdSet known_views_;

  // The root of this connection. This is a scoped_ptr to reinforce the
  // connection may have no root. A connection has no root if either the root
  // is destroyed or Embed() is invoked on the root.
  scoped_ptr<ViewId> root_;

  bool is_embed_root_;

  DISALLOW_COPY_AND_ASSIGN(ViewTreeImpl);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_VIEW_TREE_IMPL_H_
