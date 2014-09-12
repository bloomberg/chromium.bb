// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_SERVICE_IMPL_H_
#define MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_SERVICE_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/services/public/interfaces/surfaces/surface_id.mojom.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "mojo/services/view_manager/access_policy_delegate.h"
#include "mojo/services/view_manager/ids.h"
#include "mojo/services/view_manager/view_manager_export.h"

namespace gfx {
class Rect;
}

namespace mojo {
namespace service {

class AccessPolicy;
class ConnectionManager;
class ServerView;

#if defined(OS_WIN)
// Equivalent of NON_EXPORTED_BASE which does not work with the template snafu
// below.
#pragma warning(push)
#pragma warning(disable : 4275)
#endif

// Manages a connection from the client.
class MOJO_VIEW_MANAGER_EXPORT ViewManagerServiceImpl
    : public InterfaceImpl<ViewManagerService>,
      public AccessPolicyDelegate {
 public:
  ViewManagerServiceImpl(ConnectionManager* connection_manager,
                         ConnectionSpecificId creator_id,
                         const std::string& creator_url,
                         const std::string& url,
                         const ViewId& root_id,
                         InterfaceRequest<ServiceProvider> service_provider);
  virtual ~ViewManagerServiceImpl();

  // Used to mark this connection as originating from a call to
  // ViewManagerService::Connect(). When set OnConnectionError() deletes |this|.
  void set_delete_on_connection_error() { delete_on_connection_error_ = true; }

  ConnectionSpecificId id() const { return id_; }
  ConnectionSpecificId creator_id() const { return creator_id_; }
  const std::string& url() const { return url_; }

  // Returns the View with the specified id.
  ServerView* GetView(const ViewId& id) {
    return const_cast<ServerView*>(
        const_cast<const ViewManagerServiceImpl*>(this)->GetView(id));
  }
  const ServerView* GetView(const ViewId& id) const;

  // Returns true if this has |id| as a root.
  bool HasRoot(const ViewId& id) const;

  // Invoked when a connection is destroyed.
  void OnViewManagerServiceImplDestroyed(ConnectionSpecificId id);

  // The following methods are invoked after the corresponding change has been
  // processed. They do the appropriate bookkeeping and update the client as
  // necessary.
  void ProcessViewBoundsChanged(const ServerView* view,
                                const gfx::Rect& old_bounds,
                                const gfx::Rect& new_bounds,
                                bool originated_change);
  void ProcessWillChangeViewHierarchy(const ServerView* view,
                                      const ServerView* new_parent,
                                      const ServerView* old_parent,
                                      bool originated_change);
  void ProcessViewHierarchyChanged(const ServerView* view,
                                   const ServerView* new_parent,
                                   const ServerView* old_parent,
                                   bool originated_change);
  void ProcessViewReorder(const ServerView* view,
                          const ServerView* relative_view,
                          OrderDirection direction,
                          bool originated_change);
  void ProcessViewDeleted(const ViewId& view, bool originated_change);
  void ProcessWillChangeViewVisibility(const ServerView* view,
                                       bool originated_change);

  // TODO(sky): move this to private section (currently can't because of
  // bindings).
  // InterfaceImp overrides:
  virtual void OnConnectionError() MOJO_OVERRIDE;

 private:
  typedef std::map<ConnectionSpecificId, ServerView*> ViewMap;
  typedef base::hash_set<Id> ViewIdSet;

  bool IsViewKnown(const ServerView* view) const;

  // These functions return true if the corresponding mojom function is allowed
  // for this connection.
  bool CanReorderView(const ServerView* view,
                      const ServerView* relative_view,
                      OrderDirection direction) const;

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

  // Removes |view_id| from the set of roots this connection knows about.
  void RemoveRoot(const ViewId& view_id);

  void RemoveChildrenAsPartOfEmbed(const ViewId& view_id);

  // Converts View(s) to ViewData(s) for transport. This assumes all the views
  // are valid for the client. The parent of views the client is not allowed to
  // see are set to NULL (in the returned ViewData(s)).
  Array<ViewDataPtr> ViewsToViewDatas(
      const std::vector<const ServerView*>& views);
  ViewDataPtr ViewToViewData(const ServerView* view);

  // Implementation of GetViewTree(). Adds |view| to |views| and recurses if
  // CanDescendIntoViewForViewTree() returns true.
  void GetViewTreeImpl(const ServerView* view,
                       std::vector<const ServerView*>* views) const;

  // Notify the client if the drawn state of any of the roots changes.
  // |view| is the view that is changing to the drawn state |new_drawn_value|.
  void NotifyDrawnStateChanged(const ServerView* view, bool new_drawn_value);

  // ViewManagerService:
  virtual void CreateView(Id transport_view_id,
                          const Callback<void(ErrorCode)>& callback) OVERRIDE;
  virtual void DeleteView(Id transport_view_id,
                          const Callback<void(bool)>& callback) OVERRIDE;
  virtual void AddView(Id parent_id,
                       Id child_id,
                       const Callback<void(bool)>& callback) OVERRIDE;
  virtual void RemoveViewFromParent(
      Id view_id,
      const Callback<void(bool)>& callback) OVERRIDE;
  virtual void ReorderView(Id view_id,
                           Id relative_view_id,
                           OrderDirection direction,
                           const Callback<void(bool)>& callback) OVERRIDE;
  virtual void GetViewTree(
      Id view_id,
      const Callback<void(Array<ViewDataPtr>)>& callback) OVERRIDE;
  virtual void SetViewSurfaceId(Id view_id,
                                SurfaceIdPtr surface_id,
                                const Callback<void(bool)>& callback) OVERRIDE;
  virtual void SetViewBounds(Id view_id,
                             RectPtr bounds,
                             const Callback<void(bool)>& callback) OVERRIDE;
  virtual void SetViewVisibility(Id view_id,
                                 bool visible,
                                 const Callback<void(bool)>& callback) OVERRIDE;
  virtual void Embed(const String& url,
                     Id view_id,
                     ServiceProviderPtr service_provider,
                     const Callback<void(bool)>& callback) OVERRIDE;
  virtual void DispatchOnViewInputEvent(Id view_id, EventPtr event) OVERRIDE;

  // InterfaceImpl:
  virtual void OnConnectionEstablished() MOJO_OVERRIDE;

  // AccessPolicyDelegate:
  virtual const base::hash_set<Id>& GetRootsForAccessPolicy() const OVERRIDE;
  virtual bool IsViewKnownForAccessPolicy(
      const ServerView* view) const OVERRIDE;
  virtual bool IsViewRootOfAnotherConnectionForAccessPolicy(
      const ServerView* view) const OVERRIDE;

  ConnectionManager* connection_manager_;

  // Id of this connection as assigned by ConnectionManager.
  const ConnectionSpecificId id_;

  // URL this connection was created for.
  const std::string url_;

  // ID of the connection that created us. If 0 it indicates either we were
  // created by the root, or the connection that created us has been destroyed.
  ConnectionSpecificId creator_id_;

  // The URL of the app that embedded the app this connection was created for.
  const std::string creator_url_;

  scoped_ptr<AccessPolicy> access_policy_;

  // The views and views created by this connection. This connection owns these
  // objects.
  ViewMap view_map_;

  // The set of views that has been communicated to the client.
  ViewIdSet known_views_;

  // Set of root views from other connections. More specifically any time
  // Embed() is invoked the id of the view is added to this set for the child
  // connection. The connection Embed() was invoked on (the parent) doesn't
  // directly track which connections are attached to which of its views. That
  // information can be found by looking through the |roots_| of all
  // connections.
  ViewIdSet roots_;

  // See description above setter.
  bool delete_on_connection_error_;

  InterfaceRequest<ServiceProvider> service_provider_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerServiceImpl);
};

#if defined(OS_WIN)
#pragma warning(pop)
#endif

}  // namespace service
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_SERVICE_IMPL_H_
