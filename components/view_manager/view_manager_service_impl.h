// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_VIEW_MANAGER_SERVICE_IMPL_H_
#define COMPONENTS_VIEW_MANAGER_VIEW_MANAGER_SERVICE_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/view_manager/access_policy_delegate.h"
#include "components/view_manager/ids.h"
#include "components/view_manager/public/interfaces/surface_id.mojom.h"
#include "components/view_manager/public/interfaces/view_manager.mojom.h"

namespace gfx {
class Rect;
}

namespace view_manager {

class AccessPolicy;
class ConnectionManager;
class ServerView;

// An instance of ViewManagerServiceImpl is created for every ViewManagerService
// request. ViewManagerServiceImpl tracks all the state and views created by a
// client. ViewManagerServiceImpl coordinates with ConnectionManager to update
// the client (and internal state) as necessary.
class ViewManagerServiceImpl : public mojo::ViewManagerService,
                               public AccessPolicyDelegate {
 public:
  ViewManagerServiceImpl(ConnectionManager* connection_manager,
                         mojo::ConnectionSpecificId creator_id,
                         const ViewId& root_id);
  ~ViewManagerServiceImpl() override;

  // |services| and |exposed_services| are the ServiceProviders to pass to the
  // client via OnEmbed().
  void Init(mojo::ViewManagerClient* client,
            mojo::ViewManagerServicePtr service_ptr);

  mojo::ConnectionSpecificId id() const { return id_; }
  mojo::ConnectionSpecificId creator_id() const { return creator_id_; }

  mojo::ViewManagerClient* client() { return client_; }

  // Returns the View with the specified id.
  ServerView* GetView(const ViewId& id) {
    return const_cast<ServerView*>(
        const_cast<const ViewManagerServiceImpl*>(this)->GetView(id));
  }
  const ServerView* GetView(const ViewId& id) const;

  // Returns true if this connection's root is |id|.
  bool IsRoot(const ViewId& id) const;

  // Returns the id of the root node. This is null if the root has been
  // destroyed but the connection is still valid.
  const ViewId* root() const { return root_.get(); }

  bool is_embed_root() const { return is_embed_root_; }

  // Invoked when a connection is about to be destroyed.
  void OnWillDestroyViewManagerServiceImpl(ViewManagerServiceImpl* connection);

  // These functions are synchronous variants of those defined in the mojom. The
  // ViewManagerService implementations all call into these. See the mojom for
  // details.
  mojo::ErrorCode CreateView(const ViewId& view_id);
  bool AddView(const ViewId& parent_id, const ViewId& child_id);
  std::vector<const ServerView*> GetViewTree(const ViewId& view_id) const;
  bool SetViewVisibility(const ViewId& view_id, bool visible);
  void EmbedAllowingReembed(const ViewId& view_id,
                            mojo::URLRequestPtr request,
                            const mojo::Callback<void(bool)>& callback);
  bool Embed(const ViewId& view_id, mojo::ViewManagerClientPtr client);

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
  using ViewIdSet = base::hash_set<mojo::Id>;
  using ViewMap = std::map<mojo::ConnectionSpecificId, ServerView*>;

  struct PendingEmbed;

  bool IsViewKnown(const ServerView* view) const;

  // These functions return true if the corresponding mojom function is allowed
  // for this connection.
  bool CanReorderView(const ServerView* view,
                      const ServerView* relative_view,
                      mojo::OrderDirection direction) const;

  // Deletes a view owned by this connection. Returns true on success. |source|
  // is the connection that originated the change.
  bool DeleteViewImpl(ViewManagerServiceImpl* source, ServerView* view);

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

  bool CanEmbed(const ViewId& view_id) const;
  void PrepareForEmbed(const ViewId& view_id);
  void RemoveChildrenAsPartOfEmbed(const ViewId& view_id);
  void OnEmbedForDescendantDone(scoped_refptr<PendingEmbed> pending_embed,
                                mojo::ViewManagerClientPtr client);

  // Invalidates any PendingEmbeds with |connection| as the embed root.
  void InvalidatePendingEmbedForConnection(ViewManagerServiceImpl* connection);

  // Invalidates any PendingEmbemds targetting |view_id|.
  void InvalidatePendingEmbedForView(const ViewId& view_id);

  // Runs the callback for |embed| and releases it.
  void RemovePendingEmbedAndNotifyCallback(scoped_refptr<PendingEmbed> embed,
                                           bool success);

  // ViewManagerService:
  void CreateView(
      mojo::Id transport_view_id,
      const mojo::Callback<void(mojo::ErrorCode)>& callback) override;
  void DeleteView(mojo::Id transport_view_id,
                  const mojo::Callback<void(bool)>& callback) override;
  void AddView(mojo::Id parent_id,
               mojo::Id child_id,
               const mojo::Callback<void(bool)>& callback) override;
  void RemoveViewFromParent(
      mojo::Id view_id,
      const mojo::Callback<void(bool)>& callback) override;
  void ReorderView(mojo::Id view_id,
                   mojo::Id relative_view_id,
                   mojo::OrderDirection direction,
                   const mojo::Callback<void(bool)>& callback) override;
  void GetViewTree(mojo::Id view_id,
                   const mojo::Callback<void(mojo::Array<mojo::ViewDataPtr>)>&
                       callback) override;
  void SetViewSurfaceId(mojo::Id view_id,
                        mojo::SurfaceIdPtr surface_id,
                        const mojo::Callback<void(bool)>& callback) override;
  void SetViewBounds(mojo::Id view_id,
                     mojo::RectPtr bounds,
                     const mojo::Callback<void(bool)>& callback) override;
  void SetViewVisibility(mojo::Id view_id,
                         bool visible,
                         const mojo::Callback<void(bool)>& callback) override;
  void SetViewProperty(mojo::Id view_id,
                       const mojo::String& name,
                       mojo::Array<uint8_t> value,
                       const mojo::Callback<void(bool)>& callback) override;
  void SetEmbedRoot() override;
  void Embed(mojo::Id transport_view_id,
             mojo::ViewManagerClientPtr client,
             const mojo::Callback<void(bool)>& callback) override;
  void EmbedAllowingReembed(
      mojo::Id transport_view_id,
      mojo::URLRequestPtr request,
      const mojo::Callback<void(bool)>& callback) override;
  void SetFocus(uint32_t view_id, const SetFocusCallback& callback) override;
  void SetViewTextInputState(uint32_t view_id,
                             mojo::TextInputStatePtr state) override;
  void SetImeVisibility(uint32_t view_id,
                        bool visible,
                        mojo::TextInputStatePtr state) override;


  // AccessPolicyDelegate:
  bool IsRootForAccessPolicy(const ViewId& id) const override;
  bool IsViewKnownForAccessPolicy(const ServerView* view) const override;
  bool IsViewRootOfAnotherConnectionForAccessPolicy(
      const ServerView* view) const override;
  bool IsEmbedRootForAccessPolicy() override;

  ConnectionManager* connection_manager_;

  // Id of this connection as assigned by ConnectionManager.
  const mojo::ConnectionSpecificId id_;

  // ID of the connection that created us. If 0 it indicates either we were
  // created by the root, or the connection that created us has been destroyed.
  mojo::ConnectionSpecificId creator_id_;

  mojo::ViewManagerClient* client_;

  scoped_ptr<AccessPolicy> access_policy_;

  // The views created by this connection. This connection owns these objects.
  ViewMap view_map_;

  // The set of views that has been communicated to the client.
  ViewIdSet known_views_;

  // The root of this connection. This is a scoped_ptr to reinforce the
  // connection may have no root. A connection has no root if either the root
  // is destroyed or Embed() is invoked on the root.
  scoped_ptr<ViewId> root_;

  bool is_embed_root_;

  // If we have an ancestor marked as an embed root then we have to query it
  // before completing the embed. |pending_embeds_| contains such requests.
  // The requests are removed if the embed becomes invalid, for example, the
  // view embed was called for is removed.
  std::set<scoped_refptr<PendingEmbed>> pending_embeds_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerServiceImpl);
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_VIEW_MANAGER_SERVICE_IMPL_H_
